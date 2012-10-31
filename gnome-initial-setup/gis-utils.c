/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"
#include "gis-utils.h"

#include <string.h>

#include <stdlib.h>

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
