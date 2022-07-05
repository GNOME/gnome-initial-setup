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

#pragma once

#include "gis-page.h"

G_BEGIN_DECLS

#define GIS_TYPE_ASSISTANT               (gis_assistant_get_type ())

G_DECLARE_FINAL_TYPE (GisAssistant, gis_assistant, GIS, ASSISTANT, GtkBox)

void      gis_assistant_add_page          (GisAssistant *assistant,
                                           GisPage      *page);
void      gis_assistant_remove_page       (GisAssistant *assistant,
                                           GisPage      *page);

void      gis_assistant_next_page         (GisAssistant *assistant);
void      gis_assistant_previous_page     (GisAssistant *assistant);
GisPage * gis_assistant_get_current_page  (GisAssistant *assistant);
GList   * gis_assistant_get_all_pages     (GisAssistant *assistant);
const gchar *gis_assistant_get_title      (GisAssistant *assistant);
GtkWidget *gis_assistant_get_titlebar     (GisAssistant *assistant);

void      gis_assistant_locale_changed    (GisAssistant *assistant);
gboolean  gis_assistant_save_data         (GisAssistant  *assistant,
                                           GError       **error);

G_END_DECLS
