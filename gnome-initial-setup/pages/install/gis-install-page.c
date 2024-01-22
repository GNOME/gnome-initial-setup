/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2023 Red Hat
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

/* Install page {{{1 */

#define PAGE_ID "install"

#include "config.h"
#include "cc-common-language.h"
#include "gis-install-page.h"
#include "gis-pkexec.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>
#include <errno.h>

#include <act/act-user-manager.h>

#define SERVICE_NAME "gdm-password"
#define VENDOR_INSTALLER_GROUP "install"
#define VENDOR_APPLICATION_KEY "application"

struct _GisInstallPagePrivate {
  GtkWidget *try_button;
  GtkWidget *install_button;
  AdwStatusPage *status_page;
  GDesktopAppInfo *installer;
  char *user_password;
};
typedef struct _GisInstallPagePrivate GisInstallPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisInstallPage, gis_install_page, GIS_TYPE_PAGE);

static void
request_info_query (GisInstallPage  *page,
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
         GisInstallPage  *page)
{
  g_debug ("PAM module info: %s", info);
}

static void
on_problem (GdmUserVerifier *user_verifier,
            const char      *service_name,
            const char      *problem,
            GisInstallPage  *page)
{
  g_warning ("PAM module error: %s", problem);
}

static void
on_info_query (GdmUserVerifier *user_verifier,
               const char      *service_name,
               const char      *question,
               GisInstallPage  *page)
{
  request_info_query (page, user_verifier, question, FALSE);
}

static void
on_secret_info_query (GdmUserVerifier *user_verifier,
                      const char      *service_name,
                      const char      *question,
                      GisInstallPage  *page)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);
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
                   GisInstallPage *page)
{
  gdm_greeter_call_start_session_when_ready_sync (greeter, service_name,
                                                  TRUE, NULL, NULL);
}

static void
log_user_in (GisInstallPage *page)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);
  g_autoptr(GError) error = NULL;
  GdmGreeter *greeter = NULL;
  GdmUserVerifier *user_verifier = NULL;
  ActUser *user_account = NULL;
  const char *user_password = NULL;

  gis_driver_get_user_permissions (GIS_PAGE (page)->driver,
                                   &user_account,
                                   &user_password);
  if (user_account == NULL) {
    g_info ("No new user account (was the account page skipped?); not initiating login");
    return;
  }
  g_assert (priv->user_password == NULL);
  priv->user_password = g_strdup (user_password);

  if (!gis_driver_get_gdm_objects (GIS_PAGE (page)->driver,
                                   &greeter, &user_verifier)) {
    g_info ("No GDM connection; not initiating login");
    return;
  }

  if (!gis_driver_get_gdm_objects (GIS_PAGE (page)->driver,
                                   &greeter, &user_verifier)) {
    g_info ("No GDM connection; not initiating login");
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

  gdm_user_verifier_call_begin_verification_for_user_sync (user_verifier,
                                                           SERVICE_NAME,
                                                           act_user_get_user_name (user_account),
                                                           NULL, &error);

  if (error != NULL)
    g_warning ("Could not begin verification: %s", error->message);
}

static void
on_try_button_clicked (GtkButton      *button,
                       GisInstallPage *page)
{

  g_autoptr (GError) error = NULL;

  if (!gis_driver_save_data (GIS_PAGE (page)->driver, &error))
    g_warning ("Error saving data: %s", error->message);

  gis_ensure_stamp_files (GIS_PAGE (page)->driver);

  gis_driver_hide_window (GIS_PAGE (page)->driver);
  log_user_in (page);
}

static void
on_installer_exited (GPid     pid,
                     int      exit_status,
                     gpointer user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  gboolean started_to_reboot;

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_SEARCH_PATH_FROM_ENVP);

  g_subprocess_launcher_unsetenv (launcher, "SHELL");

  subprocess = g_subprocess_launcher_spawn (launcher, &error, "systemctl", "reboot", NULL);

  if (subprocess == NULL) {
    g_warning ("Failed to initiate reboot: %s\n", error->message);
    return;
  }

  started_to_reboot = g_subprocess_wait (subprocess, NULL, &error);

  if (!started_to_reboot) {
    g_warning ("Failed to reboot: %s\n", error->message);
    return;
  }
}

static void
on_installer_started (GDesktopAppInfo *appinfo,
                      GPid             pid,
                      gpointer         user_data)
{
  g_child_watch_add (pid, on_installer_exited, user_data);
}

