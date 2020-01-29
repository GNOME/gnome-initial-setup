/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2020 Endless Mobile, Inc.
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
 *     Philip Withnall <withnall@endlessm.com>
 */

/* Parental controls page {{{1 */

#define PAGE_ID "parental-controls"

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "parental-controls-resources.h"
#include "gis-parental-controls-page.h"

struct _GisParentalControlsPage
{
  GisPage parent_instance;

  GtkWidget *user_controls;
};

G_DEFINE_TYPE (GisParentalControlsPage, gis_parental_controls_page, GIS_TYPE_PAGE)

static void
enterprise_apply_complete (GisPage  *dummy,
                           gboolean  valid,
                           gpointer  user_data)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (user_data);
  gis_driver_set_username (GIS_PAGE (page)->driver, NULL);
  gis_page_apply_complete (GIS_PAGE (page), valid);
}

static gboolean
page_validate (GisParentalControlsPage *page)
{
  GisParentalControlsPagePrivate *priv = gis_parental_controls_page_get_instance_private (page);

  switch (priv->mode) {
  case UM_LOCAL:
    return gis_parental_controls_page_local_validate (GIS_PARENTAL_CONTROLS_PAGE_LOCAL (priv->page_local));
  case UM_ENTERPRISE:
    return gis_parental_controls_page_enterprise_validate (GIS_PARENTAL_CONTROLS_PAGE_ENTERPRISE (priv->page_enterprise));
  default:
    g_assert_not_reached ();
  }
}

static void
update_page_validation (GisParentalControlsPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
on_validation_changed (gpointer        page_area,
                       GisParentalControlsPage *page)
{
  update_page_validation (page);
}

static void
set_mode (GisParentalControlsPage *page,
          UmParentalControlsMode   mode)
{
  GisParentalControlsPagePrivate *priv = gis_parental_controls_page_get_instance_private (page);

  if (priv->mode == mode)
    return;

  priv->mode = mode;
  gis_driver_set_parental_controls_mode (GIS_PAGE (page)->driver, mode);

  switch (mode)
    {
    case UM_LOCAL:
      gtk_stack_set_visible_child (GTK_STACK (priv->stack), priv->page_local);
      gis_parental_controls_page_local_shown (GIS_PARENTAL_CONTROLS_PAGE_LOCAL (priv->page_local));
      break;
    case UM_ENTERPRISE:
      gtk_stack_set_visible_child (GTK_STACK (priv->stack), priv->page_enterprise);
      gis_parental_controls_page_enterprise_shown (GIS_PARENTAL_CONTROLS_PAGE_ENTERPRISE (priv->page_enterprise));
      break;
    default:
      g_assert_not_reached ();
    }

  update_page_validation (page);
}

static void
toggle_mode (GtkToggleButton *button,
             gpointer         user_data)
{
  set_mode (GIS_PARENTAL_CONTROLS_PAGE (user_data),
            gtk_toggle_button_get_active (button) ? UM_ENTERPRISE : UM_LOCAL);
}

static gboolean
gis_parental_controls_page_apply (GisPage *gis_page,
                        GCancellable *cancellable)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);
  GisParentalControlsPagePrivate *priv = gis_parental_controls_page_get_instance_private (page);

  switch (priv->mode) {
  case UM_LOCAL:
    return gis_parental_controls_page_local_apply (GIS_PARENTAL_CONTROLS_PAGE_LOCAL (priv->page_local), gis_page);
  case UM_ENTERPRISE:
    return gis_parental_controls_page_enterprise_apply (GIS_PARENTAL_CONTROLS_PAGE_ENTERPRISE (priv->page_enterprise), cancellable,
                                              enterprise_apply_complete, page);
  default:
    g_assert_not_reached ();
    break;
  }
}

