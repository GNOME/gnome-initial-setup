/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* GIS_ASSISTANT */
/* GisAssistant */

#ifndef __GIS_ASSISTANT_H__
#define __GIS_ASSISTANT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GIS_TYPE_ASSISTANT               (gis_assistant_get_type ())
#define GIS_ASSISTANT(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_ASSISTANT, GisAssistant))
#define GIS_ASSISTANT_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_ASSISTANT, GisAssistantClass))
#define GIS_IS_ASSISTANT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_ASSISTANT))
#define GIS_IS_ASSISTANT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_ASSISTANT))
#define GIS_ASSISTANT_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_ASSISTANT, GisAssistantClass))

typedef struct _GisAssistant        GisAssistant;
typedef struct _GisAssistantClass   GisAssistantClass;
typedef struct _GisAssistantPrivate GisAssistantPrivate;

struct _GisAssistant
{
  GtkBox parent;

  GisAssistantPrivate *priv;
};

struct _GisAssistantClass
{
  GtkBoxClass parent_class;

  void (* prepare) (GisAssistant *assistant, GtkWidget *page);
  void (* switch_to) (GisAssistant *assistant, GtkWidget *page);
  void (* add_page) (GisAssistant *assistant, GtkWidget *page);
};

GType gis_assistant_get_type (void);

void      gis_assistant_add_page          (GisAssistant *assistant,
                                           GtkWidget    *page);

void      gis_assistant_next_page         (GisAssistant *assistant);
void      gis_assistant_previous_page     (GisAssistant *assistant);

void      gis_assistant_set_page_complete (GisAssistant *assistant,
                                           GtkWidget    *page,
                                           gboolean      complete);
gboolean  gis_assistant_get_page_complete (GisAssistant *assistant,
                                           GtkWidget    *page);

void      gis_assistant_set_page_title    (GisAssistant *assistant,
                                           GtkWidget    *page,
                                           gchar        *title);
gchar *   gis_assistant_get_page_title    (GisAssistant *assistant,
                                           GtkWidget    *page);
gchar *   gis_assistant_get_title         (GisAssistant *assistant);

G_END_DECLS

#endif /* __GIS_ASSISTANT_H__ */
