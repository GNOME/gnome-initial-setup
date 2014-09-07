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

#ifndef __CC_LANGUAGE_CHOOSER_H__
#define __CC_LANGUAGE_CHOOSER_H__

#include <gtk/gtk.h>
#include <glib-object.h>

#define CC_TYPE_LANGUAGE_CHOOSER            (cc_language_chooser_get_type ())
#define CC_LANGUAGE_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_LANGUAGE_CHOOSER, CcLanguageChooser))
#define CC_LANGUAGE_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  CC_TYPE_LANGUAGE_CHOOSER, CcLanguageChooserClass))
#define CC_IS_LANGUAGE_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_LANGUAGE_CHOOSER))
#define CC_IS_LANGUAGE_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  CC_TYPE_LANGUAGE_CHOOSER))
#define CC_LANGUAGE_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  CC_TYPE_LANGUAGE_CHOOSER, CcLanguageChooserClass))

G_BEGIN_DECLS

typedef struct _CcLanguageChooser        CcLanguageChooser;
typedef struct _CcLanguageChooserClass   CcLanguageChooserClass;

struct _CcLanguageChooser
{
        GtkBox parent;
};

struct _CcLanguageChooserClass
{
        GtkBoxClass parent_class;

	void (*confirm) (CcLanguageChooser *chooser);
};

GType cc_language_chooser_get_type (void);

void          cc_language_chooser_clear_filter (CcLanguageChooser *chooser);
const gchar * cc_language_chooser_get_language (CcLanguageChooser *chooser);
void          cc_language_chooser_set_language (CcLanguageChooser *chooser,
                                                const gchar        *language);
gboolean      cc_language_chooser_get_showing_extra (CcLanguageChooser *chooser);

G_END_DECLS

#endif /* __CC_LANGUAGE_CHOOSER_H__ */
