/*
 * Copyright (C) 2013 Red Hat, Inc
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
 *     Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GIS_INPUT_CHOOSER_H__
#define __GIS_INPUT_CHOOSER_H__

#include <gtk/gtk.h>
#include <glib-object.h>

#define CC_TYPE_INPUT_CHOOSER            (cc_input_chooser_get_type ())
#define CC_INPUT_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_INPUT_CHOOSER, CcInputChooser))
#define CC_INPUT_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  CC_TYPE_INPUT_CHOOSER, CcInputChooserClass))
#define CC_IS_INPUT_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_INPUT_CHOOSER))
#define CC_IS_INPUT_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  CC_TYPE_INPUT_CHOOSER))
#define CC_INPUT_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  CC_TYPE_INPUT_CHOOSER, CcInputChooserClass))

G_BEGIN_DECLS

typedef struct _CcInputChooser        CcInputChooser;
typedef struct _CcInputChooserClass   CcInputChooserClass;

struct _CcInputChooser
{
        GtkBox parent;
};

struct _CcInputChooserClass
{
        GtkBoxClass parent_class;
};

GType cc_input_chooser_get_type (void);

void          cc_input_chooser_clear_filter (CcInputChooser *chooser);
const gchar * cc_input_chooser_get_input_id (CcInputChooser  *chooser);
const gchar * cc_input_chooser_get_input_type (CcInputChooser  *chooser);
void          cc_input_chooser_set_input (CcInputChooser *chooser,
                                          const gchar    *id,
                                          const gchar    *type);
void	      cc_input_chooser_get_layout (CcInputChooser *chooser,
					   const gchar    **layout,
					   const gchar    **variant);
gboolean      cc_input_chooser_get_showing_extra (CcInputChooser *chooser);

G_END_DECLS

#endif /* __GIS_INPUT_CHOOSER_H__ */
