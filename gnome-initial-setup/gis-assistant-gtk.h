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

#ifndef __GIS_ASSISTANT_GTK_H__
#define __GIS_ASSISTANT_GTK_H__

#include <glib-object.h>

#include "gis-assistant.h"

G_BEGIN_DECLS

#define GIS_TYPE_ASSISTANT_GTK             (gis_assistant_gtk_get_type ())
#define GIS_ASSISTANT_GTK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_ASSISTANT_GTK, GisAssistantGtk))
#define GIS_ASSISTANT_GTK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_ASSISTANT_GTK, GisAssistantGtkClass))
#define GIS_IS_ASSISTANT_GTK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_ASSISTANT_GTK))
#define GIS_IS_ASSISTANT_GTK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),   GIS_TYPE_ASSISTANT_GTK))
#define GIS_ASSISTANT_GTK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_ASSISTANT_GTK, GisAssistantGtkClass))

typedef struct _GisAssistantGtk        GisAssistantGtk;
typedef struct _GisAssistantGtkClass   GisAssistantGtkClass;
typedef struct _GisAssistantGtkPrivate GisAssistantGtkPrivate;

struct _GisAssistantGtk
{
  GisAssistant parent;

  GisAssistantGtkPrivate *priv;
};

struct _GisAssistantGtkClass
{
  GisAssistantClass parent_class;
};

GType gis_assistant_gtk_get_type (void);

G_END_DECLS

#endif /* __GIS_ASSISTANT_GTK_H__ */
