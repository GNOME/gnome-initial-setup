/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* -*- encoding: utf8 -*- */
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

enum {
  PROP_0,
  PROP_TITLE,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

enum {
  NEXT_PAGE,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

struct _GisAssistantPrivate
{
  GtkWidget *forward;
  GtkWidget *accept;
  GtkWidget *skip;
  GtkWidget *back;
  GtkWidget *cancel;

  GtkWidget *progress_indicator;
  GtkWidget *main_layout;
  GtkWidget *spinner;
  GtkWidget *titlebar;
  GtkWidget *stack;

  GList *pages;
  GisPage *current_page;
};
typedef struct _GisAssistantPrivate GisAssistantPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisAssistant, gis_assistant, GTK_TYPE_BOX)

struct _GisAssistantPagePrivate
{
  GList *link;
};

static void
widget_destroyed (GtkWidget    *widget,
                  GisAssistant *assistant)
{
  GisPage *page = GIS_PAGE (widget);
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);

  priv->pages = g_list_delete_link (priv->pages, page->assistant_priv->link);
  if (page == priv->current_page)
    priv->current_page = NULL;

  g_slice_free (GisAssistantPagePrivate, page->assistant_priv);
  page->assistant_priv = NULL;
}

static void
gis_assistant_switch_to (GisAssistant          *assistant,
                         GisAssistantDirection  direction,
                         GisPage               *page)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);

  gtk_stack_set_visible_child (GTK_STACK (priv->stack), GTK_WIDGET (page));
}

static void
on_apply_done (GisPage *page,
               gboolean valid,
               gpointer user_data)
{
  GisAssistant *assistant = GIS_ASSISTANT (user_data);
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);

  if (valid)
    g_signal_emit (assistant, signals[NEXT_PAGE], 0,
                   priv->current_page);

  g_object_unref (assistant);
}

void
gis_assistant_next_page (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  if (priv->current_page)
    gis_page_apply_begin (priv->current_page, on_apply_done,
                          g_object_ref (assistant));
  else
    g_signal_emit (assistant, signals[NEXT_PAGE], 0,
                   priv->current_page);
}

static inline gboolean
should_show_page (GList *l)
{
  return l != NULL && gtk_widget_get_visible (GTK_WIDGET (l->data));
}

static GisPage *
find_next_page (GisPage *page)
{
  GList *l = page->assistant_priv->link->next;
  while (!should_show_page (l)) {
    l = l->next;
  }
  return GIS_PAGE (l->data);
}

static void
gis_assistant_real_next_page (GisAssistant *assistant,
                              GisPage      *page)
{
  gis_assistant_switch_to (assistant, GIS_ASSISTANT_NEXT, find_next_page (page));
}

static GisPage *
find_prev_page (GisPage *page)
{
  GList *l = page->assistant_priv->link->prev;
  while (!should_show_page (l)) {
    l = l->prev;
  }
  return GIS_PAGE (l->data);
}

void
gis_assistant_previous_page (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  g_return_if_fail (priv->current_page != NULL);
  gis_assistant_switch_to (assistant, GIS_ASSISTANT_PREV, find_prev_page (priv->current_page));
}

static void
remove_from_progress_indicator (GtkWidget *widget,
                                gpointer   user_data)
{
  GisAssistantPrivate *priv = user_data;
  gtk_container_remove (GTK_CONTAINER (priv->progress_indicator), widget);
}

static void
update_progress_indicator (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  GList *l;

  gtk_container_foreach (GTK_CONTAINER (priv->progress_indicator),
                         remove_from_progress_indicator, priv);

  for (l = priv->pages; l != NULL; l = l->next)
    {
      GisPage *page = GIS_PAGE (l->data);
      GtkWidget *label;

      if (!gtk_widget_get_visible (GTK_WIDGET (page)))
        continue;

      label = gtk_label_new ("•");

      if (page != priv->current_page)
        {
          GtkStyleContext *context = gtk_widget_get_style_context (label);
          gtk_style_context_add_class (context, "dim-label");
        }

      gtk_container_add (GTK_CONTAINER (priv->progress_indicator), label);
      gtk_widget_show (label);
    }
}

static void
update_navigation_buttons (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  GisPage *page = priv->current_page;
  GisAssistantPagePrivate *page_priv;
  gboolean is_last_page;

  if (page == NULL)
    return;

  page_priv = page->assistant_priv;

  is_last_page = (page_priv->link->next == NULL);

  if (is_last_page)
    {
      gtk_widget_hide (priv->back);
      gtk_widget_hide (priv->forward);
    }
  else
    {
      gboolean is_first_page;
      GtkWidget *forward_widget;

      is_first_page = (page_priv->link->prev == NULL);
      gtk_widget_set_visible (priv->back, !is_first_page);

      gtk_widget_hide (priv->forward);
      gtk_widget_hide (priv->accept);
      gtk_widget_hide (priv->skip);

      if (gis_page_get_needs_accept (page))
        forward_widget = priv->accept;
      else
        forward_widget = priv->forward;

      if (gis_page_get_complete (page)) {
        gtk_widget_show (forward_widget);
        gtk_widget_hide (priv->skip);
        gtk_widget_set_sensitive (forward_widget, TRUE);
        gtk_style_context_add_class (gtk_widget_get_style_context (forward_widget), "suggested-action");
      } else if (gis_page_get_skippable (page)) {
        gtk_widget_hide (forward_widget);
        gtk_widget_show (priv->skip);
      } else {
        gtk_widget_show (forward_widget);
        gtk_widget_hide (priv->skip);
        gtk_widget_set_sensitive (forward_widget, FALSE);
        gtk_style_context_remove_class (gtk_widget_get_style_context (forward_widget), "suggested-action");
      }
    }
}

