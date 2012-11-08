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

#include "gis-assistant.h"
#include "gis-assistant-private.h"

G_DEFINE_TYPE (GisAssistant, gis_assistant, GTK_TYPE_BOX)

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_ASSISTANT, GisAssistantPrivate))

enum {
  PROP_0,
  PROP_TITLE,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

enum {
  PREPARE,
  NEXT_PAGE,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

typedef struct _PageData PageData;

struct _GisAssistantPrivate
{
  GtkWidget *frame;
  GtkWidget *forward;
  GtkWidget *back;
  GtkWidget *main_layout;
  GtkWidget *action_area;

  GList *pages;
  PageData *current_page;
};

struct _PageData
{
  GisAssistant *assistant;

  GtkWidget *widget;
  gboolean page_complete : 1;
  gboolean use_arrow_buttons : 1;
  guint padding : 6;

  GList *link;
  gchar *title;
};

static PageData *
get_page_data_for_page (GtkWidget *page)
{
  return (PageData *) g_object_get_data (G_OBJECT (page), "gis-assistant-page-data");
}

static void
remove_page_data (PageData *page_data)
{
  g_object_set_data (G_OBJECT (page_data->widget), "gis-assistant-page-data", NULL);
}

static void
free_page_data (PageData *page_data)
{
  GisAssistantPrivate *priv = page_data->assistant->priv;

  priv->pages = g_list_delete_link (priv->pages, page_data->link);
  if (page_data == priv->current_page)
    priv->current_page = NULL;

  g_free (page_data->title);
  g_slice_free (PageData, page_data);
}

static void
widget_destroyed (GtkWidget *widget,
                  PageData  *page_data)
{
  remove_page_data (page_data);
}

static PageData *
create_page_data_for_page (GisAssistant *assistant,
                           GtkWidget    *page)
{
  PageData *page_data = g_slice_new0 (PageData);
  page_data->widget = page;
  page_data->assistant = assistant;

  g_signal_connect (page, "destroy", G_CALLBACK (widget_destroyed), page_data);
  g_object_set_data_full (G_OBJECT (page), "gis-assistant-page-data",
                          page_data, (GDestroyNotify) free_page_data);

  return page_data;
}

static void
gis_assistant_switch_to (GisAssistant *assistant, GtkWidget *widget)
{
  GIS_ASSISTANT_GET_CLASS (assistant)->switch_to (assistant, widget);
}

void
gis_assistant_next_page (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = assistant->priv;
  g_signal_emit (assistant, signals[NEXT_PAGE], 0,
                 priv->current_page->widget);
}

static void
gis_assistant_real_next_page (GisAssistant *assistant,
                              GtkWidget    *leaving_page)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *next_page;

  g_return_if_fail (priv->current_page != NULL);

  next_page = (PageData *) priv->current_page->link->next->data;
  gis_assistant_switch_to (assistant, next_page->widget);
}

void
gis_assistant_previous_page (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *prev_page;

  g_return_if_fail (priv->current_page != NULL);

  prev_page = (PageData *) priv->current_page->link->prev->data;
  gis_assistant_switch_to (assistant, prev_page->widget);
}

 static void
update_navigation_buttons (GisAssistant *assistant,
                           GtkWidget    *page)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *current_page = priv->current_page;
  gboolean can_go_backward, can_go_forward, is_last_page;

  if (current_page == NULL)
    return;

  can_go_backward = (current_page->link->prev != NULL);
  gtk_widget_set_sensitive (priv->back, can_go_backward);

  can_go_forward = gis_assistant_get_page_complete (assistant, page);
  gtk_widget_set_sensitive (priv->forward, can_go_forward);

  is_last_page = (current_page->link->next == NULL);
  gtk_widget_set_visible (priv->back, !is_last_page);
  gtk_widget_set_visible (priv->forward, !is_last_page);

  if (gis_assistant_get_use_arrow_buttons (assistant, page))
    {
      gtk_button_set_label (GTK_BUTTON (priv->forward), "→");
      gtk_button_set_label (GTK_BUTTON (priv->back), "←");
    }
  else
    {
      gtk_button_set_label (GTK_BUTTON (priv->forward), _("_Next"));
      gtk_button_set_label (GTK_BUTTON (priv->back), _("_Back"));
    }
}

static void
gis_assistant_prepare (GisAssistant *assistant,
                       GtkWidget    *page)
{
  update_navigation_buttons (assistant, page);
}

void
gis_assistant_add_page (GisAssistant *assistant,
                        GtkWidget    *page)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *page_data = create_page_data_for_page (assistant, page);

  priv->pages = g_list_append (priv->pages, page_data);
  page_data->link = g_list_last (priv->pages);

  GIS_ASSISTANT_GET_CLASS (assistant)->add_page (assistant, page);

  if (priv->current_page->link->next == page_data->link)
    update_navigation_buttons (assistant, priv->current_page->widget);
}

void
gis_assistant_destroy_all_pages (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = assistant->priv;
  GList *l, *next;

  g_object_freeze_notify (G_OBJECT (assistant));

  for (l = priv->pages; l != NULL; l = next)
    {
      PageData *page_data = l->data;
      next = l->next;
      gtk_widget_destroy (page_data->widget);
    }

  g_object_thaw_notify (G_OBJECT (assistant));

  g_assert (priv->pages == NULL);
}

static void
go_forward (GtkWidget    *button,
            GisAssistant *assistant)
{
  gis_assistant_next_page (assistant);
}

static void
go_backward (GtkWidget    *button,
             GisAssistant *assistant)
{
  gis_assistant_previous_page (assistant);
}

