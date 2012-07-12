/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* EULA pages {{{1 */

#include "config.h"
#include "gis-eula-pages.h"
#include "gis-utils.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

typedef struct _EulaPage EulaPage;

struct _EulaPage {
  SetupData *setup;

  GtkWidget *widget;
  GtkWidget *text_view;
  GtkWidget *checkbox;
  GtkWidget *scrolled_window;

  gboolean require_checkbox;
};

/* heavily lifted from g_output_stream_splice */
static void
splice_buffer (GInputStream  *stream,
               GtkTextBuffer *buffer,
               GError       **error)
{
  char contents[8192];
  gssize n_read;
  GtkTextIter iter;

  while (TRUE) {
    n_read = g_input_stream_read (stream, contents, sizeof (contents), NULL, error);

    /* error or eof */
    if (n_read <= 0)
      break;

    gtk_text_buffer_get_end_iter (buffer, &iter);
    gtk_text_buffer_insert (buffer, &iter, contents, n_read);
  }
}

static GtkTextBuffer *
build_eula_text_buffer_pango_markup (GFile   *file,
                                     GError **error_out)
{
  GtkTextBuffer *buffer = NULL;
  gchar *contents;
  gsize length;
  GError *error = NULL;
  PangoAttrList *attrlist;
  gchar *text;
  GtkTextIter iter;

  if (!g_file_load_contents (file, NULL, &contents, &length, NULL, &error))
    goto error_out;

  if (!pango_parse_markup (contents, length, 0, &attrlist, &text, NULL, &error))
    goto error_out;

  g_free (contents);

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_get_end_iter (buffer, &iter);
  gis_gtk_text_buffer_insert_pango_text (buffer, &iter, attrlist, text);

  return buffer;

 error_out:
  g_propagate_error (error_out, error);
  return NULL;
}

static GtkTextBuffer *
build_eula_text_buffer_plain_text (GFile   *file,
                                   GError **error_out)
{
  GtkTextBuffer *buffer = NULL;
  GtkTextIter start, end;
  GError *error = NULL;
  GInputStream *input_stream = NULL;

  input_stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
  if (input_stream == NULL)
    goto error_out;

  buffer = gtk_text_buffer_new (NULL);
  splice_buffer (input_stream, buffer, &error);
  if (error != NULL)
    goto error_out;

  /* monospace the text */
  gtk_text_buffer_create_tag (buffer, "monospace", "family", "monospace", NULL);
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_apply_tag_by_name (buffer, "monospace", &start, &end);

  return buffer;

 error_out:
  g_propagate_error (error_out, error);
  if (buffer != NULL)
    g_object_unref (buffer);
  return NULL;
}

static GtkWidget *
build_eula_text_view (GFile *eula)
{
  GtkWidget *widget = NULL;
  GtkTextBuffer *buffer;
  gchar *path, *last_dot;
  GError *error = NULL;

  path = g_file_get_path (eula);
  last_dot = strrchr (path, '.');

  if (last_dot == NULL || strcmp(last_dot, ".txt") == 0)
    buffer = build_eula_text_buffer_plain_text (eula, &error);
  else if (strcmp (last_dot, ".xml") == 0)
    buffer = build_eula_text_buffer_pango_markup (eula, &error);
  else
    goto out;

  if (buffer == NULL)
    goto out;

  widget = gtk_text_view_new_with_buffer (buffer);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (widget), FALSE);

 out:
  if (error != NULL) {
    g_printerr ("Error while reading EULA: %s", error->message);
    g_error_free (error);
  }

  g_free (path);
  return widget;
}

static gboolean
get_page_complete (EulaPage *page)
{
  if (page->require_checkbox) {
    GtkToggleButton *checkbox = GTK_TOGGLE_BUTTON (page->checkbox);
    if (!gtk_toggle_button_get_active (checkbox))
      return FALSE;
  }

  return TRUE;
}

static void
sync_page_complete (EulaPage *page)
{
  gis_assistant_set_page_complete (gis_get_assistant (page->setup),
                                   page->widget, get_page_complete (page));
}

static void
get_config (GFile    *eula,
            gboolean *require_checkbox)
{
  gchar *path, *config_path;
  GError *error = NULL;
  GKeyFile *config;

  config = g_key_file_new ();

  path = g_file_get_path (eula);
  config_path = g_strconcat (path, ".conf", NULL);
  if (!g_key_file_load_from_file (config, config_path, 0, &error))
    goto out;

  *require_checkbox = g_key_file_get_boolean (config, "Requirements",
                                              "require-checkbox", NULL);

 out:
  g_clear_error (&error);
  g_key_file_unref (config);
}

static void
build_eula_page (SetupData *setup,
                 GFile     *eula)
{
  GtkWidget *text_view;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  EulaPage *page;

  gboolean require_checkbox = TRUE;

  text_view = build_eula_text_view (eula);
  if (text_view == NULL)
    return;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

  gtk_container_add (GTK_CONTAINER (vbox), scrolled_window);

  page = g_slice_new0 (EulaPage);
  page->setup = setup;
  page->widget = vbox;
  page->text_view = text_view;
  page->scrolled_window = scrolled_window;

  get_config (eula, &require_checkbox);

  page->require_checkbox = require_checkbox;

  if (require_checkbox) {
    GtkWidget *checkbox;

    checkbox = gtk_check_button_new_with_mnemonic (_("I have _agreed to the "
                                                     "terms and conditions in "
                                                     "this end user license "
                                                     "agreement."));

    gtk_container_add (GTK_CONTAINER (vbox), checkbox);

    g_signal_connect_swapped (checkbox, "toggled",
                              G_CALLBACK (sync_page_complete),
                              page);

    page->checkbox = checkbox;
  }

  g_object_set_data (G_OBJECT (vbox), "gis-page-title", _("License Agreements"));
  gis_assistant_add_page (gis_get_assistant (setup), vbox);

  sync_page_complete (page);

  gtk_widget_show_all (GTK_WIDGET (vbox));
}

void
gis_prepare_eula_pages (SetupData *setup)
{
  gchar *eulas_dir_path;
  GFile *eulas_dir;
  GError *error = NULL;
  GFileEnumerator *enumerator = NULL;
  GFileInfo *info;

  eulas_dir_path = g_build_filename (UIDIR, "eulas", NULL);
  eulas_dir = g_file_new_for_path (eulas_dir_path);
  g_free (eulas_dir_path);

  if (!g_file_query_exists (eulas_dir, NULL))
    goto out;

  enumerator = g_file_enumerate_children (eulas_dir,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);

  if (error != NULL)
    goto out;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL) {
    GFile *eula = g_file_get_child (eulas_dir, g_file_info_get_name (info));
    build_eula_page (setup, eula);
    g_object_unref (eula);
  }

  if (error != NULL)
    goto out;

 out:
  if (error != NULL) {
    g_printerr ("Error while parsing eulas: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (eulas_dir);
  g_clear_object (&enumerator);
}
