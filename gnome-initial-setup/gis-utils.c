/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"
#include "gis-utils.h"

#include <string.h>

#include <stdlib.h>

void
gis_copy_account_file (ActUser     *act_user,
                       const gchar *relative_path)
{
        const gchar *username;
        const gchar *homedir;
        GSList *dirs = NULL, *l;
        gchar *p, *tmp;
        gchar *argv[20];
        gint i;
        gchar *from;
        gchar *to;
        GError *error = NULL;

        username = act_user_get_user_name (act_user);
        homedir = act_user_get_home_dir (act_user);

        from = g_build_filename (g_get_home_dir (), relative_path, NULL);
        to = g_build_filename (homedir, relative_path, NULL);

        p = g_path_get_dirname (relative_path);
        while (strcmp (p, ".") != 0) {
                dirs = g_slist_prepend (dirs, g_build_filename (homedir, p, NULL));
                tmp = g_path_get_dirname (p);
                g_free (p);
                p = tmp;
        }

        i = 0;
        argv[i++] = "/usr/bin/pkexec";
        argv[i++] = "install";
        argv[i++] = "--owner";
        argv[i++] = (gchar *)username;
        argv[i++] = "--group";
        argv[i++] = (gchar *)username;
        argv[i++] = "--mode";
        argv[i++] = "755";
        argv[i++] = "--directory";
        for (l = dirs; l; l = l->next) {
                argv[i++] = l->data;
                if (i == 20) {
                        g_warning ("Too many subdirectories");
                        goto out;
                }
        }
        argv[i++] = NULL;

        if (!g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error)) {
                g_warning ("Failed to copy account data: %s", error->message);
                g_error_free (error);
                goto out;
        }

        i = 0;
        argv[i++] = "/usr/bin/pkexec";
        argv[i++] = "install";
        argv[i++] = "--owner";
        argv[i++] = (gchar *)username;
        argv[i++] = "--group";
        argv[i++] = (gchar *)username;
        argv[i++] = "--mode";
        argv[i++] = "755";
        argv[i++] = from;
        argv[i++] = to;
        argv[i++] = NULL;

        if (!g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error)) {
                g_warning ("Failed to copy account data: %s", error->message);
                g_error_free (error);
                goto out;
        }

out:
        g_slist_free_full (dirs, g_free);
        g_free (to);
        g_free (from);
}

GtkBuilder *
gis_builder (gchar *resource)
{
    GtkBuilder *builder;
    gchar *resource_path = g_strdup_printf ("/ui/%s.ui", resource);
    GError *error = NULL;

    builder = gtk_builder_new ();
    gtk_builder_add_from_resource (builder, resource_path, &error);

    g_free (resource_path);

    if (error != NULL) {
        g_printerr ("%s", error->message);
        exit (1);
    }

    return builder;
}
