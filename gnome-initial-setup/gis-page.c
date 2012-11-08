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

#include "config.h"
#include "gis-page.h"

#include <string.h>

#include <stdlib.h>

GtkBuilder *
gis_builder (gchar *page_id)
{
  GtkBuilder *builder;
  gchar *resource_path = g_strdup_printf ("/ui/gis-%s-page.ui", page_id);
  GError *error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, resource_path, &error);

  g_free (resource_path);

  if (error != NULL) {
    g_warning ("Error while loading %s: %s", resource_path, error->message);
    exit (1);
  }

  return builder;
}
