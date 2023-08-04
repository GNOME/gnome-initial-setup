/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Account page {{{1 */

#define PAGE_ID "account"

#include "config.h"
#include "account-resources.h"
#include "gis-account-page.h"
#include "gis-account-page-local.h"
#include "gis-account-page-enterprise.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

struct _GisAccountPage
{
  GisPage    parent;

  GtkWidget *page_local;
  GtkWidget *page_enterprise;
  GtkWidget *stack;

  GtkWidget *page_toggle;
  GtkWidget *offline_label;
  GtkWidget *offline_stack;

  UmAccountMode mode;
};

G_DEFINE_TYPE (GisAccountPage, gis_account_page, GIS_TYPE_PAGE);

static void
enterprise_apply_complete (GisPage  *dummy,
                           gboolean  valid,
                           gpointer  user_data)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (user_data);
  gis_driver_set_username (GIS_PAGE (page)->driver, NULL);
  gis_page_apply_complete (GIS_PAGE (page), valid);
}

static gboolean
page_validate (GisAccountPage *page)
{
  switch (page->mode) {
  case UM_LOCAL:
    return gis_account_page_local_validate (GIS_ACCOUNT_PAGE_LOCAL (page->page_local));
  case UM_ENTERPRISE:
    return gis_account_page_enterprise_validate (GIS_ACCOUNT_PAGE_ENTERPRISE (page->page_enterprise));
  default:
    g_assert_not_reached ();
  }
}

static void
update_page_validation (GisAccountPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
on_validation_changed (gpointer        page_area,
                       GisAccountPage *page)
{
  update_page_validation (page);
}

static void
set_mode (GisAccountPage *page,
          UmAccountMode   mode)
{
  if (page->mode == mode)
    return;

  page->mode = mode;
  gis_driver_set_account_mode (GIS_PAGE (page)->driver, mode);

  switch (mode)
    {
    case UM_LOCAL:
      gtk_stack_set_visible_child (GTK_STACK (page->stack), page->page_local);
      gis_account_page_local_shown (GIS_ACCOUNT_PAGE_LOCAL (page->page_local));
      break;
    case UM_ENTERPRISE:
      gtk_stack_set_visible_child (GTK_STACK (page->stack), page->page_enterprise);
      gis_account_page_enterprise_shown (GIS_ACCOUNT_PAGE_ENTERPRISE (page->page_enterprise));
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
  set_mode (GIS_ACCOUNT_PAGE (user_data),
            gtk_toggle_button_get_active (button) ? UM_ENTERPRISE : UM_LOCAL);
}

static gboolean
gis_account_page_apply (GisPage *gis_page,
                        GCancellable *cancellable)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);

  switch (page->mode) {
  case UM_LOCAL:
    return gis_account_page_local_apply (GIS_ACCOUNT_PAGE_LOCAL (page->page_local), gis_page);
  case UM_ENTERPRISE:
    return gis_account_page_enterprise_apply (GIS_ACCOUNT_PAGE_ENTERPRISE (page->page_enterprise), cancellable,
                                              enterprise_apply_complete, page);
  default:
    g_assert_not_reached ();
    break;
  }
}

static gboolean
gis_account_page_save_data (GisPage  *gis_page,
                            GError  **error)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);

  switch (page->mode) {
  case UM_LOCAL:
    return gis_account_page_local_create_user (GIS_ACCOUNT_PAGE_LOCAL (page->page_local), gis_page, error);
  case UM_ENTERPRISE:
    /* Nothing to do. */
    return TRUE;
  default:
    g_assert_not_reached ();
    return FALSE;
  }
}

static void
gis_account_page_shown (GisPage *gis_page)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);

  gis_account_page_local_shown (GIS_ACCOUNT_PAGE_LOCAL (page->page_local));
}

static void
on_local_main_user_created (GtkWidget      *page_local,
                            ActUser        *user,
                            const gchar    *password,
                            GisAccountPage *page)
{
  const gchar *language;

  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
on_local_parent_user_created (GtkWidget      *page_local,
                              ActUser        *user,
                              const gchar    *password,
                              GisAccountPage *page)
{
  const gchar *language;

  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (user, language);

  gis_driver_set_parent_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
on_local_page_confirmed (GisAccountPageLocal *local,
                         GisAccountPage      *page)
{
  gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static void
on_local_user_cached (GtkWidget      *page_local,
                      ActUser        *user,
                      char           *password,
                      GisAccountPage *page)
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
                    GisAccountPage  *page)
{
  if (!available && page->mode != UM_ENTERPRISE)
    gtk_stack_set_visible_child (GTK_STACK (page->offline_stack), page->offline_label);
  else
    gtk_stack_set_visible_child (GTK_STACK (page->offline_stack), page->page_toggle);
}

static void
gis_account_page_constructed (GObject *object)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (object);
  GNetworkMonitor *monitor;
  gboolean available;

  G_OBJECT_CLASS (gis_account_page_parent_class)->constructed (object);

  g_signal_connect (page->page_local, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (page->page_local, "main-user-created",
                    G_CALLBACK (on_local_main_user_created), page);
  g_signal_connect (page->page_local, "parent-user-created",
                    G_CALLBACK (on_local_parent_user_created), page);
  g_signal_connect (page->page_local, "confirm",
                    G_CALLBACK (on_local_page_confirmed), page);

  g_signal_connect (page->page_enterprise, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (page->page_enterprise, "user-cached",
                    G_CALLBACK (on_local_user_cached), page);

  update_page_validation (page);

  g_signal_connect (page->page_toggle, "toggled", G_CALLBACK (toggle_mode), page);
  g_object_bind_property (page, "applying", page->page_toggle, "sensitive", G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property (page->page_enterprise, "visible", page->offline_stack, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* force a refresh by setting to an invalid value */
  page->mode = NUM_MODES;
  set_mode (page, UM_LOCAL);

  monitor = g_network_monitor_get_default ();
  available = g_network_monitor_get_network_available (monitor);
  on_network_changed (monitor, available, page);
  g_signal_connect_object (monitor, "network-changed", G_CALLBACK (on_network_changed), page, 0);

  gtk_widget_set_visible (GTK_WIDGET (page), TRUE);
}

static void
gis_account_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("About You"));
}

static void
gis_account_page_class_init (GisAccountPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPage, page_local);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPage, page_enterprise);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPage, stack);

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPage, page_toggle);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPage, offline_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPage, offline_stack);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_account_page_locale_changed;
  page_class->apply = gis_account_page_apply;
  page_class->save_data = gis_account_page_save_data;
  page_class->shown = gis_account_page_shown;
  object_class->constructed = gis_account_page_constructed;

  gis_add_style_from_resource ("/org/gnome/initial-setup/gis-account-page.css");
}

static void
gis_account_page_init (GisAccountPage *page)
{
  g_type_ensure (GIS_TYPE_ACCOUNT_PAGE_LOCAL);
  g_type_ensure (GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE);

  gtk_widget_init_template (GTK_WIDGET (page));
}
