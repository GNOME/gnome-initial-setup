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
#include <libmalcontent-ui/malcontent-ui.h>

#include "parental-controls-resources.h"
#include "gis-page-header.h"
#include "gis-parental-controls-page.h"

struct _GisParentalControlsPage
{
  GisPage parent_instance;

  GtkWidget *header;
  GtkWidget *avatar;
  GtkWidget *user_controls;
};

G_DEFINE_TYPE (GisParentalControlsPage, gis_parental_controls_page, GIS_TYPE_PAGE)

static gboolean
gis_parental_controls_page_save_data (GisPage  *gis_page,
                                      GError  **error)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);
  g_autoptr(GDBusConnection) system_bus = NULL;
  g_autoptr(MctManager) manager = NULL;
  g_auto(MctAppFilterBuilder) builder = MCT_APP_FILTER_BUILDER_INIT ();
  g_autoptr(MctAppFilter) app_filter = NULL;
  ActUser *main_user;

  /* The parent and child users are created by the #GisAccountPage earlier in
   * the save_data() process. We now need to set the parental controls on the
   * child user. The earlier step in the process must have succeeded. */
  gis_driver_get_user_permissions (gis_page->driver, &main_user, NULL);
  g_return_val_if_fail (main_user != NULL, FALSE);

  mct_user_controls_build_app_filter (MCT_USER_CONTROLS (page->user_controls), &builder);
  app_filter = mct_app_filter_builder_end (&builder);

  /* FIXME: should become asynchronous */
  system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
  if (system_bus == NULL)
    return FALSE;

  manager = mct_manager_new (system_bus);
  if (!mct_manager_set_app_filter (manager,
                                   act_user_get_uid (main_user),
                                   app_filter,
                                   MCT_MANAGER_SET_VALUE_FLAGS_NONE,
                                   NULL,
                                   error))
    return FALSE;

  return TRUE;
}

static void
gis_parental_controls_page_shown (GisPage *gis_page)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);

  gtk_widget_grab_focus (page->user_controls);
}

static void
update_header (GisParentalControlsPage *page)
{
  g_autofree gchar *title = NULL;
  const gchar *subtitle;
  GdkPaintable *paintable;
  gboolean small_screen = FALSE;

  g_object_get (G_OBJECT (page), "small-screen", &small_screen, NULL);

  /* Translators: The placeholder is the user’s full name. */
  title = g_strdup_printf (_("Parental Controls for %s"),
                           gis_driver_get_full_name (GIS_PAGE (page)->driver));
  subtitle = _("Set restrictions on what this user can run or install.");

  g_object_set (G_OBJECT (page->header),
                "title", title,
                "subtitle", subtitle,
                NULL);

  paintable = gis_driver_get_has_default_avatar (GIS_PAGE (page)->driver) ? NULL :
              GDK_PAINTABLE (gis_driver_get_avatar (GIS_PAGE (page)->driver));

  g_object_set (G_OBJECT (page->avatar),
                "visible", !small_screen,
                "text", gis_driver_get_full_name (GIS_PAGE (page)->driver),
                "custom-image", paintable, NULL);
}

static void
update_user_controls (GisParentalControlsPage *page)
{
  mct_user_controls_set_user_locale (MCT_USER_CONTROLS (page->user_controls),
                                     gis_driver_get_user_language (GIS_PAGE (page)->driver));
  mct_user_controls_set_user_display_name (MCT_USER_CONTROLS (page->user_controls),
                                           gis_driver_get_full_name (GIS_PAGE (page)->driver));
}

static void
user_details_changed_cb (GObject    *obj,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (user_data);

  update_user_controls (page);
  update_header (page);
}

static void
gis_parental_controls_page_constructed (GObject *object)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (object);
  g_autoptr(GPermission) permission = NULL;
  g_auto(MctAppFilterBuilder) builder = MCT_APP_FILTER_BUILDER_INIT ();
  g_autoptr(MctAppFilter) app_filter = NULL;

  G_OBJECT_CLASS (gis_parental_controls_page_parent_class)->constructed (object);

  /* No validation needed. */
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  /* Set up the user controls. We can’t set #MctUserControls:user because
   * there’s no way to represent a not-yet-created user using an #ActUser. */
  mct_user_controls_set_user_account_type (MCT_USER_CONTROLS (page->user_controls),
                                           ACT_USER_ACCOUNT_TYPE_STANDARD);
  update_user_controls (page);

  app_filter = mct_app_filter_builder_end (&builder);
  mct_user_controls_set_app_filter (MCT_USER_CONTROLS (page->user_controls), app_filter);

  /* The gnome-initial-setup user should always be allowed to set parental
   * controls. */
  permission = g_simple_permission_new (TRUE);
  mct_user_controls_set_permission (MCT_USER_CONTROLS (page->user_controls), permission);

  /* Connect to signals. */
  g_signal_connect (GIS_PAGE (page)->driver, "notify::full-name",
                    G_CALLBACK (user_details_changed_cb), page);
  g_signal_connect (GIS_PAGE (page)->driver, "notify::avatar",
                    G_CALLBACK (user_details_changed_cb), page);
  g_signal_connect (GIS_PAGE (page)->driver, "notify::has-default-avatar",
                    G_CALLBACK (user_details_changed_cb), page);
  g_signal_connect (GIS_PAGE (page)->driver, "notify::user-locale",
                    G_CALLBACK (user_details_changed_cb), page);
  g_signal_connect (GIS_PAGE (page)->driver, "notify::user-display-name",
                    G_CALLBACK (user_details_changed_cb), page);
  g_signal_connect (page, "notify::small-screen",
                    G_CALLBACK (update_header), NULL);

  update_header (page);

  gtk_widget_set_visible (GTK_WIDGET (page), TRUE);
}

static void
gis_parental_controls_page_dispose (GObject *object)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (object);

  if (GIS_PAGE (object)->driver != NULL)
    {
      g_signal_handlers_disconnect_by_func (GIS_PAGE (page)->driver,
                                            user_details_changed_cb, page);
    }

  G_OBJECT_CLASS (gis_parental_controls_page_parent_class)->dispose (object);
}

static void
gis_parental_controls_page_locale_changed (GisPage *gis_page)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);

  gis_page_set_title (gis_page, _("Parental Controls"));
  update_header (page);
}

static void
gis_parental_controls_page_class_init (GisParentalControlsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);

  object_class->constructed = gis_parental_controls_page_constructed;
  object_class->dispose = gis_parental_controls_page_dispose;

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_parental_controls_page_locale_changed;
  page_class->save_data = gis_parental_controls_page_save_data;
  page_class->shown = gis_parental_controls_page_shown;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-parental-controls-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GisParentalControlsPage, header);
  gtk_widget_class_bind_template_child (widget_class, GisParentalControlsPage, user_controls);
  gtk_widget_class_bind_template_child (widget_class, GisParentalControlsPage, avatar);
}

static void
gis_parental_controls_page_init (GisParentalControlsPage *page)
{

  /* Ensure types exist for widgets in the UI file. */
  g_type_ensure (GIS_TYPE_PAGE_HEADER);
  g_type_ensure (MCT_TYPE_USER_CONTROLS);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_parental_controls_page (GisDriver *driver)
{
  /* Skip parental controls if they’re not enabled. */
  if (!gis_driver_get_parental_controls_enabled (driver))
    return NULL;

  return g_object_new (GIS_TYPE_PARENTAL_CONTROLS_PAGE,
                       "driver", driver,
                       NULL);
}
