/*
 * Copyright (C) 2012 Intel, Inc
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Michael Wood <michael.g.wood@intel.com>
 *   Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Based on gnome-control-center region page by:
 *  Sergey Udaltsov <svu@gnome.org>
 *
 */

#define PAGE_ID "keyboard"

#include "config.h"
#include "keyboard-resources.h"
#include "gis-keyboard-page.h"

#include <gtk/gtk.h>

#include "gnome-region-panel-input.h"

G_DEFINE_TYPE (GisKeyboardPage, gis_keyboard_page, GIS_TYPE_PAGE)

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
gis_keyboard_page_constructed (GObject *object)
{
  GisKeyboardPage *page = GIS_KEYBOARD_PAGE (object);

  G_OBJECT_CLASS (gis_keyboard_page_parent_class)->constructed (object);

  setup_input_tabs (GIS_PAGE (page)->builder, GIS_KEYBOARD_PAGE (page));

  gtk_container_add (GTK_CONTAINER (page), WID("keyboard-page"));

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_keyboard_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Keyboard Layout"));
}

static void
gis_keyboard_page_class_init (GisKeyboardPageClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GisPageClass * page_class = GIS_PAGE_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_keyboard_page_locale_changed;
  object_class->constructed = gis_keyboard_page_constructed;
}

static void
gis_keyboard_page_init (GisKeyboardPage * self)
{
  g_resources_register (keyboard_get_resource ());
}

void
gis_prepare_keyboard_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_KEYBOARD_PAGE,
                                     "driver", driver,
                                     NULL));
}
