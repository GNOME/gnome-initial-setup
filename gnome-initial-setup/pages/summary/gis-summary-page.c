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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Summary page {{{1 */

#define PAGE_ID "summary"

#include "config.h"
#include "gis-summary-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>

#include <act/act-user-manager.h>

#include <gdm/gdm-client.h>

#define OBJ(type,name) ((type)gtk_builder_get_object(data->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

#define SERVICE_NAME "gdm-password"

typedef struct _SummaryData SummaryData;

struct _SummaryData {
  GisDriver *driver;
  GtkWidget *widget;
  GtkBuilder *builder;

  ActUser *user_account;
  const gchar *user_password;
};

static gboolean
connect_to_gdm (GdmGreeter      **greeter,
                GdmUserVerifier **user_verifier)
{
  GdmClient *client;

  GError *error = NULL;
  gboolean res = FALSE;

  client = gdm_client_new ();

  *greeter = gdm_client_get_greeter_sync (client, NULL, &error);
  if (error != NULL)
    goto out;

  *user_verifier = gdm_client_get_user_verifier_sync (client, NULL, &error);
  if (error != NULL)
    goto out;

  res = TRUE;

 out:
  if (error != NULL) {
    g_warning ("Failed to open connection to GDM: %s", error->message);
    g_error_free (error);
  }

  return res;
}

static void
request_info_query (SummaryData     *data,
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
         SummaryData     *data)
{
  g_debug ("PAM module info: %s\n", info);
}

static void
on_problem (GdmUserVerifier *user_verifier,
            const char      *service_name,
            const char      *problem,
            SummaryData     *data)
{
  g_warning ("PAM module error: %s\n", problem);
}

static void
on_info_query (GdmUserVerifier *user_verifier,
               const char      *service_name,
               const char      *question,
               SummaryData     *data)
{
  request_info_query (data, user_verifier, question, FALSE);
}

static void
on_secret_info_query (GdmUserVerifier *user_verifier,
                      const char      *service_name,
                      const char      *question,
                      SummaryData     *data)
{
  gboolean should_send_password = data->user_password != NULL;

  g_debug ("PAM module secret info query: %s\n", question);
  if (should_send_password) {
    g_debug ("sending password\n");
    gdm_user_verifier_call_answer_query (user_verifier,
                                         service_name,
                                         data->user_password,
                                         NULL, NULL, NULL);
    g_clear_pointer (&data->user_password, (GDestroyNotify) g_free);
  } else {
    request_info_query (data, user_verifier, question, TRUE);
  }
}

static void
on_session_opened (GdmGreeter  *greeter,
                   const char  *service_name,
                   SummaryData *data)
{
  gdm_greeter_call_start_session_when_ready_sync (greeter, service_name,
                                                  TRUE, NULL, NULL);
}

static void
log_user_in (SummaryData *data)
{
  GError *error = NULL;
  GdmGreeter *greeter;
  GdmUserVerifier *user_verifier;

  if (!connect_to_gdm (&greeter, &user_verifier)) {
    g_warning ("No GDM connection; not initiating login");
    return;
  }

  if (error != NULL) {
    g_warning ("Could not set PAM_AUTHTOK: %s", error->message);
    return;
  }

  g_signal_connect (user_verifier, "info",
                    G_CALLBACK (on_info), data);
  g_signal_connect (user_verifier, "problem",
                    G_CALLBACK (on_problem), data);
  g_signal_connect (user_verifier, "info-query",
                    G_CALLBACK (on_info_query), data);
  g_signal_connect (user_verifier, "secret-info-query",
                    G_CALLBACK (on_secret_info_query), data);

  g_signal_connect (greeter, "session-opened",
                    G_CALLBACK (on_session_opened), data);

  gdm_user_verifier_call_begin_verification_for_user_sync (user_verifier,
                                                           SERVICE_NAME,
                                                           act_user_get_user_name (data->user_account),
                                                           NULL, &error);

  if (error != NULL) {
    g_warning ("Could not begin verification: %s", error->message);
    return;
  }
}

static void
byebye (SummaryData *data)
{
  log_user_in (data);
}

static void
byebye_cb (GtkButton *button, SummaryData *data)
{
  byebye (data);
}

static void
tour_cb (GtkButton *button, SummaryData *data)
{
  gchar *file;

  /* the tour is triggered by $XDG_CONFIG_HOME/run-welcome-tour */
  file = g_build_filename (g_get_user_config_dir (), "run-welcome-tour", NULL);
  g_file_set_contents (file, "yes", -1, NULL);
  g_free (file);
  byebye (data);
}

static void
prepare_cb (GisAssistant *assistant, GtkWidget *page, SummaryData *data)
{
  if (page == data->widget)
    {
      gis_driver_get_user_permissions (data->driver,
                                       &data->user_account,
                                       &data->user_password);
    }
}

static GtkBuilder *
get_builder (void)
{
  GtkBuilder *builder = gtk_builder_new ();

  char *filename = g_build_filename (UIDIR, "summary-distro.ui", NULL);
  GError *error = NULL;

  if (gtk_builder_add_from_file (builder, filename, &error))
    goto out;

  if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    g_warning ("Error while loading summary override: %s", error->message);

  g_clear_error (&error);

  {
    char *resource_path = "/ui/gis-" PAGE_ID "-page.ui";
    gtk_builder_add_from_resource (builder, resource_path, &error);

    if (error != NULL) {
      g_warning ("Error while loading %s: %s", resource_path, error->message);
      exit (1);
    }
  }

 out:
  return builder;
}

void
gis_prepare_summary_page (GisDriver *driver)
{
  GisAssistant *assistant = gis_driver_get_assistant (driver);
  SummaryData *data;

  data = g_slice_new0 (SummaryData);
  data->driver = driver;
  data->builder = get_builder ();
  data->widget = WID ("summary-page");

  g_signal_connect (assistant, "prepare", G_CALLBACK (prepare_cb), data);

  g_signal_connect (WID("summary-start-button"), "clicked", G_CALLBACK (byebye_cb), data);
  g_signal_connect (WID("summary-tour-button"), "clicked", G_CALLBACK (tour_cb), data);

  gis_assistant_add_page (assistant, data->widget);
  gis_assistant_set_page_title (assistant, data->widget, _("Thank You"));
  gis_assistant_set_page_complete (assistant, data->widget, TRUE);
}
