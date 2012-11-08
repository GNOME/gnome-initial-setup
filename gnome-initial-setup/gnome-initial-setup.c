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

#include "gnome-initial-setup.h"

#include <stdlib.h>

#ifdef HAVE_CLUTTER
#include <clutter-gtk/clutter-gtk.h>
#endif

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#include "pages/language/gis-language-page.h"
#include "pages/eulas/gis-eula-pages.h"
#include "pages/location/gis-location-page.h"
#include "pages/account/gis-account-page.h"
#include "pages/network/gis-network-page.h"
#include "pages/goa/gis-goa-page.h"
#include "pages/summary/gis-summary-page.h"

/* main {{{1 */

static void
rebuild_pages_cb (GisDriver *driver)
{
  gis_prepare_language_page (driver);
  gis_prepare_eula_pages (driver);
  gis_prepare_network_page (driver);
  gis_prepare_account_page (driver);
  gis_prepare_location_page (driver);
  gis_prepare_goa_page (driver);
  gis_prepare_summary_page (driver);
}

int
main (int argc, char *argv[])
{
  GisDriver *driver;
  int status;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

#ifdef HAVE_CHEESE
  cheese_gtk_init (NULL, NULL);
#endif

  gtk_init (&argc, &argv);

#if HAVE_CLUTTER
  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS) {
    g_critical ("Clutter-GTK init failed");
    exit (1);
  }
#endif

  driver = gis_driver_new ();
  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);
  g_object_unref (driver);
  return status;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
