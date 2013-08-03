/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Red Hat
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
 *     Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __GIS_CENTER_CONTAINER_H__
#define __GIS_CENTER_CONTAINER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIS_TYPE_CENTER_CONTAINER               (gis_center_container_get_type ())
#define GIS_CENTER_CONTAINER(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_CENTER_CONTAINER, GisCenterContainer))
#define GIS_CENTER_CONTAINER_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_CENTER_CONTAINER, GisCenterContainerClass))
#define GIS_IS_CENTER_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_CENTER_CONTAINER))
#define GIS_IS_CENTER_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_CENTER_CONTAINER))
#define GIS_CENTER_CONTAINER_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_CENTER_CONTAINER, GisCenterContainerClass))

typedef struct _GisCenterContainer        GisCenterContainer;
typedef struct _GisCenterContainerClass   GisCenterContainerClass;

struct _GisCenterContainer
{
    GtkContainer parent;
};

struct _GisCenterContainerClass
{
    GtkContainerClass parent_class;
};

GType gis_center_container_get_type (void);

GtkWidget * gis_center_container_new (GtkWidget *left,
                                      GtkWidget *center,
                                      GtkWidget *right);

G_END_DECLS

#endif /* __GIS_CENTER_CONTAINER_H__ */
