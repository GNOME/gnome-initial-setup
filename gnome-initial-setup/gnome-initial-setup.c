/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "gnome-initial-setup.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>

#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>

#include "gis-assistant-gtk.h"
#include "gis-assistant-clutter.h"

#include "pages/language/gis-language-page.h"
#include "pages/eulas/gis-eula-pages.h"
#include "pages/location/gis-location-page.h"
#include "pages/account/gis-account-page.h"
#include "pages/network/gis-network-page.h"
#include "pages/goa/gis-goa-page.h"
#include "pages/summary/gis-summary-page.h"

#include "gis-utils.h"

/* Setup data {{{1 */
struct _SetupData {
  GtkWindow *main_window;
  GKeyFile *overrides;
  GisAssistant *assistant;

  GSList *finals;
};

typedef struct _AsyncClosure AsyncClosure;

struct _AsyncClosure {
  GFunc callback;
  gpointer user_data;
};

static void
run_finals (SetupData *setup)
{
  GSList *l;

  for (l = setup->finals; l != NULL; l = l->next) {
    AsyncClosure *closure = l->data;
    closure->callback (setup, closure->user_data);
  }
}

static void
prepare_cb (GisAssistant *assi, GtkWidget *page, SetupData *setup)
{
  gchar *page_title;

  g_debug ("Preparing page %s", gtk_widget_get_name (page));

  page_title = g_object_get_data (G_OBJECT (page), "gis-page-title");
  gtk_window_set_title (setup->main_window, page_title);

  if (g_object_get_data (G_OBJECT (page), "gis-summary"))
    run_finals (setup);
}

static void
recenter_window (GdkScreen *screen, SetupData *setup)
{
  gtk_window_set_position (setup->main_window, GTK_WIN_POS_CENTER_ALWAYS);
}

static void
prepare_main_window (SetupData *setup)
{
  g_signal_connect (gtk_widget_get_screen (GTK_WIDGET (setup->main_window)),
                    "monitors-changed", G_CALLBACK (recenter_window), setup);

  g_signal_connect (setup->assistant, "prepare",
                    G_CALLBACK (prepare_cb), setup);

  gis_prepare_language_page (setup);
  gis_prepare_eula_pages (setup);
  gis_prepare_network_page (setup);
  gis_prepare_account_page (setup);
  gis_prepare_location_page (setup);
  gis_prepare_online_page (setup);
  gis_prepare_summary_page (setup);
}

GKeyFile *
gis_get_overrides (SetupData *setup)
{
  return g_key_file_ref (setup->overrides);
}

GtkWindow *
gis_get_main_window (SetupData *setup)
{
  return setup->main_window;
}

GisAssistant *
gis_get_assistant (SetupData *setup)
{
  return setup->assistant;
}

void
gis_add_summary_callback (SetupData *setup,
                          GFunc      callback,
                          gpointer   user_data)
{
  AsyncClosure *closure = g_slice_new (AsyncClosure);

  closure->callback = callback;
  closure->user_data = user_data;

  setup->finals = g_slist_append (setup->finals, closure);
}

/* main {{{1 */

int
main (int argc, char *argv[])
{
  SetupData *setup;
  gchar *filename;
  GError *error;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#ifdef HAVE_CHEESE
  cheese_gtk_init (NULL, NULL);
#endif

  setup = g_new0 (SetupData, 1);

  gtk_init (&argc, &argv);

  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS) {
    g_critical ("Clutter-GTK init failed");
    exit (1);
  }

  error = NULL;
  if (g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error) == NULL) {
    g_error ("Couldn't get on session bus: %s", error->message);
    exit (1);
  }

  setup->main_window = g_object_new (GTK_TYPE_WINDOW,
                                     "type", GTK_WINDOW_TOPLEVEL,
                                     "border-width", 12,
                                     "icon-name", "preferences-system",
                                     "deletable", FALSE,
                                     "resizable", FALSE,
                                     "window-position", GTK_WIN_POS_CENTER_ALWAYS,
                                     NULL);

  setup->assistant = g_object_new (GIS_TYPE_ASSISTANT_CLUTTER, NULL);
  gtk_container_add (GTK_CONTAINER (setup->main_window), GTK_WIDGET (setup->assistant));

  gtk_widget_show (GTK_WIDGET (setup->assistant));

  setup->overrides = g_key_file_new ();
  filename = g_build_filename (UIDIR, "overrides.ini", NULL);
  if (!g_key_file_load_from_file (setup->overrides, filename, 0, &error)) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
      g_error ("%s", error->message);
      exit (1);
    }
    g_error_free (error);
  }
  g_free (filename);

  prepare_main_window (setup);

  gtk_window_present (GTK_WINDOW (setup->main_window));

  gtk_main ();

  return 0;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
