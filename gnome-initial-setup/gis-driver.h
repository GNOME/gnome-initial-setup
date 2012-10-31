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

#ifndef __GIS_DRIVER_H__
#define __GIS_DRIVER_H__

#include "gis-assistant.h"
#include "gis-page.h"
#include <act/act-user-manager.h>

G_BEGIN_DECLS

#define GIS_TYPE_DRIVER               (gis_driver_get_type ())
#define GIS_DRIVER(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_DRIVER, GisDriver))
#define GIS_DRIVER_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_DRIVER, GisDriverClass))
#define GIS_IS_DRIVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_DRIVER))
#define GIS_IS_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_DRIVER))
#define GIS_DRIVER_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_DRIVER, GisDriverClass))

typedef struct _GisDriver        GisDriver;
typedef struct _GisDriverClass   GisDriverClass;
typedef struct _GisDriverPrivate GisDriverPrivate;

struct _GisDriver
{
  GtkApplication parent;

  GisDriverPrivate *priv;
};

struct _GisDriverClass
{
  GtkApplicationClass parent_class;

  void (* rebuild_pages) (GisDriver *driver);
};

GType gis_driver_get_type (void);

GisAssistant *gis_driver_get_assistant (GisDriver *driver);
void gis_driver_locale_changed (GisDriver *driver);

void gis_driver_set_user_permissions (GisDriver   *driver,
                                      ActUser     *user,
                                      const gchar *password);

void gis_driver_get_user_permissions (GisDriver    *driver,
                                      ActUser     **user,
                                      const gchar **password);
void gis_driver_add_page (GisDriver *driver,
                          GisPage   *page);
GisDriver *gis_driver_new (void);

G_END_DECLS

#endif /* __GIS_DRIVER_H__ */
