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
#define GIS_PAGE_HEADER(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_PAGE_HEADER, GisPageHeader))
#define GIS_PAGE_HEADER_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_PAGE_HEADER, GisPageHeaderClass))
#define GIS_IS_PAGE_HEADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_PAGE_HEADER))
#define GIS_IS_PAGE_HEADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_PAGE_HEADER))
#define GIS_PAGE_HEADER_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_PAGE_HEADER, GisPageHeaderClass))

typedef struct _GisPageHeader        GisPageHeader;
typedef struct _GisPageHeaderClass   GisPageHeaderClass;

struct _GisPageHeader
{
  GtkBox parent;
};

struct _GisPageHeaderClass
{
  GtkBoxClass parent_class;
};

GType gis_page_header_get_type (void);

G_END_DECLS

#endif /* __GIS_PAGE_HEADER_H__ */
