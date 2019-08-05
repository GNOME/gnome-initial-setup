/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* -*- encoding: utf8 -*- */
/*
 * Copyright (C) 2019 Purism SPC
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
 *     Adrien Plazas <kekun.plazas@laposte.net>
 */

#ifndef __GIS_PAGE_HEADER_H__
#define __GIS_PAGE_HEADER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIS_TYPE_PAGE_HEADER               (gis_page_header_get_type ())

G_DECLARE_FINAL_TYPE (GisPageHeader, gis_page_header, GIS, PAGE_HEADER, GtkBox)

G_END_DECLS

#endif /* __GIS_PAGE_HEADER_H__ */
