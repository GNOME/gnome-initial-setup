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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __GIS_BUBBLE_WIDGET_H__
#define __GIS_BUBBLE_WIDGET_H__

#include <adwaita.h>

G_BEGIN_DECLS

#define GIS_TYPE_BUBBLE_WIDGET               (gis_bubble_widget_get_type ())
#define GIS_BUBBLE_WIDGET(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_BUBBLE_WIDGET, GisBubbleWidget))
#define GIS_BUBBLE_WIDGET_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_BUBBLE_WIDGET, GisBubbleWidgetClass))
#define GIS_IS_BUBBLE_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_BUBBLE_WIDGET))
#define GIS_IS_BUBBLE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_BUBBLE_WIDGET))
#define GIS_BUBBLE_WIDGET_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_BUBBLE_WIDGET, GisBubbleWidgetClass))

typedef struct _GisBubbleWidget        GisBubbleWidget;
typedef struct _GisBubbleWidgetClass   GisBubbleWidgetClass;

struct _GisBubbleWidget
{
    AdwBin parent;
};

struct _GisBubbleWidgetClass
{
    AdwBinClass parent_class;
};

GType gis_bubble_widget_get_type (void);

G_END_DECLS

#endif /* __GIS_BUBBLE_WIDGET_H__ */
