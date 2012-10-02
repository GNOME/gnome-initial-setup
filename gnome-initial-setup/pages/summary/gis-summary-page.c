/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"
#include "gis-summary-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <act/act-user-manager.h>

#include <gdm/gdm-client.h>

#define OBJ(type,name) ((type)gtk_builder_get_object(builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

#define SERVICE_NAME "gnome-initial-setup"

#define SKELETON_DIR "/dev/shm/gnome-initial-setup/skeleton"

typedef struct _SummaryData SummaryData;

struct _SummaryData {
  ActUser *user_account;
  const gchar *user_password;
  SetupData *setup;
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

static gboolean
recursively_delete (GFile   *file,
                    GError **error_out)
{
  GError *error = NULL;

  if (!g_file_query_exists (file, NULL))
    goto out;

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY) {
    GFileEnumerator *enumerator;
    GFileInfo *info;

    enumerator = g_file_enumerate_children (file,
                                            G_FILE_ATTRIBUTE_STANDARD_NAME,
                                            G_FILE_QUERY_INFO_NONE,
                                            NULL,
                                            &error);
    if (error != NULL)
      goto out;

    while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL) {
      GFile *child;
      gboolean ret;

      if (error != NULL)
        goto out;

      child = g_file_get_child (file, g_file_info_get_name (info));

      ret = recursively_delete (child, &error);
      g_object_unref (child);

      if (!ret)
        goto out;
    }
  }

  if (!g_file_delete (file, NULL, &error))
    goto out;

 out:
  if (error != NULL) {
    g_propagate_error (error_out, error);
    return FALSE;
  } else {
    return TRUE;
  }
}

static void
copy_file_to_tmpfs (GFile *dest_base,
                    const gchar *dir,
                    const gchar *path)
{
  GFile *src_dir = g_file_new_for_path (dir);
  GFile *src = g_file_get_child (src_dir, path);
  gchar *basename = g_file_get_basename (src);
  GFile *dest = g_file_get_child (dest_base, basename);

  GError *error = NULL;

  if (!g_file_copy (src, dest, G_FILE_COPY_NONE,
                    NULL, NULL, NULL, &error)) {
    g_warning ("Unable to copy %s to %s: %s",
               g_file_get_path (src),
               g_file_get_path (dest),
               error->message);
    goto out;
  }

 out:
  g_object_unref (src_dir);
  g_object_unref (src);
  g_object_unref (dest);
  g_free (basename);
  g_clear_error (&error);
}

static void
copy_files_to_tmpfs (void)
{
  GFile *dest = g_file_new_for_path (SKELETON_DIR);
  GError *error = NULL;

  if (!recursively_delete (dest, &error)) {
    g_warning ("Unable to delete old skeleton folder: %s",
               error->message);
    goto out;
  }
 
  if (!g_file_make_directory_with_parents (dest, NULL, &error)) {
    g_warning ("Unable to make new skeleton folder: %s",
               error->message);
    goto out;
  }

  copy_file_to_tmpfs (dest, g_get_user_config_dir (), "dconf/user");
  copy_file_to_tmpfs (dest, g_get_user_config_dir (), "goa-1.0/accounts.conf");
  copy_file_to_tmpfs (dest, g_get_user_data_dir (), "keyrings/Default.keyring");

 out:
  g_object_unref (dest);
  g_clear_error (&error);
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
  copy_files_to_tmpfs ();
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
  /* the tour is triggered by /tmp/run-welcome-tour */
  g_file_set_contents ("/tmp/run-welcome-tour", "yes", -1, NULL);
  byebye (data);
}

static void
install_overrides (SetupData  *setup,
                   GtkBuilder *builder)
{
  gchar *s;
  GKeyFile *overrides = gis_get_overrides (setup);

  s = g_key_file_get_locale_string (overrides,
                                    "Summary", "summary-title",
                                    NULL, NULL);
  if (s)
    gtk_label_set_text (GTK_LABEL (WID ("summary-title")), s);
  g_free (s);

  s = g_key_file_get_locale_string (overrides,
                                    "Summary", "summary-details",
                                    NULL, NULL);
  if (s) {
    gtk_label_set_text (GTK_LABEL (WID ("summary-details")), s);
  }
  g_free (s);

  s = g_key_file_get_locale_string (overrides,
                                    "Summary", "summary-details2",
                                    NULL, NULL);
  if (s)
    gtk_label_set_text (GTK_LABEL (WID ("summary-details2")), s);
  g_free (s);

  s = g_key_file_get_locale_string (overrides,
                                    "Summary", "summary-start-button",
                                    NULL, NULL);
  if (s)
    gtk_button_set_label (GTK_BUTTON (WID ("summary-start-button")), s);
  g_free (s);

  s = g_key_file_get_locale_string (overrides,
                                    "Summary", "summary-tour-details",
                                    NULL, NULL);
  if (s)
    gtk_label_set_text (GTK_LABEL (WID ("summary-tour-details")), s);
  g_free (s);

  s = g_key_file_get_locale_string (overrides,
                                    "Summary", "summary-tour-button",
                                    NULL, NULL);
  if (s)
    gtk_button_set_label (GTK_BUTTON (WID ("summary-tour-button")), s);
  g_free (s);
}

static void
prepare_cb (GisAssistant *assistant, GtkWidget *page, SummaryData *data)
{
  if (g_strcmp0 (gtk_widget_get_name (page), "summary-page") == 0)
    {
      gis_get_user_permissions (data->setup,
                                &data->user_account,
                                &data->user_password);
    }
}

void
gis_prepare_summary_page (SetupData *setup)
{
  GisAssistant *assistant = gis_get_assistant (setup);
  GtkBuilder *builder = gis_builder ("gis-summary-page");
  SummaryData *data;

  data = g_slice_new0 (SummaryData);
  data->setup = setup;

  g_signal_connect (assistant, "prepare", G_CALLBACK (prepare_cb), data);

  install_overrides (setup, builder);

  g_signal_connect (WID("summary-start-button"), "clicked", G_CALLBACK (byebye_cb), data);
  g_signal_connect (WID("summary-tour-button"), "clicked", G_CALLBACK (tour_cb), data);

  gis_assistant_add_page (assistant, WID ("summary-page"));
  gis_assistant_set_page_title (assistant, WID ("summary-page"), _("Thank You"));
  gis_assistant_set_page_complete (assistant, WID ("summary-page"), TRUE);
}
