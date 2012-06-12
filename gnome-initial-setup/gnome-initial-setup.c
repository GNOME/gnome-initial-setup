/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "gnome-initial-setup.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>
#include <gtk/gtk.h>

#include <act/act-user-manager.h>

#include "um-utils.h"
#include "um-photo-dialog.h"
#include "pw-utils.h"
#include "gdm-greeter-client.h"

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#include <gnome-keyring.h>

#include "gis-welcome-page.h"
#include "gis-eula-pages.h"
#include "gis-location-page.h"
#include "gis-network-page.h"
#include "gis-goa-page.h"

/* Setup data {{{1 */
struct _SetupData {
        GtkBuilder *builder;
        GtkAssistant *assistant;

        GKeyFile *overrides;

        GdmGreeterClient *greeter_client;

        /* account data */
        ActUserManager *act_client;
        ActUser *act_user;

        gboolean valid_name;
        gboolean valid_username;
        gboolean valid_password;
        const gchar *password_reason;
        ActUserPasswordMode password_mode;
        ActUserAccountType account_type;

        gboolean user_data_unsaved;

        GtkWidget *photo_dialog;
        GdkPixbuf *avatar_pixbuf;
        gchar *avatar_filename;
};

#include "gis-account-page.c"
#include "gis-summary-page.c"

static void
prepare_cb (GtkAssistant *assi, GtkWidget *page, SetupData *setup)
{
        g_debug ("Preparing page %s", gtk_widget_get_name (page));

        save_account_data (setup);

        if (page == WID("summary-page"))
                copy_account_data (setup);
}

static void
prepare_assistant (SetupData *setup)
{
        GList *list;

        setup->assistant = OBJ(GtkAssistant*, "gnome-setup-assistant");

        /* small hack to get rid of cancel button */
        gtk_assistant_commit (setup->assistant);

        /* another small hack to hide the sidebar */
        list = gtk_container_get_children (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (setup->assistant))));
        gtk_widget_hide (GTK_WIDGET (list->data));
        g_list_free (list);

        g_signal_connect (G_OBJECT (setup->assistant), "prepare",
                          G_CALLBACK (prepare_cb), setup);

        /* connect to gdm slave */
        connect_to_slave (setup);

        gis_prepare_welcome_page (setup);
        gis_prepare_eula_pages (setup);
        gis_prepare_network_page (setup);
        prepare_account_page (setup);
        gis_prepare_location_page (setup);
        gis_prepare_online_page (setup);
        prepare_summary_page (setup);
}

GKeyFile *
gis_get_overrides (SetupData *data)
{
        return g_key_file_ref (data->overrides);
}

GtkBuilder *
gis_get_builder (SetupData *data)
{
        return data->builder;
}

GtkAssistant *
gis_get_assistant (SetupData *data)
{
        return data->assistant;
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

        error = NULL;
        if (g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error) == NULL) {
                g_error ("Couldn't get on session bus: %s", error->message);
                exit (1);
        };

        setup->builder = gtk_builder_new ();
        if (g_file_test ("setup.ui", G_FILE_TEST_EXISTS)) {
                gtk_builder_add_from_file (setup->builder, "setup.ui", &error);
        }
        else if (!gtk_builder_add_from_resource (setup->builder, "/ui/setup.ui", &error)) {
                g_error ("%s", error->message);
                exit (1);
        }

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

        prepare_assistant (setup);

        gtk_window_present (GTK_WINDOW (setup->assistant));

        gtk_main ();

        return 0;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
