/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

gboolean splice_buffer_text (GInputStream  *stream,
                             GtkTextBuffer *buffer,
                             GError       **error);

gboolean splice_buffer_markup (GInputStream  *stream,
                               GtkTextBuffer *buffer,
                               GError       **error);

G_END_DECLS

#endif /* __UTILS_H__ */
