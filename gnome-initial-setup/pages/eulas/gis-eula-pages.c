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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* EULA pages {{{1 */

#include "config.h"
#include "gis-eula-pages.h"
#include "gis-eula-page.h"

GisPage *
gis_prepare_eula_page (GisDriver *driver)
{
  gchar *eulas_dir_path;
  GFile *eulas_dir;
  GError *error = NULL;
  GFileEnumerator *enumerator = NULL;
  GFileInfo *info;
  GisPage *page = NULL;

  eulas_dir_path = g_build_filename (PKGDATADIR, "eulas", NULL);
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
    GFile *eula = g_file_enumerator_get_child (enumerator, info);

    page = g_object_new (GIS_TYPE_EULA_PAGE,
                         "driver", driver,
                         "eula", eula,
                         NULL);

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

  return page;
}
