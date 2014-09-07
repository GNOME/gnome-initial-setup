/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *     Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __CC_REGION_CHOOSER_H__
#define __CC_REGION_CHOOSER_H__

#include <gtk/gtk.h>
#include <glib-object.h>

#define CC_TYPE_REGION_CHOOSER            (cc_region_chooser_get_type ())
#define CC_REGION_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_REGION_CHOOSER, CcRegionChooser))
#define CC_REGION_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  CC_TYPE_REGION_CHOOSER, CcRegionChooserClass))
#define CC_IS_REGION_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_REGION_CHOOSER))
#define CC_IS_REGION_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  CC_TYPE_REGION_CHOOSER))
#define CC_REGION_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  CC_TYPE_REGION_CHOOSER, CcRegionChooserClass))

G_BEGIN_DECLS

typedef struct _CcRegionChooser        CcRegionChooser;
typedef struct _CcRegionChooserClass   CcRegionChooserClass;

struct _CcRegionChooser
{
        GtkBox parent;
};

struct _CcRegionChooserClass
{
        GtkBoxClass parent_class;

        void (*confirm) (CcRegionChooser *chooser);
};

GType cc_region_chooser_get_type (void);

void          cc_region_chooser_clear_filter (CcRegionChooser *chooser);
const gchar * cc_region_chooser_get_locale   (CcRegionChooser *chooser);
void          cc_region_chooser_set_locale   (CcRegionChooser *chooser,
                                              const gchar     *locale);
gboolean      cc_region_chooser_get_showing_extra (CcRegionChooser *chooser);
gint          cc_region_chooser_get_n_regions (CcRegionChooser *chooser);

G_END_DECLS

#endif /* __CC_REGION_CHOOSER_H__ */
