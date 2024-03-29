/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2015 Red Hat
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
 */

/* Privacy page {{{1 */

#define PAGE_ID "privacy"

#include "config.h"
#include "privacy-resources.h"
#include "gis-privacy-page.h"

#ifdef HAVE_WEBKITGTK
#include <webkit/webkit.h>
#endif

#include <locale.h>
#include <gtk/gtk.h>

#include "gis-page-header.h"

struct _GisPrivacyPage
{
  GisPage parent;

  GtkWidget *location_switch;
  GtkWidget *location_privacy_label;
  GtkWidget *reporting_group;
  GtkWidget *reporting_label;
  GtkWidget *reporting_switch;
  GSettings *location_settings;
  GSettings *privacy_settings;
  guint abrt_watch_id;
};

G_DEFINE_TYPE (GisPrivacyPage, gis_privacy_page, GIS_TYPE_PAGE);

#ifdef HAVE_WEBKITGTK
static void
notify_progress_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  GtkWidget *progress_bar = user_data;
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (object);
  gdouble progress;

  progress = webkit_web_view_get_estimated_load_progress (web_view);

  gtk_widget_set_visible (progress_bar, progress != 1.0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), progress);
}

static gboolean
activate_link (GtkLabel       *label,
               const gchar    *uri,
               GisPrivacyPage *page)
{
  GtkWidget *headerbar;
  GtkWidget *dialog;
  GtkWidget *overlay;
  GtkWidget *view;
  GtkWidget *progress_bar;

  headerbar = gtk_header_bar_new ();
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (headerbar), TRUE);

  dialog = g_object_new (GTK_TYPE_WINDOW,
                         "destroy-with-parent", TRUE,
                         "transient-for", gtk_widget_get_root (GTK_WIDGET (page)),
                         "titlebar", headerbar,
                         "title", _("Privacy Policy"),
                         "modal", TRUE,
                         "default-width", 800,
                         "default-height", 600,
                         NULL);

  overlay = gtk_overlay_new ();
  gtk_window_set_child (GTK_WINDOW (dialog), overlay);

  progress_bar = gtk_progress_bar_new ();
  gtk_widget_add_css_class (progress_bar, "osd");
  gtk_widget_set_halign (progress_bar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (progress_bar, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), progress_bar);

  view = webkit_web_view_new ();
  gtk_widget_set_hexpand (view, TRUE);
  gtk_widget_set_vexpand (view, TRUE);
  g_signal_connect (view, "notify::estimated-load-progress",
                    G_CALLBACK (notify_progress_cb), progress_bar);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), view);

  gtk_window_present (GTK_WINDOW (dialog));

  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), uri);

  return TRUE;
}
#else
static gboolean
activate_link (GtkLabel       *label,
               const gchar    *uri,
               GisPrivacyPage *page)
{
  /* Fall back to default handler */
  return FALSE;
}
#endif

static gboolean
update_os_data (GisPrivacyPage *page)
{
  g_autofree char *name = g_get_os_info (G_OS_INFO_KEY_NAME);
  g_autofree char *subtitle = NULL;
#ifdef HAVE_WEBKITGTK
  g_autofree char *privacy_policy = g_get_os_info (G_OS_INFO_KEY_PRIVACY_POLICY_URL);
#endif

  if (!name)
    return FALSE;

#ifdef HAVE_WEBKITGTK
  if (privacy_policy)
    {
      /* Translators: the first parameter here is the name of a distribution,
       * like "Fedora" or "Ubuntu".
       */
      subtitle = g_strdup_printf (_("Sends technical reports that do not contain personal information. "
                                    "Data is collected by %1$s (<a href='%2$s'>privacy policy</a>)."),
                                    name, privacy_policy);
      gtk_label_set_markup (GTK_LABEL (page->reporting_label), subtitle);

      return TRUE;
    }
#endif

  /* Translators: the parameter here is the name of a distribution,
   * like "Fedora" or "Ubuntu".
   */
  subtitle = g_strdup_printf (_("Sends technical reports that do not contain personal information. "
                                "Data is collected by %s."), name);
  gtk_label_set_label (GTK_LABEL (page->reporting_label), subtitle);
  return TRUE;
}

static void
abrt_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  GisPrivacyPage *page = user_data;

  gtk_widget_set_visible (page->reporting_group, TRUE);
}

static void
abrt_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  GisPrivacyPage *page = user_data;

  gtk_widget_set_visible (page->reporting_group, FALSE);
}

static void
gis_privacy_page_constructed (GObject *object)
{
  GisPrivacyPage *page = GIS_PRIVACY_PAGE (object);

  G_OBJECT_CLASS (gis_privacy_page_parent_class)->constructed (object);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  page->location_settings = g_settings_new ("org.gnome.system.location");
  page->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  gtk_switch_set_active (GTK_SWITCH (page->location_switch), TRUE);
  gtk_switch_set_active (GTK_SWITCH (page->reporting_switch), TRUE);

#ifdef HAVE_WEBKITGTK
  gtk_label_set_markup (GTK_LABEL (page->location_privacy_label),
                        _("Allows apps to determine your geographical location. Uses the Mozilla Location Service (<a href='https://location.services.mozilla.com/privacy'>privacy policy</a>)."));
#else
  gtk_label_set_label (GTK_LABEL (page->location_privacy_label),
                       _("Allows apps to determine your geographical location. Uses the Mozilla Location Service."));
#endif

  if (update_os_data (page))
    {
      page->abrt_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                              "org.freedesktop.problems.daemon",
                                              G_BUS_NAME_WATCHER_FLAGS_NONE,
                                              abrt_appeared_cb,
                                              abrt_vanished_cb,
                                              page,
                                              NULL);
    }
}

static void
gis_privacy_page_dispose (GObject *object)
{
  GisPrivacyPage *page = GIS_PRIVACY_PAGE (object);

  g_clear_object (&page->location_settings);
  g_clear_object (&page->privacy_settings);

  g_clear_handle_id (&page->abrt_watch_id, g_bus_unwatch_name);

  G_OBJECT_CLASS (gis_privacy_page_parent_class)->dispose (object);
}

static gboolean
gis_privacy_page_apply (GisPage *gis_page,
                        GCancellable *cancellable)
{
  GisPrivacyPage *page = GIS_PRIVACY_PAGE (gis_page);
  gboolean active;

  active = gtk_widget_is_visible (page->location_switch) && gtk_switch_get_active (GTK_SWITCH (page->location_switch));
  g_settings_set_boolean (page->location_settings, "enabled", active);

  active = gtk_widget_is_visible (page->reporting_switch) && gtk_switch_get_active (GTK_SWITCH (page->reporting_switch));
  g_settings_set_boolean (page->privacy_settings, "report-technical-problems", active);

  return FALSE;
}

static void
gis_privacy_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Privacy"));
}

static void
gis_privacy_page_class_init (GisPrivacyPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-privacy-page.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisPrivacyPage, location_switch);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisPrivacyPage, location_privacy_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisPrivacyPage, reporting_group);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisPrivacyPage, reporting_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisPrivacyPage, reporting_switch);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), activate_link);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_privacy_page_locale_changed;
  page_class->apply = gis_privacy_page_apply;
  object_class->constructed = gis_privacy_page_constructed;
  object_class->dispose = gis_privacy_page_dispose;
}

static void
gis_privacy_page_init (GisPrivacyPage *page)
{
  g_type_ensure (GIS_TYPE_PAGE_HEADER);
  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_privacy_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_PRIVACY_PAGE,
                       "driver", driver,
                       NULL);
}
