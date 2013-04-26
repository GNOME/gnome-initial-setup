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
#include <libgd/gd.h>

#include "gis-assistant-gd.h"
#include "gis-assistant-private.h"

G_DEFINE_TYPE (GisAssistantGd, gis_assistant_gd, GIS_TYPE_ASSISTANT)

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_ASSISTANT_GD, GisAssistantGdPrivate))

struct _GisAssistantGdPrivate
{
  GtkWidget *stack;
};

static void
current_page_changed (GObject    *gobject,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GdStack *stack = GD_STACK (gobject);
  GisAssistant *assistant = GIS_ASSISTANT (user_data);
  GtkWidget *new_page = gd_stack_get_visible_child (stack);
  _gis_assistant_current_page_changed (assistant, GIS_PAGE (new_page));
}

static void
gis_assistant_gd_switch_to (GisAssistant          *assistant,
                            GisAssistantDirection  direction,
                            GisPage               *page)
{
  GisAssistantGdPrivate *priv = GIS_ASSISTANT_GD (assistant)->priv;
  GdStackTransitionType transition_type;

  switch (direction) {
  case GIS_ASSISTANT_NEXT:
    transition_type = GD_STACK_TRANSITION_TYPE_SLIDE_LEFT;
    break;
  case GIS_ASSISTANT_PREV:
    transition_type = GD_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
    break;
  default:
    g_assert_not_reached ();
  }

  gd_stack_set_transition_type (GD_STACK (priv->stack), transition_type);

  gd_stack_set_visible_child (GD_STACK (priv->stack),
                              GTK_WIDGET (page));
}

static void
gis_assistant_gd_add_page (GisAssistant *assistant,
                           GisPage      *page)
{
  GisAssistantGdPrivate *priv = GIS_ASSISTANT_GD (assistant)->priv;
  gtk_container_add (GTK_CONTAINER (priv->stack), GTK_WIDGET (page));
}

static void
gis_assistant_gd_init (GisAssistantGd *assistant_gd)
{
  GisAssistantGdPrivate *priv = GET_PRIVATE (assistant_gd);
  GisAssistant *assistant = GIS_ASSISTANT (assistant_gd);
  GtkWidget *frame;

  assistant_gd->priv = priv;

  frame = _gis_assistant_get_frame (assistant);
  priv->stack = gd_stack_new ();
  gd_stack_set_transition_type (GD_STACK (priv->stack),
                                GD_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_container_add (GTK_CONTAINER (frame), priv->stack);

  gtk_widget_show (priv->stack);

  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (current_page_changed), assistant);
}

static void
gis_assistant_gd_class_init (GisAssistantGdClass *klass)
{
  GisAssistantClass *assistant_class = GIS_ASSISTANT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GisAssistantGdPrivate));

  assistant_class->add_page = gis_assistant_gd_add_page;
  assistant_class->switch_to = gis_assistant_gd_switch_to;
}