void
gis_assistant_set_page_complete (GisAssistant *assistant,
                                 GtkWidget    *page,
                                 gboolean      complete)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *page_data = get_page_data_for_page (page);

  page_data->page_complete = complete;

  if (page_data == priv->current_page)
    update_navigation_buttons (assistant, page);
}

gboolean
gis_assistant_get_page_complete (GisAssistant *assistant,
                                 GtkWidget    *page)
{
  PageData *page_data = get_page_data_for_page (page);
  return page_data->page_complete;
}

void
gis_assistant_set_page_title (GisAssistant *assistant,
                              GtkWidget    *page,
                              gchar        *title)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *page_data = get_page_data_for_page (page);
  g_free (page_data->title);
  page_data->title = g_strdup (title);

  if (page_data == priv->current_page)
    g_object_notify_by_pspec (G_OBJECT (assistant), obj_props[PROP_TITLE]);
}

gchar *
gis_assistant_get_page_title (GisAssistant *assistant,
                              GtkWidget    *page)
{
  PageData *page_data = get_page_data_for_page (page);
  return page_data->title;
}

void
gis_assistant_set_use_arrow_buttons (GisAssistant *assistant,
                                     GtkWidget    *page,
                                     gboolean      use_arrow_buttons)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *page_data = get_page_data_for_page (page);
  page_data->use_arrow_buttons = use_arrow_buttons;

  if (page_data == priv->current_page)
    update_navigation_buttons (assistant, page);
}

gboolean
gis_assistant_get_use_arrow_buttons (GisAssistant *assistant,
                                     GtkWidget    *page)
{
  PageData *page_data = get_page_data_for_page (page);
  return page_data->use_arrow_buttons;
}

gchar *
gis_assistant_get_title (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = assistant->priv;
  if (priv->current_page != NULL && priv->current_page->title != NULL)
    return priv->current_page->title;
  else
    return "";
}

GtkWidget *
_gis_assistant_get_frame (GisAssistant *assistant)
{
  return assistant->priv->frame;
}

void
_gis_assistant_current_page_changed (GisAssistant *assistant,
                                     GtkWidget    *page)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *page_data = get_page_data_for_page (page);

  if (priv->current_page != page_data) {
    priv->current_page = page_data;
    g_object_notify_by_pspec (G_OBJECT (assistant), obj_props[PROP_TITLE]);
    g_signal_emit (assistant, signals[PREPARE], 0, page);
  }
}

static void
gis_assistant_init (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = GET_PRIVATE (assistant);
  assistant->priv = priv;

  priv->main_layout = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_pack_start (GTK_BOX (assistant), priv->main_layout, TRUE, TRUE, 0);

  priv->frame = gtk_frame_new ("");
  gtk_frame_set_shadow_type (GTK_FRAME (priv->frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (priv->main_layout), priv->frame, TRUE, TRUE, 0);

  priv->action_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_set_homogeneous (GTK_BOX (priv->action_area), TRUE);
  gtk_box_pack_start (GTK_BOX (priv->main_layout), priv->action_area, FALSE, TRUE, 0);
  gtk_widget_set_halign (priv->action_area, GTK_ALIGN_END);

  priv->forward = gtk_button_new ();
  gtk_button_set_use_underline (GTK_BUTTON (priv->forward), TRUE);
  gtk_button_set_image (GTK_BUTTON (priv->forward),
                        gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_can_default (priv->forward, TRUE);

  priv->back = gtk_button_new ();
  gtk_button_set_use_underline (GTK_BUTTON (priv->back), TRUE);
  gtk_button_set_image (GTK_BUTTON (priv->back),
                        gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON));

  gtk_box_pack_start (GTK_BOX (priv->action_area), priv->back, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->action_area), priv->forward, FALSE, FALSE, 0);

  g_signal_connect (priv->forward, "clicked",
                    G_CALLBACK (go_forward), assistant);

  g_signal_connect (priv->back, "clicked",
                    G_CALLBACK (go_backward), assistant);

  gtk_widget_show_all (GTK_WIDGET (assistant));
}

static void
gis_assistant_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GisAssistant *assistant = GIS_ASSISTANT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gis_assistant_get_title (assistant));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_assistant_finalize (GObject *gobject)
{
  GisAssistant *assistant = GIS_ASSISTANT (gobject);
  GisAssistantPrivate *priv = assistant->priv;

  priv->current_page = NULL;

  g_list_free_full (priv->pages, (GDestroyNotify) remove_page_data);

  G_OBJECT_CLASS (gis_assistant_parent_class)->finalize (gobject);
}

static void
gis_assistant_class_init (GisAssistantClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GisAssistantPrivate));

  gobject_class->get_property = gis_assistant_get_property;
  gobject_class->finalize = gis_assistant_finalize;

  klass->prepare = gis_assistant_prepare;
  klass->next_page = gis_assistant_real_next_page;

  obj_props[PROP_TITLE] =
    g_param_spec_string ("title",
                         "", "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GisAssistant::next-page:
   * @assistant: the #GisAssistant
   * @page: the page we're leaving
   *
   * The ::next-page signal is emitted when we're leaving
   * a page, allowing a page to do something when it's left.
   */
  signals[NEXT_PAGE] =
    g_signal_new ("next-page",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GisAssistantClass, next_page),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  /**
   * GisAssistant::prepare:
   * @assistant: the #GisAssistant
   * @page: the current page
   *
   * The ::prepare signal is emitted when a new page is set as the
   * assistant's current page, before making the new page visible.
   *
   * A handler for this signal can do any preparations which are
   * necessary before showing @page.
   */
  signals[PREPARE] =
    g_signal_new ("prepare",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GisAssistantClass, prepare),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

}