static void
gis_parental_controls_page_save_data (GisPage *gis_page)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);
  GisParentalControlsPagePrivate *priv = gis_parental_controls_page_get_instance_private (page);

  switch (priv->mode) {
  case UM_LOCAL:
    gis_parental_controls_page_local_create_user (GIS_PARENTAL_CONTROLS_PAGE_LOCAL (priv->page_local));
    break;
  case UM_ENTERPRISE:
    break;
  default:
    g_assert_not_reached ();
  }
}

static void
gis_parental_controls_page_shown (GisPage *gis_page)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);
  GisParentalControlsPagePrivate *priv = gis_parental_controls_page_get_instance_private (page);

  gis_parental_controls_page_local_shown (GIS_PARENTAL_CONTROLS_PAGE_LOCAL (priv->page_local));
}

static void
on_local_user_created (GtkWidget      *page_local,
                       ActUser        *user,
                       char           *password,
                       GisParentalControlsPage *page)
{
  const gchar *language;

  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
on_local_page_confirmed (GisParentalControlsPageLocal *local,
                         GisParentalControlsPage      *page)
{
  gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static void
on_local_user_cached (GtkWidget      *page_local,
                      ActUser        *user,
                      char           *password,
                      GisParentalControlsPage *page)
{
  const gchar *language;

  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
on_network_changed (GNetworkMonitor *monitor,
                    gboolean         available,
                    GisParentalControlsPage  *page)
{
  GisParentalControlsPagePrivate *priv = gis_parental_controls_page_get_instance_private (page);

  if (!available && priv->mode != UM_ENTERPRISE)
    gtk_stack_set_visible_child (GTK_STACK (priv->offline_stack), priv->offline_label);
  else
    gtk_stack_set_visible_child (GTK_STACK (priv->offline_stack), priv->page_toggle);
}

static void
gis_parental_controls_page_constructed (GObject *object)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (object);
  GisParentalControlsPagePrivate *priv = gis_parental_controls_page_get_instance_private (page);
  GNetworkMonitor *monitor;
  gboolean available;

  G_OBJECT_CLASS (gis_parental_controls_page_parent_class)->constructed (object);

  g_signal_connect (priv->page_local, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (priv->page_local, "user-created",
                    G_CALLBACK (on_local_user_created), page);
  g_signal_connect (priv->page_local, "confirm",
                    G_CALLBACK (on_local_page_confirmed), page);

  g_signal_connect (priv->page_enterprise, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (priv->page_enterprise, "user-cached",
                    G_CALLBACK (on_local_user_cached), page);

  update_page_validation (page);

  g_signal_connect (priv->page_toggle, "toggled", G_CALLBACK (toggle_mode), page);
  g_object_bind_property (page, "applying", priv->page_toggle, "sensitive", G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property (priv->page_enterprise, "visible", priv->offline_stack, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* force a refresh by setting to an invalid value */
  priv->mode = NUM_MODES;
  set_mode (page, UM_LOCAL);

  monitor = g_network_monitor_get_default ();
  available = g_network_monitor_get_network_available (monitor);
  on_network_changed (monitor, available, page);
  g_signal_connect_object (monitor, "network-changed", G_CALLBACK (on_network_changed), page, 0);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_parental_controls_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("About You"));
}

static void
gis_parental_controls_page_class_init (GisParentalControlsPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-parental_controls-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GisParentalControlsPage, user_controls);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_parental_controls_page_locale_changed;
  page_class->apply = gis_parental_controls_page_apply;
  page_class->save_data = gis_parental_controls_page_save_data;
  page_class->shown = gis_parental_controls_page_shown;
  object_class->constructed = gis_parental_controls_page_constructed;
}

static void
gis_parental_controls_page_init (GisParentalControlsPage *page)
{
  g_resources_register (parental_controls_get_resource ());
  g_type_ensure (GIS_TYPE_PARENTAL_CONTROLS_PAGE_LOCAL);
  g_type_ensure (GIS_TYPE_PARENTAL_CONTROLS_PAGE_ENTERPRISE);

  gtk_widget_init_template (GTK_WIDGET (page));
}
