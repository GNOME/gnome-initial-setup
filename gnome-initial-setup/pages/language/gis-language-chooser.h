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
 *     Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GIS_LANGUAGE_CHOOSER_H__
#define __GIS_LANGUAGE_CHOOSER_H__

#include <gtk/gtk.h>
#include <glib-object.h>

#define GIS_TYPE_LANGUAGE_CHOOSER            (gis_language_chooser_get_type ())
#define GIS_LANGUAGE_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_LANGUAGE_CHOOSER, GisLanguageChooser))
#define GIS_LANGUAGE_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_LANGUAGE_CHOOSER, GisLanguageChooserClass))
#define GIS_IS_LANGUAGE_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_LANGUAGE_CHOOSER))
#define GIS_IS_LANGUAGE_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_LANGUAGE_CHOOSER))
#define GIS_LANGUAGE_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_LANGUAGE_CHOOSER, GisLanguageChooserClass))

G_BEGIN_DECLS

typedef struct _GisLanguageChooser        GisLanguageChooser;
typedef struct _GisLanguageChooserClass   GisLanguageChooserClass;
typedef struct _GisLanguageChooserPrivate GisLanguageChooserPrivate;

struct _GisLanguageChooser
{
  GtkBin parent;

  GisLanguageChooserPrivate *priv;
};

struct _GisLanguageChooserClass
{
  GtkBinClass parent_class;
};

GType gis_language_chooser_get_type (void);

void          gis_language_chooser_clear_filter (GisLanguageChooser *chooser);
const gchar * gis_language_chooser_get_language (GisLanguageChooser *chooser);
void          gis_language_chooser_set_language (GisLanguageChooser *chooser,
                                                 const gchar        *language);
gboolean      gis_language_chooser_get_showing_extra (GisLanguageChooser *chooser);

G_END_DECLS

#endif /* __GIS_LANGUAGE_CHOOSER_H__ */
