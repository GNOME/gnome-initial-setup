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

/* Summary page {{{1 */

#define PAGE_ID "summary"

#include "config.h"
#include "summary-resources.h"
#include "gis-summary-page.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

#include <act/act-user-manager.h>

#define SERVICE_NAME "gdm-password"

struct _GisSummaryPagePrivate {
  GtkWidget *start_button;
  AdwStatusPage *status_page;

  ActUser *user_account;
  const gchar *user_password;
};
typedef struct _GisSummaryPagePrivate GisSummaryPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSummaryPage, gis_summary_page, GIS_TYPE_PAGE);

static void
request_info_query (GisSummaryPage  *page,
                    GdmUserVerifier *user_verifier,
                    const char      *question,
                    gboolean         is_secret)
{
  /* TODO: pop up modal dialog */
  g_debug ("user verifier asks%s question: %s",
           is_secret ? " secret" : "",
           question);
}

static void
on_info (GdmUserVerifier *user_verifier,
         const char      *service_name,
         const char      *info,
         GisSummaryPage  *page)
{
  g_debug ("PAM module info: %s", info);
}

static void
on_problem (GdmUserVerifier *user_verifier,
            const char      *service_name,
            const char      *problem,
            GisSummaryPage  *page)
{
  g_warning ("PAM module error: %s", problem);
}

static void
on_info_query (GdmUserVerifier *user_verifier,
               const char      *service_name,
               const char      *question,
               GisSummaryPage  *page)
{
  request_info_query (page, user_verifier, question, FALSE);
}

static void
on_secret_info_query (GdmUserVerifier *user_verifier,
                      const char      *service_name,
                      const char      *question,
                      GisSummaryPage  *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  gboolean should_send_password = priv->user_password != NULL;

  g_debug ("PAM module secret info query: %s", question);
  if (should_send_password) {
    g_debug ("sending password\n");
    gdm_user_verifier_call_answer_query (user_verifier,
                                         service_name,
                                         priv->user_password,
                                         NULL, NULL, NULL);
    priv->user_password = NULL;
  } else {
    request_info_query (page, user_verifier, question, TRUE);
  }
}

static void
on_session_opened (GdmGreeter     *greeter,
                   const char     *service_name,
                   GisSummaryPage *page)
{
  gdm_greeter_call_start_session_when_ready_sync (greeter, service_name,
                                                  TRUE, NULL, NULL);
}

static void
add_uid_file (uid_t uid)
{
  gchar *gis_uid_path;
  gchar *uid_str;
  g_autoptr(GError) error = NULL;

  gis_uid_path = g_build_filename (g_get_home_dir (),
                                   "gnome-initial-setup-uid",
                                   NULL);
  uid_str = g_strdup_printf ("%u", uid);

  if (!g_file_set_contents (gis_uid_path, uid_str, -1, &error))
      g_warning ("Unable to create %s: %s", gis_uid_path, error->message);

  g_free (uid_str);
  g_free (gis_uid_path);
}

static void
log_user_in (GisSummaryPage *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  g_autoptr(GError) error = NULL;
  GdmGreeter *greeter = NULL;
  GdmUserVerifier *user_verifier = NULL;

  if (!gis_driver_get_gdm_objects (GIS_PAGE (page)->driver,
                                   &greeter, &user_verifier)) {
    g_warning ("No GDM connection; not initiating login");
    return;
  }

  g_signal_connect (user_verifier, "info",
                    G_CALLBACK (on_info), page);
  g_signal_connect (user_verifier, "problem",
                    G_CALLBACK (on_problem), page);
  g_signal_connect (user_verifier, "info-query",
                    G_CALLBACK (on_info_query), page);
  g_signal_connect (user_verifier, "secret-info-query",
                    G_CALLBACK (on_secret_info_query), page);

  g_signal_connect (greeter, "session-opened",
                    G_CALLBACK (on_session_opened), page);

  /* We are in NEW_USER mode and we want to make it possible for third
   * parties to find out which user ID we created.
   */
  add_uid_file (act_user_get_uid (priv->user_account));

  gdm_user_verifier_call_begin_verification_for_user_sync (user_verifier,
                                                           SERVICE_NAME,
                                                           act_user_get_user_name (priv->user_account),
                                                           NULL, &error);

  if (error != NULL)
    g_warning ("Could not begin verification: %s", error->message);
}

static void
done_cb (GtkButton *button, GisSummaryPage *page)
{
  gis_ensure_stamp_files (GIS_PAGE (page)->driver);

  switch (gis_driver_get_mode (GIS_PAGE (page)->driver))
    {
    case GIS_DRIVER_MODE_NEW_USER:
      gis_driver_hide_window (GIS_PAGE (page)->driver);
      log_user_in (page);
      break;
    case GIS_DRIVER_MODE_EXISTING_USER:
      g_application_quit (G_APPLICATION (GIS_PAGE (page)->driver));
    default:
      break;
    }
}

static void
gis_summary_page_shown (GisPage *page)
{
  GisSummaryPage *summary = GIS_SUMMARY_PAGE (page);
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (summary);
  g_autoptr(GError) local_error = NULL;

  if (!gis_driver_save_data (GIS_PAGE (page)->driver, &local_error))
    {
      g_warning ("Error saving data: %s", local_error->message);

      GtkWindow *parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (page)));
      GtkWidget *dialog = adw_message_dialog_new (parent,
                                                  _("Setup Failed"),
                                                  local_error->message);
      adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "close", _("Close"));
      /* FIXME: Provide some more options for debugging or recovery */

      gtk_window_present (GTK_WINDOW (dialog));
    }

  gis_driver_get_user_permissions (GIS_PAGE (page)->driver,
                                   &priv->user_account,
                                   &priv->user_password);

  gtk_widget_grab_focus (priv->start_button);
}

static void
update_distro_name (GisSummaryPage *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  g_autofree char *name = g_get_os_info (G_OS_INFO_KEY_NAME);
  char *text;

  if (!name)
    name = g_strdup ("GNOME");

  /* Translators: the parameter here is the name of a distribution,
   * like "Fedora" or "Ubuntu". It falls back to "GNOME" if we can't
   * detect any distribution. */
  text = g_strdup_printf (_("_Start Using %s"), name);
  gtk_button_set_label (GTK_BUTTON (priv->start_button), text);
  g_free (text);

  /* Translators: the parameter here is the name of a distribution,
   * like "Fedora" or "Ubuntu". It falls back to "GNOME" if we can't
   * detect any distribution. */
  text = g_strdup_printf (_("%s is ready to be used."), name);
  adw_status_page_set_description (priv->status_page, text);
  g_free (text);
}

static void
gis_summary_page_constructed (GObject *object)
{
  GisSummaryPage *page = GIS_SUMMARY_PAGE (object);
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_summary_page_parent_class)->constructed (object);

  update_distro_name (page);
  g_signal_connect (priv->start_button, "clicked", G_CALLBACK (done_cb), page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_set_visible (GTK_WIDGET (page), TRUE);
}

static void
gis_summary_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Setup Complete"));
  update_distro_name (GIS_SUMMARY_PAGE (page));
}

static void
gis_summary_page_class_init (GisSummaryPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-summary-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, start_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, status_page);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_summary_page_locale_changed;
  page_class->shown = gis_summary_page_shown;
  object_class->constructed = gis_summary_page_constructed;
}

static void
gis_summary_page_init (GisSummaryPage *page)
{
  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_summary_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_SUMMARY_PAGE,
                       "driver", driver,
                       NULL);
}
