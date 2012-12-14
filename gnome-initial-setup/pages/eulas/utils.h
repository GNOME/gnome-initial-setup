/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

gboolean splice_buffer (GInputStream  *stream,
                        GtkTextBuffer *buffer,
                        GError       **error);

void text_buffer_insert_pango_text (GtkTextBuffer *buffer,
                                    GtkTextIter *iter,
                                    PangoAttrList *attrlist,
                                    gchar *text);

G_END_DECLS

#endif /* __UTILS_H__ */
