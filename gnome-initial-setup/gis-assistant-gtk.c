/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
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

struct _GisAssistantGtkPrivate
{
  GtkWidget *stack;
};
typedef struct _GisAssistantGtkPrivate GisAssistantGtkPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisAssistantGtk, gis_assistant_gtk, GIS_TYPE_ASSISTANT)

static void
current_page_changed (GObject    *gobject,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GtkStack *stack = GTK_STACK (gobject);
  GisAssistant *assistant = GIS_ASSISTANT (user_data);
  GtkWidget *new_page = gtk_stack_get_visible_child (stack);
  _gis_assistant_current_page_changed (assistant, GIS_PAGE (new_page));
}

static void
gis_assistant_gtk_switch_to (GisAssistant          *assistant,
                             GisAssistantDirection  direction,
                             GisPage               *page)
{
  GisAssistantGtkPrivate *priv = gis_assistant_gtk_get_instance_private (GIS_ASSISTANT_GTK (assistant));
  gtk_stack_set_visible_child (GTK_STACK (priv->stack), GTK_WIDGET (page));
}

static void
gis_assistant_gtk_add_page (GisAssistant *assistant,
                            GisPage      *page)
{
  GisAssistantGtkPrivate *priv = gis_assistant_gtk_get_instance_private (GIS_ASSISTANT_GTK (assistant));
  gtk_container_add (GTK_CONTAINER (priv->stack), GTK_WIDGET (page));
}

static void
gis_assistant_gtk_init (GisAssistantGtk *assistant_gtk)
{
  GisAssistant *assistant = GIS_ASSISTANT (assistant_gtk);
  GisAssistantGtkPrivate *priv = gis_assistant_gtk_get_instance_private (assistant_gtk);
  GtkWidget *frame;

  frame = _gis_assistant_get_frame (assistant);
  priv->stack = gtk_stack_new ();
  gtk_stack_set_transition_type (GTK_STACK (priv->stack),
                                 GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  gtk_container_add (GTK_CONTAINER (frame), priv->stack);

  gtk_widget_show (priv->stack);

  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (current_page_changed), assistant);
}

static void
gis_assistant_gtk_class_init (GisAssistantGtkClass *klass)
{
  GisAssistantClass *assistant_class = GIS_ASSISTANT_CLASS (klass);

  assistant_class->add_page = gis_assistant_gtk_add_page;
  assistant_class->switch_to = gis_assistant_gtk_switch_to;
}