static void
run_installer (GisInstallPage *page)
{
  g_autoptr (GError) error = NULL;
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);
  gboolean installer_launched;
  g_autoptr (GAppLaunchContext) launch_context = NULL;
  g_autofree char *language = NULL;

  if (!gis_driver_save_data (GIS_PAGE (page)->driver, &error))
    g_warning ("Error saving data: %s", error->message);

  gis_ensure_stamp_files (GIS_PAGE (page)->driver);

  launch_context = g_app_launch_context_new ();

  g_app_launch_context_unsetenv (launch_context, "SHELL");

  language = cc_common_language_get_current_language ();

  if (language != NULL)
    g_app_launch_context_setenv (launch_context, "LANG", language);

  installer_launched = g_desktop_app_info_launch_uris_as_manager (priv->installer,
                                                                  NULL,
                                                                  launch_context,
                                                                  G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CHILD_INHERITS_STDERR | G_SPAWN_CHILD_INHERITS_STDOUT | G_SPAWN_SEARCH_PATH,
                                                                  NULL,
                                                                  NULL,
                                                                  on_installer_started,
                                                                  page,
                                                                  &error);

  if (!installer_launched)
    g_warning ("Could not launch installer: %s", error->message);
}

static void
on_install_button_clicked (GtkButton      *button,
                           GisInstallPage *page)
{
  gis_driver_hide_window (GIS_PAGE (page)->driver);
  run_installer (page);
}

static void
gis_install_page_shown (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);

  gtk_widget_grab_focus (priv->install_button);
}

static void
update_distro_name (GisInstallPage *page)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);
  g_autofree char *text = NULL;

  text = g_strdup (adw_status_page_get_description (priv->status_page));
  gis_substitute_variables_in_text (&text, (GisVariableLookupFunc) g_get_os_info, NULL);
  adw_status_page_set_description (priv->status_page, text);
  g_clear_pointer (&text, g_free);

  text = g_strdup (gtk_button_get_label (GTK_BUTTON (priv->try_button)));
  gis_substitute_variables_in_text (&text, (GisVariableLookupFunc) g_get_os_info, NULL);
  gtk_button_set_label (GTK_BUTTON (priv->try_button), text);
  g_clear_pointer (&text, g_free);
}


static void
apply_stylesheet (GisInstallPage *page)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);
  g_autoptr (GtkCssProvider) css_provider = gtk_css_provider_new();

  gtk_widget_add_css_class (GTK_WIDGET (priv->status_page), "override-icon-size");
  gtk_widget_add_css_class (GTK_WIDGET (priv->status_page), "override-button-line-height");

  gtk_css_provider_load_from_resource (css_provider, "/org/gnome/initial-setup/gis-install-page.css");

  gtk_style_context_add_provider_for_display (gtk_widget_get_display (GTK_WIDGET (priv->status_page)),
                                              GTK_STYLE_PROVIDER (css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static gboolean
find_installer (GisInstallPage *page)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);
  g_autofree char *desktop_file = NULL;

  desktop_file = gis_driver_conf_get_string (GIS_PAGE (page)->driver,
                                             VENDOR_INSTALLER_GROUP,
                                             VENDOR_APPLICATION_KEY);

  if (!desktop_file)
    return FALSE;

  priv->installer = g_desktop_app_info_new (desktop_file);

  return priv->installer != NULL;
}

static void
gis_install_page_constructed (GObject *object)
{
  GisInstallPage *page = GIS_INSTALL_PAGE (object);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_install_page_parent_class)->constructed (object);

  if (!find_installer (page))
    gtk_widget_set_sensitive (priv->install_button, FALSE);

  apply_stylesheet (page);
  update_distro_name (page);
  g_signal_connect (priv->try_button, "clicked", G_CALLBACK (on_try_button_clicked), page);
  g_signal_connect (priv->install_button, "clicked", G_CALLBACK (on_install_button_clicked), page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_set_visible (GTK_WIDGET (page), TRUE);
}

static void
gis_install_page_finalize (GObject *object)
{
  GisInstallPage *page = GIS_INSTALL_PAGE (object);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);

  g_clear_pointer (&priv->user_password, g_free);

  G_OBJECT_CLASS (gis_install_page_parent_class)->finalize (object);
}

static void
gis_install_page_locale_changed (GisPage *page)
{
  g_autofree char *title = g_strdup (_("Install ${PRETTY_NAME}"));
  gis_substitute_variables_in_text (&title, (GisVariableLookupFunc) g_get_os_info, NULL);
  gis_page_set_title (page, title);
}

static void
gis_install_page_class_init (GisInstallPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-install-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisInstallPage, try_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisInstallPage, install_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisInstallPage, status_page);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_install_page_locale_changed;
  page_class->shown = gis_install_page_shown;
  object_class->constructed = gis_install_page_constructed;
  object_class->finalize = gis_install_page_finalize;
}

static void
gis_install_page_init (GisInstallPage *page)
{
  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_install_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_INSTALL_PAGE,
                       "driver", driver,
                       NULL);
}
