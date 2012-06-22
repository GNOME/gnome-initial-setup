/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "gnome-initial-setup.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>

#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>

#include "um-utils.h"
#include "um-photo-dialog.h"
#include "pw-utils.h"

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#include <gnome-keyring.h>

#include "gis-assistant.h"

#include "gis-welcome-page.h"
#include "gis-eula-pages.h"
#include "gis-location-page.h"
#include "gis-network-page.h"
#include "gis-goa-page.h"
#include "gis-summary-page.h"

#include "gis-utils.h"

/* Setup data {{{1 */
struct _SetupData {
  GtkWindow *main_window;
  GKeyFile *overrides;
  GtkBuilder *builder;
  GisAssistant *assistant;

  ActUser *act_user;
};

#include "gis-account-page.c"

#define OBJ(type,name) ((type)gtk_builder_get_object(setup->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
copy_account_data (SetupData *setup)
{
        ActUser *user = setup->act_user;
        /* here is where we copy all the things we just
         * configured, from the current users home dir to the
         * account that was created in the first step
         */
        g_debug ("Copying account data");
        g_settings_sync ();

        gis_copy_account_file (user, ".config/dconf/user");
        gis_copy_account_file (user, ".config/goa-1.0/accounts.conf");
        gis_copy_account_file (user, ".gnome2/keyrings/Default.keyring");
}

static void
prepare_cb (GisAssistant *assi, GtkWidget *page, SetupData *setup)
{
        g_debug ("Preparing page %s", gtk_widget_get_name (page));

        if (page == WID("welcome-page")) {
                gtk_window_set_title (setup->main_window, _("Welcome"));
        }
        else if (page == WID("network-page")) {
                gtk_window_set_title (setup->main_window, _("Network"));
        }
        else if (page == WID("account-page")) {
                gtk_window_set_title (setup->main_window, _("Login"));
        }
        else if (page == WID("location-page")) {
                gtk_window_set_title (setup->main_window, _("Location"));
        }
        else if (page == WID("online-page")) {
                gtk_window_set_title (setup->main_window, _("Online Accounts"));
        }
        else if (page == WID("summary-page")) {
                copy_account_data (setup);
                gtk_window_set_title (setup->main_window, _("Thank You"));
        }
}

static void
recenter_window (GdkScreen *screen, SetupData *setup)
{
        gtk_window_set_position (setup->main_window, GTK_WIN_POS_CENTER_ALWAYS);
}

static void
prepare_main_window (SetupData *setup)
{
        setup->main_window = OBJ(GtkWindow*, "main-window");
        setup->assistant = OBJ(GisAssistant*, "assistant");

        g_signal_connect (gtk_widget_get_screen (GTK_WIDGET (setup->main_window)),
                          "monitors-changed", G_CALLBACK (recenter_window), setup);

        g_signal_connect (setup->assistant, "prepare",
                          G_CALLBACK (prepare_cb), setup);

        gis_prepare_welcome_page (setup);
        gis_prepare_eula_pages (setup);
        gis_prepare_network_page (setup);
        prepare_account_page (setup);
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

ActUser *
gis_get_act_user (SetupData *setup)
{
        return setup->act_user;
}

/* main {{{1 */

int
main (int argc, char *argv[])
{
        SetupData *setup;
        gchar *filename;
        GError *error;
        GOptionEntry entries[] = {
                { "skip-account", 0, 0, G_OPTION_ARG_NONE, &skip_account, "Skip account creation", NULL },
                { NULL, 0 }
        };

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#ifdef HAVE_CHEESE
        cheese_gtk_init (NULL, NULL);
#endif

        setup = g_new0 (SetupData, 1);

        gtk_init_with_args (&argc, &argv, "", entries, GETTEXT_PACKAGE, NULL);

        if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS) {
                g_critical ("Clutter-GTK init failed");
                exit (1);
        }

        error = NULL;
        if (g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error) == NULL) {
                g_error ("Couldn't get on session bus: %s", error->message);
                exit (1);
        };

        /* Make sure GisAssistant is initialized. */
        g_debug ("Registering: %s\n", g_type_name (gis_assistant_get_type ()));

        setup->builder = gis_builder ("setup");

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
