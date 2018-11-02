/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2018 Red Hat
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
 */

#include "config.h"

#include "gis-account-page-offline.h"
#include "gnome-initial-setup.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

struct _GisAccountPageOfflinePrivate
{
  GtkWidget *image;
};
typedef struct _GisAccountPageOfflinePrivate GisAccountPageOfflinePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisAccountPageOffline, gis_account_page_offline, GTK_TYPE_BIN);

static void
gis_account_page_offline_realize (GtkWidget *widget)
{
  GisAccountPageOffline *page = GIS_ACCOUNT_PAGE_OFFLINE (widget);
  GisAccountPageOfflinePrivate *priv = gis_account_page_offline_get_instance_private (page);
  GtkWidget *gis_page;

  gis_page = gtk_widget_get_ancestor (widget, GIS_TYPE_PAGE);
  g_object_bind_property (gis_page, "small-screen",
                          priv->image, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  GTK_WIDGET_CLASS (gis_account_page_offline_parent_class)->realize (widget);
}

static void
gis_account_page_offline_class_init (GisAccountPageOfflineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->realize = gis_account_page_offline_realize;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page-offline.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageOffline, image);
}

static void
gis_account_page_offline_init (GisAccountPageOffline *page)
{
  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_account_page_offline_shown (GisAccountPageOffline *page)
{

}
