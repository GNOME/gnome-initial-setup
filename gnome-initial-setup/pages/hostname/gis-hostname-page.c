/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2018 Matthias Klumpp <matthias.klumpp@puri.sm>
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
 */

/* Hostname page {{{1 */

#define PAGE_ID "hostname"

#include "config.h"
#include "hostname-resources.h"
#include "gis-hostname-page.h"

#include "hostname-utils.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#define VALIDATION_TIMEOUT 600

struct _GisHostnamePagePrivate
{
  GtkWidget *hostname_entry;
  GtkWidget *hostname_explanation;

  gboolean valid_hostname;
  guint timeout_id;
};
typedef struct _GisHostnamePagePrivate GisHostnamePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisHostnamePage, gis_hostname_page, GIS_TYPE_PAGE);

static gboolean
page_validate (GisHostnamePage *page)
{
  GisHostnamePagePrivate *priv = gis_hostname_page_get_instance_private (page);

  return priv->valid_hostname;
}

static void
update_page_validation (GisHostnamePage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
gis_hostname_page_save_data (GisPage *gis_page)
{
  GisHostnamePage *page = GIS_HOSTNAME_PAGE (gis_page);
  GisHostnamePagePrivate *priv = gis_hostname_page_get_instance_private (page);
  g_autoptr(GDBusConnection) connection = NULL;
  const gchar *hostname;
  g_autoptr(GError) error = NULL;

  hostname = gtk_entry_get_text (GTK_ENTRY (priv->hostname_entry));
  g_debug ("Setting StaticHostname to '%s'", hostname);

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (error != NULL) {
    g_warning ("Could not get DBus connection: %s", error->message);
    g_error_free (error);
    return;
  }

  g_dbus_connection_call (connection, "org.freedesktop.hostname1",
                          "/org/freedesktop/hostname1", "org.freedesktop.hostname1",
                          "SetStaticHostname",
                          g_variant_new ("(sb)", hostname, TRUE),
                          G_VARIANT_TYPE ("()"),
                          G_DBUS_CALL_FLAGS_NONE,
                          G_MAXINT, NULL, NULL, NULL);
}

static void
gis_hostname_page_shown (GisPage *gis_page)
{
  GisHostnamePage *page = GIS_HOSTNAME_PAGE (gis_page);
  GisHostnamePagePrivate *priv = gis_hostname_page_get_instance_private (page);

  gtk_widget_grab_focus (priv->hostname_entry);
}

static gboolean
validate (GisHostnamePage *page)
{
  GisHostnamePagePrivate *priv = gis_hostname_page_get_instance_private (page);
  const gchar *hostname;

  if (priv->timeout_id != 0) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  hostname = gtk_entry_get_text (GTK_ENTRY (priv->hostname_entry));

  priv->valid_hostname = hostname_is_valid (hostname, FALSE);
  if (!priv->valid_hostname) {
    gtk_label_set_label (GTK_LABEL (priv->hostname_explanation),
                         _("The name must start with a letter, and can only contain letters from a-z and number from 0-9, hyphens, underscores and dots."));
  }

  update_page_validation (page);

  return FALSE;
}

static gboolean
on_focusout (GisHostnamePage *page)
{
  validate (page);

  return FALSE;
}

static void
hostname_changed (GtkWidget      *w,
                  GParamSpec     *pspec,
                  GisHostnamePage *page)
{
  GisHostnamePagePrivate *priv = gis_hostname_page_get_instance_private (page);

  priv->valid_hostname = FALSE;
  update_page_validation (page);

  if (priv->timeout_id != 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate, page);
}

static void
confirm (GisHostnamePage *page)
{
  if (page_validate (page))
    gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static void
gis_hostname_page_constructed (GObject *object)
{
  GisHostnamePage *page = GIS_HOSTNAME_PAGE (object);
  GisHostnamePagePrivate *priv = gis_hostname_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_hostname_page_parent_class)->constructed (object);

  g_signal_connect (priv->hostname_entry, "notify::text",
                    G_CALLBACK (hostname_changed), page);
  g_signal_connect_swapped (priv->hostname_entry, "focus-out-event",
                            G_CALLBACK (on_focusout), page);
  g_signal_connect_swapped (priv->hostname_entry, "activate",
                            G_CALLBACK (confirm), page);

  validate (page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_hostname_page_dispose (GObject *object)
{
  G_OBJECT_CLASS (gis_hostname_page_parent_class)->dispose (object);
}

static void
gis_hostname_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Device Name"));
}

static void
gis_hostname_page_class_init (GisHostnamePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-hostname-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisHostnamePage, hostname_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisHostnamePage, hostname_explanation);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_hostname_page_locale_changed;
  page_class->save_data = gis_hostname_page_save_data;
  page_class->shown = gis_hostname_page_shown;

  object_class->constructed = gis_hostname_page_constructed;
  object_class->dispose = gis_hostname_page_dispose;
}

static void
gis_hostname_page_init (GisHostnamePage *page)
{
  g_resources_register (hostname_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage*
gis_prepare_hostname_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_HOSTNAME_PAGE,
                       "driver", driver,
                       NULL);
}
