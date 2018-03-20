/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2015 Red Hat
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

#ifndef __GIS_PRIVACY_PAGE_H__
#define __GIS_PRIVACY_PAGE_H__

#include <glib-object.h>

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_PRIVACY_PAGE               (gis_privacy_page_get_type ())
#define GIS_PRIVACY_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_PRIVACY_PAGE, GisPrivacyPage))
#define GIS_PRIVACY_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_PRIVACY_PAGE, GisPrivacyPageClass))
#define GIS_IS_PRIVACY_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_PRIVACY_PAGE))
#define GIS_IS_PRIVACY_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_PRIVACY_PAGE))
#define GIS_PRIVACY_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_PRIVACY_PAGE, GisPrivacyPageClass))

typedef struct _GisPrivacyPage        GisPrivacyPage;
typedef struct _GisPrivacyPageClass   GisPrivacyPageClass;

struct _GisPrivacyPage
{
  GisPage parent;
};

struct _GisPrivacyPageClass
{
  GisPageClass parent_class;
};

GType gis_privacy_page_get_type (void);

GisPage *gis_prepare_privacy_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_PRIVACY_PAGE_H__ */
