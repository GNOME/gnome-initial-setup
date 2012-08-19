/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GIS_UTILS_H__
#define __GIS_UTILS_H__

#include "gnome-initial-setup.h"

#include <act/act-user-manager.h>

G_BEGIN_DECLS

GtkBuilder * gis_builder (gchar *resource);
void gis_gtk_text_buffer_insert_pango_text (GtkTextBuffer *buffer,
                                            GtkTextIter *iter,
                                            PangoAttrList *attrlist,
                                            gchar *text);

G_END_DECLS

#endif /* __GIS_UTILS_H__ */
