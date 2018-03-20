/*
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Sergey Udaltsov <svu@gnome.org>
 *
 */


#ifndef _GIS_KEYBOARD_PAGE_H
#define _GIS_KEYBOARD_PAGE_H

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_KEYBOARD_PAGE gis_keyboard_page_get_type()

#define GIS_KEYBOARD_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  GIS_TYPE_KEYBOARD_PAGE, GisKeyboardPage))

#define GIS_KEYBOARD_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  GIS_TYPE_KEYBOARD_PAGE, GisKeyboardPageClass))

#define GIS_IS_KEYBOARD_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  GIS_TYPE_KEYBOARD_PAGE))

#define GIS_IS_KEYBOARD_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  GIS_TYPE_KEYBOARD_PAGE))

#define GIS_KEYBOARD_PAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  GIS_TYPE_KEYBOARD_PAGE, GisKeyboardPageClass))

typedef struct _GisKeyboardPage GisKeyboardPage;
typedef struct _GisKeyboardPageClass GisKeyboardPageClass;

struct _GisKeyboardPage
{
  GisPage parent;
};

struct _GisKeyboardPageClass
{
  GisPageClass parent_class;
};

GType gis_keyboard_page_get_type (void) G_GNUC_CONST;

GisPage *gis_prepare_keyboard_page (GisDriver *driver);

G_END_DECLS

#endif /* _GIS_KEYBOARD_PAGE_H */