static void
update_applying_state (GisAssistant *assistant)
{
  gboolean applying = FALSE;
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  if (priv->current_page)
    applying = gis_page_get_applying (priv->current_page);
  gtk_widget_set_sensitive (priv->forward, !applying);
  gtk_widget_set_visible (priv->back, !applying);
  gtk_widget_set_visible (priv->cancel, applying);
  gtk_widget_set_visible (priv->spinner, applying);

  if (applying)
    gtk_spinner_start (GTK_SPINNER (priv->spinner));
  else
    gtk_spinner_stop (GTK_SPINNER (priv->spinner));
}

static void
update_titlebar (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);

  gtk_header_bar_set_title (GTK_HEADER_BAR (priv->titlebar),
                            gis_assistant_get_title (assistant));
}

static void
page_notify (GisPage      *page,
             GParamSpec   *pspec,
             GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);

  if (page != priv->current_page)
    return;

  if (strcmp (pspec->name, "title") == 0)
    g_object_notify_by_pspec (G_OBJECT (assistant), obj_props[PROP_TITLE]);
  else if (strcmp (pspec->name, "applying") == 0)
    update_applying_state (assistant);
  else
    update_navigation_buttons (assistant);
}

void
gis_assistant_add_page (GisAssistant *assistant,
                        GisPage      *page)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  GList *link;

  g_return_if_fail (page->assistant_priv == NULL);

  page->assistant_priv = g_slice_new0 (GisAssistantPagePrivate);
  priv->pages = g_list_append (priv->pages, page);
  link = page->assistant_priv->link = g_list_last (priv->pages);

  g_signal_connect (page, "destroy", G_CALLBACK (widget_destroyed), assistant);
  g_signal_connect (page, "notify", G_CALLBACK (page_notify), assistant);

  gtk_container_add (GTK_CONTAINER (priv->stack), GTK_WIDGET (page));

  if (priv->current_page->assistant_priv->link == link->prev)
    update_navigation_buttons (assistant);

  update_progress_indicator (assistant);
}

GisPage *
gis_assistant_get_current_page (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  return priv->current_page;
}

GList *
gis_assistant_get_all_pages (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  return priv->pages;
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

static void
do_cancel (GtkWidget    *button,
           GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  if (priv->current_page)
    gis_page_apply_cancel (priv->current_page);
}

const gchar *
gis_assistant_get_title (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  if (priv->current_page != NULL)
    return gis_page_get_title (priv->current_page);
  else
    return "";
}

GtkWidget *
gis_assistant_get_titlebar (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  return priv->titlebar;
}

static void
update_current_page (GisAssistant *assistant,
                     GisPage      *page)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);

  if (priv->current_page == page)
    return;

  priv->current_page = page;
  g_object_notify_by_pspec (G_OBJECT (assistant), obj_props[PROP_TITLE]);

  update_titlebar (assistant);
  update_applying_state (assistant);
  update_navigation_buttons (assistant);
  update_progress_indicator (assistant);
  gis_page_shown (page);
}

static void
current_page_changed (GObject    *gobject,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GisAssistant *assistant = GIS_ASSISTANT (user_data);
  GtkStack *stack = GTK_STACK (gobject);
  GtkWidget *new_page = gtk_stack_get_visible_child (stack);

  update_current_page (assistant, GIS_PAGE (new_page));
}

void
gis_assistant_locale_changed (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  GList *l;

  gtk_button_set_label (GTK_BUTTON (priv->forward), _("_Next"));
  gtk_button_set_label (GTK_BUTTON (priv->accept), _("_Accept"));
  gtk_button_set_label (GTK_BUTTON (priv->skip), _("_Skip"));
  gtk_button_set_label (GTK_BUTTON (priv->back), _("_Previous"));
  gtk_button_set_label (GTK_BUTTON (priv->cancel), _("_Cancel"));

  for (l = priv->pages; l != NULL; l = l->next)
    gis_page_locale_changed (l->data);
}

void
gis_assistant_save_data (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);
  GList *l;

  for (l = priv->pages; l != NULL; l = l->next)
    gis_page_save_data (l->data);
}

static void
gis_assistant_init (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = gis_assistant_get_instance_private (assistant);

  gtk_widget_init_template (GTK_WIDGET (assistant));

  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (current_page_changed), assistant);

  g_signal_connect (priv->forward, "clicked", G_CALLBACK (go_forward), assistant);
  g_signal_connect (priv->accept, "clicked", G_CALLBACK (go_forward), assistant);
  g_signal_connect (priv->skip, "clicked", G_CALLBACK (go_forward), assistant);

  g_signal_connect (priv->back, "clicked", G_CALLBACK (go_backward), assistant);
  g_signal_connect (priv->cancel, "clicked", G_CALLBACK (do_cancel), assistant);

  gis_assistant_locale_changed (assistant);
  update_applying_state (assistant);
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
gis_assistant_class_init (GisAssistantClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-assistant.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, forward);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, accept);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, skip);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, back);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, cancel);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, progress_indicator);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, main_layout);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, spinner);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, titlebar);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAssistant, stack);

  gobject_class->get_property = gis_assistant_get_property;

  klass->next_page = gis_assistant_real_next_page;

  obj_props[PROP_TITLE] =
    g_param_spec_string ("title",
                         "", "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);

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
                  G_TYPE_NONE, 1, GIS_TYPE_PAGE);
}
