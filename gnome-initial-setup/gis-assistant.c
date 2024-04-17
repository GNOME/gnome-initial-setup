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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

enum {
  PAGE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GParamSpec *obj_props[PROP_LAST];

struct _GisAssistant
{
  GtkBox     parent_instance;

  GtkWidget *forward;
  GtkWidget *accept;
  GtkWidget *skip;
  GtkWidget *back;
  GtkWidget *cancel;

  GtkWidget *spinner;
  GtkWidget *titlebar;
  GtkWidget *title;
  GtkWidget *stack;

  GList *pages;
  GisPage *current_page;
};

G_DEFINE_TYPE (GisAssistant, gis_assistant, GTK_TYPE_BOX)

static void
visible_child_changed (GisAssistant *assistant)
{
  g_signal_emit (assistant, signals[PAGE_CHANGED], 0);
}

static void
switch_to (GisAssistant          *assistant,
           GisPage               *page)
{
  g_return_if_fail (page != NULL);

  gtk_stack_set_visible_child (GTK_STACK (assistant->stack), GTK_WIDGET (page));
}

static inline gboolean
should_show_page (GisPage *page)
{
  return gtk_widget_get_visible (GTK_WIDGET (page));
}

static GisPage *
find_next_page (GisAssistant *self,
                GisPage      *page)
{
  GList *l = g_list_find (self->pages, page);

  g_return_val_if_fail (l != NULL, NULL);

  /* We need the next page */
  l = l->next;

  for (; l != NULL; l = l->next)
    {
      GisPage *page = GIS_PAGE (l->data);

      if (should_show_page (page))
        return page;
    }

  return NULL;
}

static void
switch_to_next_page (GisAssistant *assistant)
{
  switch_to (assistant, find_next_page (assistant, assistant->current_page));
}

static void
on_apply_done (GisPage *page,
               gboolean valid,
               gpointer user_data)
{
  GisAssistant *assistant = GIS_ASSISTANT (user_data);

  if (valid)
    switch_to_next_page (assistant);
}

void
gis_assistant_next_page (GisAssistant *assistant)
{
  if (assistant->current_page)
    gis_page_apply_begin (assistant->current_page, on_apply_done, assistant);
  else
    switch_to_next_page (assistant);
}

static GisPage *
find_prev_page (GisAssistant *self,
                GisPage      *page)
{
  GList *l = g_list_find (self->pages, page);

  g_return_val_if_fail (l != NULL, NULL);

  /* We need the previous page */
  l = l->prev;

  for (; l != NULL; l = l->prev)
    {
      GisPage *page = GIS_PAGE (l->data);

      if (should_show_page (page))
        return page;
    }

  return NULL;
}

void
gis_assistant_previous_page (GisAssistant *assistant)
{
  g_return_if_fail (assistant->current_page != NULL);
  switch_to (assistant, find_prev_page (assistant, assistant->current_page));
}

static void
set_navigation_button (GisAssistant *assistant,
                       GtkWidget    *widget,
                       gboolean     sensitive)
{
  gtk_widget_set_visible (assistant->forward, (widget == assistant->forward));
  gtk_widget_set_sensitive (assistant->forward, (widget == assistant->forward && sensitive));
  gtk_widget_set_visible (assistant->accept, (widget == assistant->accept));
  gtk_widget_set_sensitive (assistant->accept, (widget == assistant->accept && sensitive));
  gtk_widget_set_visible (assistant->skip, (widget == assistant->skip));
  gtk_widget_set_sensitive (assistant->skip, (widget == assistant->skip && sensitive));
}

void
update_navigation_buttons (GisAssistant *assistant)
{
  GisPage *page = assistant->current_page;
  GList *l;
  gboolean is_last_page;

  if (page == NULL)
    return;

  l = g_list_find (assistant->pages, page);

  is_last_page = (l->next == NULL);

  if (is_last_page)
    {
      gtk_widget_set_visible (assistant->back, FALSE);
      gtk_widget_set_visible (assistant->forward, FALSE);
      gtk_widget_set_visible (assistant->skip, FALSE);
      gtk_widget_set_visible (assistant->cancel, FALSE);
      gtk_widget_set_visible (assistant->accept, FALSE);
    }
  else
    {
      gboolean is_first_page;
      GtkWidget *next_widget;

      is_first_page = (l->prev == NULL);
      gtk_widget_set_visible (assistant->back, !is_first_page);

      if (gis_page_get_needs_accept (page))
        next_widget = assistant->accept;
      else
        next_widget = assistant->forward;

      if (gis_page_get_complete (page)) {
        set_navigation_button (assistant, next_widget, TRUE);
      } else if (gis_page_get_skippable (page)) {
        set_navigation_button (assistant, assistant->skip, TRUE);
      } else {
        set_navigation_button (assistant, next_widget, FALSE);
      }

      /* This really means "page manages its own forward button" */
      if (gis_page_get_has_forward (page)) {
        GtkWidget *dummy_widget = NULL;
        /* Ensure none of the three buttons is visible or sensitive */
        set_navigation_button (assistant, dummy_widget, FALSE);
      }
    }
}

static void
update_applying_state (GisAssistant *assistant)
{
  gboolean applying = FALSE;
  gboolean is_first_page = FALSE;

  if (assistant->current_page)
    {
      applying = gis_page_get_applying (assistant->current_page);
      is_first_page = assistant->pages->data == assistant->current_page;
    }
  gtk_widget_set_sensitive (assistant->forward, !applying);
  gtk_widget_set_visible (assistant->back, !applying && !is_first_page);
  gtk_widget_set_visible (assistant->cancel, applying);
  gtk_widget_set_visible (assistant->spinner, applying);

  if (applying)
    gtk_spinner_start (GTK_SPINNER (assistant->spinner));
  else
    gtk_spinner_stop (GTK_SPINNER (assistant->spinner));
}

static void
update_titlebar (GisAssistant *assistant)
{
  gtk_label_set_label (GTK_LABEL (assistant->title),
                       gis_assistant_get_title (assistant));
}

static void
page_notify (GisPage      *page,
             GParamSpec   *pspec,
             GisAssistant *assistant)
{
  if (page != assistant->current_page)
    return;

  if (strcmp (pspec->name, "title") == 0)
    {
      g_object_notify_by_pspec (G_OBJECT (assistant), obj_props[PROP_TITLE]);
      update_titlebar (assistant);
    }
  else if (strcmp (pspec->name, "applying") == 0)
    {
      update_applying_state (assistant);
    }
  else
    {
      update_navigation_buttons (assistant);
    }
}

void
gis_assistant_add_page (GisAssistant *assistant,
                        GisPage      *page)
{
  GList *link;

  /* Page shouldn't already exist */
  g_return_if_fail (!g_list_find (assistant->pages, page));

  assistant->pages = g_list_append (assistant->pages, page);
  link = g_list_last (assistant->pages);
  link = link->prev;

  g_signal_connect (page, "notify", G_CALLBACK (page_notify), assistant);

  gtk_stack_add_child (GTK_STACK (assistant->stack), GTK_WIDGET (page));

  /* Update buttons if current page is now the second last page */
  if (assistant->current_page && link &&
      link->data == assistant->current_page)
    update_navigation_buttons (assistant);
}

void
gis_assistant_remove_page (GisAssistant *assistant,
                           GisPage      *page)
{
  assistant->pages = g_list_remove (assistant->pages, page);
  if (page == assistant->current_page)
    assistant->current_page = NULL;

  gtk_stack_remove (GTK_STACK (assistant->stack), GTK_WIDGET (page));
}

GisPage *
gis_assistant_get_current_page (GisAssistant *assistant)
{
  return assistant->current_page;
}

GList *
gis_assistant_get_all_pages (GisAssistant *assistant)
{
  return assistant->pages;
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
  if (assistant->current_page)
    gis_page_apply_cancel (assistant->current_page);
}

const gchar *
gis_assistant_get_title (GisAssistant *assistant)
{
  if (assistant->current_page != NULL)
    return gis_page_get_title (assistant->current_page);
  else
    return "";
}

GtkWidget *
gis_assistant_get_titlebar (GisAssistant *assistant)
{
  return assistant->titlebar;
}

static void
update_current_page (GisAssistant *assistant,
                     GisPage      *page)
{
  if (assistant->current_page == page)
    return;

  assistant->current_page = page;
  g_object_notify_by_pspec (G_OBJECT (assistant), obj_props[PROP_TITLE]);

  update_titlebar (assistant);
  update_applying_state (assistant);
  update_navigation_buttons (assistant);

  gtk_widget_grab_focus (assistant->forward);

  if (page)
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
  GList *l;

  gtk_button_set_label (GTK_BUTTON (assistant->forward), _("_Next"));
  gtk_button_set_label (GTK_BUTTON (assistant->accept), _("_Accept"));
  gtk_button_set_label (GTK_BUTTON (assistant->skip), _("_Skip"));
  gtk_button_set_label (GTK_BUTTON (assistant->back), _("_Previous"));
  gtk_button_set_label (GTK_BUTTON (assistant->cancel), _("_Cancel"));

  for (l = assistant->pages; l != NULL; l = l->next)
    gis_page_locale_changed (l->data);

  update_titlebar (assistant);
}

gboolean
gis_assistant_save_data (GisAssistant  *assistant,
                         GError       **error)
{
  GList *l;

  for (l = assistant->pages; l != NULL; l = l->next)
    {
      if (!gis_page_save_data (l->data, error))
        return FALSE;
    }

  return TRUE;
}

static void
gis_assistant_init (GisAssistant *assistant)
{
  gtk_widget_init_template (GTK_WIDGET (assistant));

  g_signal_connect (assistant->stack, "notify::visible-child",
                    G_CALLBACK (current_page_changed), assistant);

  g_signal_connect (assistant->forward, "clicked", G_CALLBACK (go_forward), assistant);
  g_signal_connect (assistant->accept, "clicked", G_CALLBACK (go_forward), assistant);
  g_signal_connect (assistant->skip, "clicked", G_CALLBACK (go_forward), assistant);

  g_signal_connect (assistant->back, "clicked", G_CALLBACK (go_backward), assistant);
  g_signal_connect (assistant->cancel, "clicked", G_CALLBACK (do_cancel), assistant);

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

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, forward);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, accept);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, skip);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, back);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, cancel);

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, spinner);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, titlebar);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, title);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAssistant, stack);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), visible_child_changed);

  gobject_class->get_property = gis_assistant_get_property;

  obj_props[PROP_TITLE] =
    g_param_spec_string ("title",
                         "", "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);


  /**
   * GisAssistant::page-changed:
   * @assistant: the #GisAssistant
   *
   * The ::page-changed signal is emitted when the visible page
   * changed.
   */
  signals[PAGE_CHANGED] =
    g_signal_new ("page-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}
