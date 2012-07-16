/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* GIS_ASSISTANT_CLUTTER */
/* GisAssistantClutter */

#ifndef __GIS_ASSISTANT_CLUTTER_H__
#define __GIS_ASSISTANT_CLUTTER_H__

#include <glib-object.h>

#include "gis-assistant.h"

G_BEGIN_DECLS

#define GIS_TYPE_ASSISTANT_CLUTTER             (gis_assistant_clutter_get_type ())
#define GIS_ASSISTANT_CLUTTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_ASSISTANT_CLUTTER, GisAssistantClutter))
#define GIS_ASSISTANT_CLUTTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_ASSISTANT_CLUTTER, GisAssistantClutterClass))
#define GIS_IS_ASSISTANT_CLUTTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_ASSISTANT_CLUTTER))
#define GIS_IS_ASSISTANT_CLUTTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),   GIS_TYPE_ASSISTANT_CLUTTER))
#define GIS_ASSISTANT_CLUTTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_ASSISTANT_CLUTTER, GisAssistantClutterClass))

typedef struct _GisAssistantClutter        GisAssistantClutter;
typedef struct _GisAssistantClutterClass   GisAssistantClutterClass;
typedef struct _GisAssistantClutterPrivate GisAssistantClutterPrivate;

struct _GisAssistantClutter
{
  GisAssistant parent;

  GisAssistantClutterPrivate *priv;
};

struct _GisAssistantClutterClass
{
  GisAssistantClass parent_class;
};

GType gis_assistant_clutter_get_type (void);

G_END_DECLS

#endif /* __GIS_ASSISTANT_CLUTTER_H__ */
