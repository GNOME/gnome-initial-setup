/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2018 Red Hat
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

#ifndef __GIS_ACCOUNT_PAGE_OFFLINE_H__
#define __GIS_ACCOUNT_PAGE_OFFLINE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIS_TYPE_ACCOUNT_PAGE_OFFLINE            (gis_account_page_offline_get_type ())
#define GIS_ACCOUNT_PAGE_OFFLINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_ACCOUNT_PAGE_OFFLINE, GisAccountPageOffline))
#define GIS_ACCOUNT_PAGE_OFFLINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_ACCOUNT_PAGE_OFFLINE, GisAccountPageOfflineClass))
#define GIS_IS_ACCOUNT_PAGE_OFFLINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_ACCOUNT_PAGE_OFFLINE))
#define GIS_IS_ACCOUNT_PAGE_OFFLINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_ACCOUNT_PAGE_OFFLINE))
#define GIS_ACCOUNT_PAGE_OFFLINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_ACCOUNT_PAGE_OFFLINE, GisAccountPageOfflineClass))

typedef struct _GisAccountPageOffline        GisAccountPageOffline;
typedef struct _GisAccountPageOfflineClass   GisAccountPageOfflineClass;

struct _GisAccountPageOffline
{
    GtkBin parent;
};

struct _GisAccountPageOfflineClass
{
    GtkBinClass parent_class;
};

GType gis_account_page_offline_get_type (void);

void     gis_account_page_offline_shown (GisAccountPageOffline *offline);

G_END_DECLS

#endif /* __GIS_ACCOUNT_PAGE_OFFLINE_H__ */
