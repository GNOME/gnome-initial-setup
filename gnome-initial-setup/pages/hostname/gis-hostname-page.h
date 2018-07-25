/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2018 Matthias Klumpp <matthias.klumpp@puri.sm>
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
 */

#ifndef __GIS_HOSTNAME_PAGE_H__
#define __GIS_HOSTNAME_PAGE_H__

#include <glib-object.h>

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_HOSTNAME_PAGE               (gis_hostname_page_get_type ())
#define GIS_HOSTNAME_PAGE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_HOSTNAME_PAGE, GisHostnamePage))
#define GIS_HOSTNAME_PAGE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_HOSTNAME_PAGE, GisHostnamePageClass))
#define GIS_IS_HOSTNAME_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_HOSTNAME_PAGE))
#define GIS_IS_HOSTNAME_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_HOSTNAME_PAGE))
#define GIS_HOSTNAME_PAGE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_HOSTNAME_PAGE, GisHostnamePageClass))

typedef struct _GisHostnamePage        GisHostnamePage;
typedef struct _GisHostnamePageClass   GisHostnamePageClass;

struct _GisHostnamePage
{
  GisPage parent;
};

struct _GisHostnamePageClass
{
  GisPageClass parent_class;
};

GType gis_hostname_page_get_type (void);

GisPage *gis_prepare_hostname_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_HOSTNAME_PAGE_H__ */
