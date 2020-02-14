/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2020 Endless Mobile, Inc.
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
 *     Philip Withnall <withnall@endlessm.com>
 */

#pragma once

#include <glib.h>
#include <glib-object.h>

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_PARENTAL_CONTROLS_PAGE (gis_parental_controls_page_get_type ())
G_DECLARE_FINAL_TYPE (GisParentalControlsPage, gis_parental_controls_page, GIS, PARENTAL_CONTROLS_PAGE, GisPage)

GType gis_parental_controls_page_get_type (void);

GisPage *gis_prepare_parental_controls_page (GisDriver *driver);

G_END_DECLS
