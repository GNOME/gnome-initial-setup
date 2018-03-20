/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
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
 */

#ifndef __GIS_PAGE_H__
#define __GIS_PAGE_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_PAGE               (gis_page_get_type ())
#define GIS_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_PAGE, GisPage))
#define GIS_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_PAGE, GisPageClass))
#define GIS_IS_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_PAGE))
#define GIS_IS_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_PAGE))
#define GIS_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_PAGE, GisPageClass))

typedef struct _GisPage        GisPage;
typedef struct _GisPageClass   GisPageClass;
typedef struct _GisAssistantPagePrivate GisAssistantPagePrivate;

typedef void (* GisPageApplyCallback) (GisPage *page,
                                       gboolean valid,
                                       gpointer user_data);

struct _GisPage
{
  GtkBin parent;

  GisDriver *driver;

  GisAssistantPagePrivate *assistant_priv;
};

struct _GisPageClass
{
  GtkBinClass parent_class;
  char *page_id;

  void         (*locale_changed) (GisPage *page);
  gboolean     (*apply) (GisPage *page,
                         GCancellable *cancellable);
  void         (*save_data) (GisPage *page);
  void         (*shown) (GisPage *page);
  gboolean     (*skip) (GisPage *page);
};

GType gis_page_get_type (void);

char *       gis_page_get_title (GisPage *page);
void         gis_page_set_title (GisPage *page, char *title);
gboolean     gis_page_get_complete (GisPage *page);
void         gis_page_set_complete (GisPage *page, gboolean complete);
gboolean     gis_page_get_skippable (GisPage *page);
void         gis_page_set_skippable (GisPage *page, gboolean skippable);
gboolean     gis_page_get_needs_accept (GisPage *page);
void         gis_page_set_needs_accept (GisPage *page, gboolean needs_accept);
GtkWidget *  gis_page_get_action_widget (GisPage *page);
void         gis_page_locale_changed (GisPage *page);
void         gis_page_apply_begin (GisPage *page, GisPageApplyCallback callback, gpointer user_data);
void         gis_page_apply_cancel (GisPage *page);
void         gis_page_apply_complete (GisPage *page, gboolean valid);
gboolean     gis_page_get_applying (GisPage *page);
void         gis_page_save_data (GisPage *page);
void         gis_page_shown (GisPage *page);
gboolean     gis_page_skip (GisPage *page);

G_END_DECLS

#endif /* __GIS_PAGE_H__ */
