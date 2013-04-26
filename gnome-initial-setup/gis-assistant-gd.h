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
 */

#ifndef __GIS_ASSISTANT_GD_H__
#define __GIS_ASSISTANT_GD_H__

#include <glib-object.h>

#include "gis-assistant.h"

G_BEGIN_DECLS

#define GIS_TYPE_ASSISTANT_GD             (gis_assistant_gd_get_type ())
#define GIS_ASSISTANT_GD(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_ASSISTANT_GD, GisAssistantGd))
#define GIS_ASSISTANT_GD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_ASSISTANT_GD, GisAssistantGdClass))
#define GIS_IS_ASSISTANT_GD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_ASSISTANT_GD))
#define GIS_IS_ASSISTANT_GD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),   GIS_TYPE_ASSISTANT_GD))
#define GIS_ASSISTANT_GD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_ASSISTANT_GD, GisAssistantGdClass))

typedef struct _GisAssistantGd        GisAssistantGd;
typedef struct _GisAssistantGdClass   GisAssistantGdClass;
typedef struct _GisAssistantGdPrivate GisAssistantGdPrivate;

struct _GisAssistantGd
{
  GisAssistant parent;

  GisAssistantGdPrivate *priv;
};

struct _GisAssistantGdClass
{
  GisAssistantClass parent_class;
};

GType gis_assistant_gd_get_type (void);

G_END_DECLS

#endif /* __GIS_ASSISTANT_GD_H__ */
