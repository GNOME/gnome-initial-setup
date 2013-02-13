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

/* Welcome page {{{1 */

#define PAGE_ID "welcome"

#include "config.h"
#include "gis-welcome-page.h"
#include "welcome-resources.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

G_DEFINE_TYPE (GisWelcomePage, gis_welcome_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
gis_welcome_page_constructed (GObject *object)
{
  GisWelcomePage *page = GIS_WELCOME_PAGE (object);

  G_OBJECT_CLASS (gis_welcome_page_parent_class)->constructed (object);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_container_add (GTK_CONTAINER (page), WID ("welcome-page"));

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_welcome_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Welcome"));
}

static void
gis_welcome_page_class_init (GisWelcomePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_welcome_page_locale_changed;
  object_class->constructed = gis_welcome_page_constructed;
}

static void
gis_welcome_page_init (GisWelcomePage *page)
{
  g_resources_register (welcome_get_resource ());
}

void
gis_prepare_welcome_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_WELCOME_PAGE,
                                     "driver", driver,
                                     NULL));
}
