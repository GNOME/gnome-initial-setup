/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "gnome-initial-setup.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>

#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>

#include "gis-assistant-gtk.h"

#include <cheese-gtk.h>

#ifdef HAVE_CLUTTER
#include "gis-assistant-clutter.h"
#endif

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
title_changed_cb (GisAssistant *assistant,
                  GParamSpec   *gparam,
                  SetupData    *setup)
{
  gtk_window_set_title (setup->main_window, gis_assistant_get_title (assistant));
}

static void
prepare_cb (GisAssistant *assi, GtkWidget *page, SetupData *setup)
{
  g_debug ("Preparing page %s", gtk_widget_get_name (page));

  title_changed_cb (assi, NULL, setup);

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

  g_signal_connect (setup->assistant, "notify::title",
                    G_CALLBACK (title_changed_cb), setup);

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

static GType
get_assistant_type (void)
{
#ifdef HAVE_CLUTTER
  gboolean enable_animations;
  g_object_get (gtk_settings_get_default (),
                "gtk-enable-animations", &enable_animations,
                NULL);

  if (enable_animations && g_getenv ("GIS_DISABLE_CLUTTER") == NULL)
    return GIS_TYPE_ASSISTANT_CLUTTER;
#endif /* HAVE_CLUTTER */

  return GIS_TYPE_ASSISTANT_GTK;
}

/* main {{{1 */

static void
load_overrides (SetupData *setup)
{
  gchar *filename;
  GError *error = NULL;

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
}

static void
activate_cb (GApplication *app,
             gpointer      user_data)
{
  SetupData *setup = user_data;

  gtk_window_present (GTK_WINDOW (setup->main_window));
}

static void
startup_cb (GApplication *app,
            gpointer      user_data)
{
  SetupData *setup = user_data;

  setup->main_window = g_object_new (GTK_TYPE_WINDOW,
                                     "type", GTK_WINDOW_TOPLEVEL,
                                     "border-width", 12,
                                     "icon-name", "preferences-system",
                                     "deletable", FALSE,
                                     "resizable", FALSE,
                                     "window-position", GTK_WIN_POS_CENTER_ALWAYS,
                                     NULL);
  gtk_application_add_window (GTK_APPLICATION (app), setup->main_window);

  setup->assistant = g_object_new (get_assistant_type (), NULL);
  gtk_container_add (GTK_CONTAINER (setup->main_window), GTK_WIDGET (setup->assistant));

  gtk_widget_show (GTK_WIDGET (setup->assistant));

  load_overrides (setup);

  prepare_main_window (setup);
}

int
main (int argc, char *argv[])
{
  SetupData *setup;
  GtkApplication *application;
  int status;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  cheese_gtk_init (NULL, NULL);

  setup = g_new0 (SetupData, 1);

  gtk_init (&argc, &argv);

#if HAVE_CLUTTER
  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS) {
    g_critical ("Clutter-GTK init failed");
    exit (1);
  }
#endif

  application = gtk_application_new ("org.gnome.InitialSetup", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (application, "startup",
                    G_CALLBACK (startup_cb), setup);
  g_signal_connect (application, "activate",
                    G_CALLBACK (activate_cb), setup);

  status = g_application_run (G_APPLICATION (application), argc, argv);

  g_object_unref (application);

  return status;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
