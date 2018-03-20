/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 * 	Matthias Clasen <mclasen@redhat.com>
 *
 * Based on gnome-control-center cc-region-panel.c
 */

/* Region page {{{1 */

#define PAGE_ID "region"

#include "config.h"
#include "region-resources.h"
#include "cc-region-chooser.h"
#include "cc-common-language.h"
#include "gis-region-page.h"

#include <act/act-user-manager.h>
#include <polkit/polkit.h>
#include <locale.h>
#include <gtk/gtk.h>

struct _GisRegionPagePrivate
{
  GtkWidget *region_chooser;

  GDBusProxy *localed;
  GPermission *permission;
  const gchar *new_locale_id;
  gboolean updating;

  GCancellable *cancellable;
};
typedef struct _GisRegionPagePrivate GisRegionPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisRegionPage, gis_region_page, GIS_TYPE_PAGE);

static void
set_localed_locale (GisRegionPage *self)
{
  GisRegionPagePrivate *priv = gis_region_page_get_instance_private (self);
  GVariantBuilder *b;
  gchar *s;

  b = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  s = g_strconcat ("LANG=", priv->new_locale_id, NULL);
  g_variant_builder_add (b, "s", s);
  g_free (s);

  g_dbus_proxy_call (priv->localed,
                     "SetLocale",
                     g_variant_new ("(asb)", b, TRUE),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
  g_variant_builder_unref (b);
}

static void
change_locale_permission_acquired (GObject      *source,
                                   GAsyncResult *res,
                                   gpointer      data)
{
  GisRegionPage *page = GIS_REGION_PAGE (data);
  GisRegionPagePrivate *priv = gis_region_page_get_instance_private (page);
  GError *error = NULL;
  gboolean allowed;

  allowed = g_permission_acquire_finish (priv->permission, res, &error);
  if (error) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to acquire permission: %s", error->message);
      g_error_free (error);
      return;
  }

  if (allowed)
    set_localed_locale (page);
}

static void
user_loaded (GObject    *object,
             GParamSpec *pspec,
             gpointer    user_data)
{
  gchar *new_locale_id = user_data;

  act_user_set_language (ACT_USER (object), new_locale_id);

  g_free (new_locale_id);
}

static void
region_changed (CcRegionChooser  *chooser,
                  GParamSpec         *pspec,
                  GisRegionPage    *page)
{
  GisRegionPagePrivate *priv = gis_region_page_get_instance_private (page);
  ActUser *user;
  GisDriver *driver;

  if (priv->updating)
    return;

  priv->new_locale_id = cc_region_chooser_get_locale (chooser);
  driver = GIS_PAGE (page)->driver;

  setlocale (LC_MESSAGES, priv->new_locale_id);
  gis_driver_locale_changed (driver);

  if (gis_driver_get_mode (driver) == GIS_DRIVER_MODE_NEW_USER) {
      if (g_permission_get_allowed (priv->permission)) {
          set_localed_locale (page);
      }
      else if (g_permission_get_can_acquire (priv->permission)) {
          g_permission_acquire_async (priv->permission,
                                      NULL,
                                      change_locale_permission_acquired,
                                      page);
      }
  }
  user = act_user_manager_get_user (act_user_manager_get_default (),
                                    g_get_user_name ());
  if (act_user_is_loaded (user))
    act_user_set_language (user, priv->new_locale_id);
  else
    g_signal_connect (user,
                      "notify::is-loaded",
                      G_CALLBACK (user_loaded),
                      g_strdup (priv->new_locale_id));

  gis_driver_set_user_language (driver, priv->new_locale_id);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
  GisRegionPage *self = data;
  GisRegionPagePrivate *priv = gis_region_page_get_instance_private (self);
  GDBusProxy *proxy;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_finish (res, &error);

  if (!proxy) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to contact localed: %s", error->message);
      g_error_free (error);
      return;
  }

  priv->localed = proxy;
}

static void
region_confirmed (CcRegionChooser *chooser,
                  GisRegionPage   *page)
{
  gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static void
gis_region_page_constructed (GObject *object)
{
  GisRegionPage *page = GIS_REGION_PAGE (object);
  GisRegionPagePrivate *priv = gis_region_page_get_instance_private (page);
  GDBusConnection *bus;

  g_type_ensure (CC_TYPE_REGION_CHOOSER);

  G_OBJECT_CLASS (gis_region_page_parent_class)->constructed (object);

  g_signal_connect (priv->region_chooser, "notify::locale",
                    G_CALLBACK (region_changed), page);
  g_signal_connect (priv->region_chooser, "confirm",
                    G_CALLBACK (region_confirmed), page);

  /* If we're in new user mode then we're manipulating system settings */
  if (gis_driver_get_mode (GIS_PAGE (page)->driver) == GIS_DRIVER_MODE_NEW_USER)
    {
      priv->permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-locale", NULL, NULL, NULL);

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
      g_dbus_proxy_new (bus,
                        G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                        NULL,
                        "org.freedesktop.locale1",
                        "/org/freedesktop/locale1",
                        "org.freedesktop.locale1",
                        priv->cancellable,
                        (GAsyncReadyCallback) localed_proxy_ready,
                        object);
      g_object_unref (bus);
  }

  gis_page_set_complete (GIS_PAGE (page), TRUE);
  if (cc_region_chooser_get_n_regions (CC_REGION_CHOOSER (priv->region_chooser)) > 1)
    gtk_widget_show (GTK_WIDGET (page));
  else
    gtk_widget_hide (GTK_WIDGET (page));
}

static void
gis_region_page_locale_changed (GisPage *page)
{
  GisRegionPagePrivate *priv = gis_region_page_get_instance_private (GIS_REGION_PAGE (page));
  char *locale;

  gis_page_set_title (page, _("Region"));

  locale = g_strdup (setlocale (LC_MESSAGES, NULL));

  priv->updating = TRUE;
  cc_region_chooser_set_locale (CC_REGION_CHOOSER (priv->region_chooser), locale);
  priv->updating = FALSE;
  g_free (locale);

  if (cc_region_chooser_get_n_regions (CC_REGION_CHOOSER (priv->region_chooser)) > 1)
    gtk_widget_show (GTK_WIDGET (page));
  else
    gtk_widget_hide (GTK_WIDGET (page));
}

static void
gis_region_page_dispose (GObject *object)
{
  GisRegionPage *page = GIS_REGION_PAGE (object);
  GisRegionPagePrivate *priv = gis_region_page_get_instance_private (page);

  g_clear_object (&priv->permission);
  g_clear_object (&priv->localed);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (gis_region_page_parent_class)->dispose (object);
}

static void
gis_region_page_class_init (GisRegionPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-region-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisRegionPage, region_chooser);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_region_page_locale_changed;
  object_class->constructed = gis_region_page_constructed;
  object_class->dispose = gis_region_page_dispose;
}

static void
gis_region_page_init (GisRegionPage *page)
{
  g_resources_register (region_get_resource ());
  g_type_ensure (CC_TYPE_REGION_CHOOSER);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_region_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_REGION_PAGE,
                       "driver", driver,
                       NULL);
}
