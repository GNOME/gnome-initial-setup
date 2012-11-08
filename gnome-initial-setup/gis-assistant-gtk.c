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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gis-assistant-gtk.h"
#include "gis-assistant-private.h"

G_DEFINE_TYPE (GisAssistantGtk, gis_assistant_gtk, GIS_TYPE_ASSISTANT)

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_ASSISTANT_GTK, GisAssistantGtkPrivate))

struct _GisAssistantGtkPrivate
{
  GtkWidget *notebook;
};

static void
current_page_changed (GtkNotebook  *notebook,
                      GtkWidget    *new_page,
                      gint          new_page_num,
                      GisAssistant *assistant)
{
  _gis_assistant_current_page_changed (assistant, new_page);
}

static void
gis_assistant_gtk_switch_to (GisAssistant *assistant,
                             GtkWidget    *widget)
{
  GisAssistantGtkPrivate *priv = GIS_ASSISTANT_GTK (assistant)->priv;
  gint page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), widget);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), page_num);
}

static void
gis_assistant_gtk_add_page (GisAssistant *assistant,
                            GtkWidget    *widget)
{
  GisAssistantGtkPrivate *priv = GIS_ASSISTANT_GTK (assistant)->priv;
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), widget, NULL);
}

static void
gis_assistant_gtk_init (GisAssistantGtk *assistant_gtk)
{
  GisAssistantGtkPrivate *priv = GET_PRIVATE (assistant_gtk);
  GisAssistant *assistant = GIS_ASSISTANT (assistant_gtk);
  GtkWidget *frame;

  assistant_gtk->priv = priv;

  frame = _gis_assistant_get_frame (assistant);
  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_container_add (GTK_CONTAINER (frame), priv->notebook);

  gtk_widget_show (priv->notebook);

  g_signal_connect (priv->notebook, "switch-page",
                    G_CALLBACK (current_page_changed), assistant);
}

static void
gis_assistant_gtk_class_init (GisAssistantGtkClass *klass)
{
  GisAssistantClass *assistant_class = GIS_ASSISTANT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GisAssistantGtkPrivate));

  assistant_class->add_page = gis_assistant_gtk_add_page;
  assistant_class->switch_to = gis_assistant_gtk_switch_to;
}
