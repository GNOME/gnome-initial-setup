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

#include <webkit2/webkit2.h>

#include <locale.h>
#include <gtk/gtk.h>

struct _GisPrivacyPagePrivate
{
  GtkWidget *location_switch;
  GtkWidget *reporting_row;
  GtkWidget *reporting_switch;
  GtkWidget *reporting_label;
  GtkWidget *mozilla_privacy_policy_label;
  GtkWidget *distro_privacy_policy_label;
  GSettings *location_settings;
  GSettings *privacy_settings;
  guint abrt_watch_id;
};
typedef struct _GisPrivacyPagePrivate GisPrivacyPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisPrivacyPage, gis_privacy_page, GIS_TYPE_PAGE);

static char *
get_item (const char *buffer, const char *name)
{
  char *label, *start, *end, *result;
  char end_char;

  result = NULL;
  start = NULL;
  end = NULL;
  label = g_strconcat (name, "=", NULL);
  if ((start = strstr (buffer, label)) != NULL)
    {
      start += strlen (label);
      end_char = '\n';
      if (*start == '"')
        {
          start++;
          end_char = '"';
        }

      end = strchr (start, end_char);
    }

    if (start != NULL && end != NULL)
      {
        result = g_strndup (start, end - start);
      }

  g_free (label);

  return result;
}

static void
update_os_data (GisPrivacyPage *page)
{
  GisPrivacyPagePrivate *priv = gis_privacy_page_get_instance_private (page);
  char *buffer;
  char *name;
  char *privacy_policy;
  char *text;

  name = NULL;
  privacy_policy = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
      name = get_item (buffer, "NAME");
      privacy_policy = get_item (buffer, "PRIVACY_POLICY_URL");
      g_free (buffer);
    }

  if (!name)
    name = g_strdup ("GNOME");

  /* Translators: the parameter here is the name of a distribution,
   * like "Fedora" or "Ubuntu". It falls back to "GNOME" if we can't
   * detect any distribution.
   */
  text = g_strdup_printf (_("Sending reports of technical problems helps us to improve %s. Reports are sent anonymously and are scrubbed of personal data."), name);
  gtk_label_set_label (GTK_LABEL (priv->reporting_label), text);
  g_free (text);

  if (privacy_policy)
    {
      /* Translators: the parameter here is the name of a distribution,
       * like "Fedora" or "Ubuntu". It falls back to "GNOME" if we can't
       * detect any distribution.
       */
      char *distro_label = g_strdup_printf (_("Problem data will be collected by %s:"), name);
      text = g_strdup_printf ("%s <a href='%s'>%s</a>", distro_label, privacy_policy, _("Privacy Policy"));
      gtk_label_set_markup (GTK_LABEL (priv->distro_privacy_policy_label), text);
      g_free (distro_label);
      g_free (text);
    }
  else
    {
      gtk_widget_hide (priv->distro_privacy_policy_label);
    }

  g_free (name);
  g_free (privacy_policy);
}

static void
abrt_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  GisPrivacyPage *page = user_data;
  GisPrivacyPagePrivate *priv = gis_privacy_page_get_instance_private (page);

  gtk_widget_show (priv->reporting_row);
  gtk_widget_show (priv->reporting_label);
  gtk_widget_show (priv->distro_privacy_policy_label);
}

static void
abrt_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  GisPrivacyPage *page = user_data;
  GisPrivacyPagePrivate *priv = gis_privacy_page_get_instance_private (page);

  gtk_widget_hide (priv->reporting_row);
  gtk_widget_hide (priv->reporting_label);
  gtk_widget_hide (priv->distro_privacy_policy_label);
}

