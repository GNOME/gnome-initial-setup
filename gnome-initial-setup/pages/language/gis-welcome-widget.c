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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"
#include "gis-welcome-widget.h"

#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>

#include "cc-common-language.h"

struct _GisWelcomeWidgetPrivate
{
  GtkWidget *stack;
  GHashTable *translation_widgets;  /* (element-type owned utf8 unowned GtkWidget) (owned) */

  guint timeout_id;
};
typedef struct _GisWelcomeWidgetPrivate GisWelcomeWidgetPrivate;

#define TIMEOUT 5

G_DEFINE_TYPE_WITH_PRIVATE (GisWelcomeWidget, gis_welcome_widget, GTK_TYPE_BIN);

static gboolean
advance_stack (gpointer user_data)
{
  GisWelcomeWidget *widget = user_data;
  GisWelcomeWidgetPrivate *priv = gis_welcome_widget_get_instance_private (widget);
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (priv->stack));
  if (children == NULL)
    goto out;

  for (l = children; l != NULL; l = l->next)
    {
      if (l->data == gtk_stack_get_visible_child (GTK_STACK (priv->stack)))
        break;
    }

  /* wrap around */
  if (l->next)
    l = l->next;
  else
    l = children;

  gtk_stack_set_visible_child (GTK_STACK (priv->stack), l->data);

  g_list_free (children);

 out:
  return G_SOURCE_CONTINUE;
}

static void
gis_welcome_widget_start (GisWelcomeWidget *widget)
{
  GisWelcomeWidgetPrivate *priv = gis_welcome_widget_get_instance_private (widget);

  if (priv->timeout_id > 0)
    return;

  priv->timeout_id = g_timeout_add_seconds (5, advance_stack, widget);
}

static void
gis_welcome_widget_stop (GisWelcomeWidget *widget)
{
  GisWelcomeWidgetPrivate *priv = gis_welcome_widget_get_instance_private (widget);

  if (priv->timeout_id == 0)
    return;

  g_source_remove (priv->timeout_id);
  priv->timeout_id = 0;
}

static void
gis_welcome_widget_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gis_welcome_widget_parent_class)->map (widget);
  gis_welcome_widget_start (GIS_WELCOME_WIDGET (widget));
}

static void
gis_welcome_widget_unmap (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gis_welcome_widget_parent_class)->unmap (widget);
  gis_welcome_widget_stop (GIS_WELCOME_WIDGET (widget));
}

static const char *
welcome (const char *locale_id)
{
  locale_t locale;
  locale_t old_locale;
  const char *welcome;

  locale = newlocale (LC_MESSAGES_MASK, locale_id, (locale_t) 0);
  if (locale == (locale_t) 0)
    {
      if (errno == ENOENT)
        g_debug ("Failed to create locale %s: %s", locale_id, g_strerror (errno));
      else
        g_warning ("Failed to create locale %s: %s", locale_id, g_strerror (errno));

      return "Welcome!";
    }

  old_locale = uselocale (locale);

  /* Translators: This is meant to be a warm, engaging welcome message,
   * like greeting somebody at the door. If the exclamation mark is not
   * suitable for this in your language you may replace it.
   */
  welcome = _("Welcome!");

  uselocale (old_locale);
  freelocale (locale);

  return welcome;
}

static GtkWidget *
big_label (const char *text)
{
  GtkWidget *label = gtk_label_new (text);

  gtk_style_context_add_class (gtk_widget_get_style_context (label), "title-1");

  return label;
}

static void
fill_stack (GisWelcomeWidget *widget)
{
  GisWelcomeWidgetPrivate *priv = gis_welcome_widget_get_instance_private (widget);
  g_autoptr(GHashTable) initial = cc_common_language_get_initial_languages ();
  GHashTableIter iter;
  gpointer key, value;
  g_autoptr(GHashTable) added_translations = NULL;

  added_translations = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_iter_init (&iter, initial);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *locale_id = key;
      const char *text;
      GtkWidget *label;

      if (!cc_common_language_has_font (locale_id))
        continue;

      text = welcome (locale_id);
      label = g_hash_table_lookup (added_translations, text);
      if (label == NULL) {
        label = big_label (text);
        gtk_container_add (GTK_CONTAINER (priv->stack), label);
        gtk_widget_show (label);
        g_hash_table_insert (added_translations, (gpointer) text, label);
      }

      g_hash_table_insert (priv->translation_widgets, g_strdup (locale_id), label);
    }
}

static void
gis_welcome_widget_constructed (GObject *object)
{
  fill_stack (GIS_WELCOME_WIDGET (object));
}

static void
gis_welcome_widget_dispose (GObject *object)
{
  GisWelcomeWidget *widget = GIS_WELCOME_WIDGET (object);
  GisWelcomeWidgetPrivate *priv = gis_welcome_widget_get_instance_private (widget);

  g_clear_pointer (&priv->translation_widgets, g_hash_table_unref);

  G_OBJECT_CLASS (gis_welcome_widget_parent_class)->dispose (object);
}

static void
gis_welcome_widget_class_init (GisWelcomeWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-welcome-widget.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GisWelcomeWidget, stack);

  object_class->constructed = gis_welcome_widget_constructed;
  object_class->dispose = gis_welcome_widget_dispose;
  widget_class->map = gis_welcome_widget_map;
  widget_class->unmap = gis_welcome_widget_unmap;
}

static void
gis_welcome_widget_init (GisWelcomeWidget *widget)
{
  GisWelcomeWidgetPrivate *priv = gis_welcome_widget_get_instance_private (widget);

  priv->translation_widgets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  gtk_widget_init_template (GTK_WIDGET (widget));
}

void
gis_welcome_widget_show_locale (GisWelcomeWidget *widget,
                                const char       *locale_id)
{
  GisWelcomeWidgetPrivate *priv = gis_welcome_widget_get_instance_private (widget);
  GtkWidget *label;

  /* Restart the widget to reset the timer. */
  gis_welcome_widget_stop (widget);
  gis_welcome_widget_start (widget);

  label = g_hash_table_lookup (priv->translation_widgets, locale_id);
  if (label)
    gtk_stack_set_visible_child (GTK_STACK (priv->stack), label);
}
