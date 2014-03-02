/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2014 Red Hat
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
 *     Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GIS_PROMPT_H__
#define __GIS_PROMPT_H__

#include <glib-object.h>

G_BEGIN_DECLS

GType   gis_prompt_get_type       (void) G_GNUC_CONST;
#define GIS_TYPE_PROMPT            (gis_prompt_get_type ())
#define GIS_PROMPT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_PROMPT, GisPrompt))
#define GIS_IS_PROMPT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_PROMPT))
#define GIS_IS_PROMPT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIS_TYPE_PROMPT))
#define GIS_PROMPT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIS_TYPE_PROMPT, GisPromptClass))
#define GIS_PROMPT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIS_TYPE_PROMPT, GisPromptClass))

typedef struct _GisPrompt GisPrompt;
typedef struct _GisPromptClass GisPromptClass;

struct _GisPrompt {
	GObject parent;
	GHashTable *properties;
};

struct _GisPromptClass {
	GObjectClass parent_class;
};

G_END_DECLS

#endif /* __GIS_PROMPT_H__ */