static void
gis_privacy_page_constructed (GObject *object)
{
  GisPrivacyPage *page = GIS_PRIVACY_PAGE (object);
  GisPrivacyPagePrivate *priv = gis_privacy_page_get_instance_private (page);
  char *text;

  G_OBJECT_CLASS (gis_privacy_page_parent_class)->constructed (object);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  priv->location_settings = g_settings_new ("org.gnome.system.location");
  priv->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  gtk_switch_set_active (GTK_SWITCH (priv->location_switch), TRUE);
  gtk_switch_set_active (GTK_SWITCH (priv->reporting_switch), TRUE);

  update_os_data (page);

  text = g_strdup_printf ("%s <a href='%s'>%s</a>", _("Uses Mozilla Location Service:"), "https://location.services.mozilla.com/privacy", _("Privacy Policy"));
  gtk_label_set_markup (GTK_LABEL (priv->mozilla_privacy_policy_label), text);
  g_free (text);

  priv->abrt_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                          "org.freedesktop.problems.daemon",
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          abrt_appeared_cb,
                                          abrt_vanished_cb,
                                          page,
                                          NULL);
}

static void
gis_privacy_page_dispose (GObject *object)
{
  GisPrivacyPage *page = GIS_PRIVACY_PAGE (object);
  GisPrivacyPagePrivate *priv = gis_privacy_page_get_instance_private (page);

  g_clear_object (&priv->location_settings);
  g_clear_object (&priv->privacy_settings);

  if (priv->abrt_watch_id > 0)
    {
      g_bus_unwatch_name (priv->abrt_watch_id);
      priv->abrt_watch_id = 0;
    }

  G_OBJECT_CLASS (gis_privacy_page_parent_class)->dispose (object);
}

static gboolean
gis_privacy_page_apply (GisPage *gis_page,
                        GCancellable *cancellable)
{
  GisPrivacyPage *page = GIS_PRIVACY_PAGE (gis_page);
  GisPrivacyPagePrivate *priv = gis_privacy_page_get_instance_private (page);
  gboolean active;

  active = gtk_switch_get_active (GTK_SWITCH (priv->location_switch));
  g_settings_set_boolean (priv->location_settings, "enabled", active);

  active = gtk_switch_get_active (GTK_SWITCH (priv->reporting_switch));
  g_settings_set_boolean (priv->privacy_settings, "report-technical-problems", active);

  return FALSE;
}

static void
notify_progress_cb (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  GtkWidget *progress_bar = user_data;
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (object);
  gdouble progress;

  progress = webkit_web_view_get_estimated_load_progress (web_view);

  if (progress == 1.0)
    gtk_widget_hide (progress_bar);
  else
    gtk_widget_show (progress_bar);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), progress);
}

static gboolean
activate_link (GtkLabel       *label,
               const gchar    *uri,
               GisPrivacyPage *page)
{
  GtkWidget *dialog;
  GtkWidget *overlay;
  GtkWidget *view;
  GtkWidget *progress_bar;

  dialog = gtk_dialog_new_with_buttons (_("Privacy Policy"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_USE_HEADER_BAR,
                                        NULL, NULL);

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), overlay);

  progress_bar = gtk_progress_bar_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (progress_bar), GTK_STYLE_CLASS_OSD);
  gtk_widget_set_halign (progress_bar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (progress_bar, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), progress_bar);

  view = webkit_web_view_new ();
  gtk_widget_set_size_request (view, 600, 500);
  gtk_widget_set_hexpand (view, TRUE);
  gtk_widget_set_vexpand (view, TRUE);
  g_signal_connect (view, "notify::estimated-load-progress",
                    G_CALLBACK (notify_progress_cb), progress_bar);

  gtk_container_add (GTK_CONTAINER (overlay), view);
  gtk_widget_show_all (overlay);

  gtk_window_present (GTK_WINDOW (dialog));

  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), uri);

  return TRUE;
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
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPrivacyPage, location_switch);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPrivacyPage, reporting_row);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPrivacyPage, reporting_switch);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPrivacyPage, reporting_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPrivacyPage, mozilla_privacy_policy_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPrivacyPage, distro_privacy_policy_label);
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
  g_resources_register (privacy_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_privacy_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_PRIVACY_PAGE,
                       "driver", driver,
                       NULL);
}
