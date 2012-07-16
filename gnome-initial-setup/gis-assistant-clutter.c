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

#include "gis-assistant-clutter.h"
#include "gis-assistant-private.h"
#include "cc-notebook.h"

G_DEFINE_TYPE (GisAssistantClutter, gis_assistant_clutter, GIS_TYPE_ASSISTANT)

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_ASSISTANT_CLUTTER, GisAssistantClutterPrivate))

struct _GisAssistantClutterPrivate
{
  GtkWidget *notebook;
};

static void
current_page_changed (CcNotebook   *notebook,
                      GParamSpec   *pspec,
                      GisAssistant *assistant)
{
  GtkWidget *page = cc_notebook_get_selected_page (notebook);
  _gis_assistant_current_page_changed (assistant, page);
}

static void
gis_assistant_clutter_switch_to (GisAssistant *assistant, GtkWidget *widget)
{
  GisAssistantClutterPrivate *priv = GIS_ASSISTANT_CLUTTER (assistant)->priv;
  cc_notebook_select_page (CC_NOTEBOOK (priv->notebook), widget, TRUE);
}

static void
gis_assistant_clutter_add_page (GisAssistant *assistant,
                                GtkWidget    *page)
{
  GisAssistantClutterPrivate *priv = GIS_ASSISTANT_CLUTTER (assistant)->priv;
  cc_notebook_add_page (CC_NOTEBOOK (priv->notebook), page);
}

static void
gis_assistant_clutter_init (GisAssistantClutter *assistant_clutter)
{
  GisAssistantClutterPrivate *priv = GET_PRIVATE (assistant_clutter);
  GisAssistant *assistant = GIS_ASSISTANT (assistant_clutter);
  GtkWidget *frame;

  assistant_clutter->priv = priv;

  frame = _gis_assistant_get_frame (assistant);
  priv->notebook = cc_notebook_new ();
  gtk_container_add (GTK_CONTAINER (frame), priv->notebook);

  gtk_widget_show (priv->notebook);

  g_signal_connect (priv->notebook, "notify::current-page",
                    G_CALLBACK (current_page_changed), assistant);
}

static void
gis_assistant_clutter_class_init (GisAssistantClutterClass *klass)
{
  GisAssistantClass *assistant_class = GIS_ASSISTANT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GisAssistantClutterPrivate));

  assistant_class->add_page = gis_assistant_clutter_add_page;
  assistant_class->switch_to = gis_assistant_clutter_switch_to;
}
