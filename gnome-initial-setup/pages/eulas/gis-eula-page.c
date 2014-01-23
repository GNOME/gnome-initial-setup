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

/* EULA pages {{{1 */

#define PAGE_ID "eula"

#include "config.h"
#include "eulas-resources.h"
#include "gis-eula-page.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct _GisEulaPagePrivate
{
  GtkWidget *checkbox;
  GtkWidget *scrolled_window;
  GtkWidget *text_view;

  GFile *eula;

  gboolean require_checkbox;
  gboolean require_scroll;
};
typedef struct _GisEulaPagePrivate GisEulaPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisEulaPage, gis_eula_page, GIS_TYPE_PAGE);

enum
{
  PROP_0,
  PROP_EULA,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

typedef enum {
  TEXT,
  MARKUP,
  SKIP,
} FileType;

static FileType
get_file_type (GFile *file)
{
  gchar *path, *last_dot;
  FileType type;

  path = g_file_get_path (file);
  last_dot = strrchr (path, '.');

  if (g_strcmp0 (last_dot, ".txt") == 0)
    type = TEXT;
  else if (g_strcmp0 (last_dot, ".xml") == 0)
    type = MARKUP;
  else
    type = SKIP;

  g_free (path);
  return type;
}

static gboolean
build_eula_text_buffer (GFile          *file,
                        GtkTextBuffer **buffer_out,
                        GError        **error_out)
{
  GtkTextBuffer *buffer = NULL;
  GtkTextIter start, end;
  GError *error = NULL;
  GInputStream *input_stream = NULL;
  FileType type = get_file_type (file);

  if (type == SKIP)
    return FALSE;

  input_stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
  if (input_stream == NULL)
    goto error_out;

  buffer = gtk_text_buffer_new (NULL);

  switch (type) {
  case TEXT:
    if (!splice_buffer_text (input_stream, buffer, &error))
      goto error_out;

    /* monospace the text */
    gtk_text_buffer_create_tag (buffer, "monospace", "family", "monospace", NULL);
    gtk_text_buffer_get_start_iter (buffer, &start);
    gtk_text_buffer_get_end_iter (buffer, &end);
    gtk_text_buffer_apply_tag_by_name (buffer, "monospace", &start, &end);
    break;
  case MARKUP:
    if (!splice_buffer_markup (input_stream, buffer, &error))
      goto error_out;
    break;
  default:
    g_assert_not_reached ();
    break;
  }

  *buffer_out = buffer;
  return TRUE;

 error_out:
  g_propagate_error (error_out, error);
  if (buffer != NULL)
    g_object_unref (buffer);
  return FALSE;
}

static gboolean
get_page_complete (GisEulaPage *page)
{
  GisEulaPagePrivate *priv = gis_eula_page_get_instance_private (page);

  if (priv->require_checkbox) {
    GtkToggleButton *checkbox = GTK_TOGGLE_BUTTON (priv->checkbox);
    if (!gtk_toggle_button_get_active (checkbox))
      return FALSE;
  }

  if (priv->require_scroll) {
    GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (priv->scrolled_window);
    GtkAdjustment *vadjust = gtk_scrolled_window_get_vadjustment (scrolled_window);
    gdouble value, upper;

    value = gtk_adjustment_get_value (vadjust);
    upper = gtk_adjustment_get_upper (vadjust) - gtk_adjustment_get_page_size (vadjust);

    if (value < upper)
      return FALSE;
  }

  return TRUE;
}

static void
sync_page_complete (GisEulaPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), get_page_complete (page));
}

static void
get_config (GFile    *eula,
            gboolean *require_checkbox,
            gboolean *require_scroll)
{
  gchar *path, *config_path;
  GError *error = NULL;
  GKeyFile *config;

  config = g_key_file_new ();

  path = g_file_get_path (eula);
  config_path = g_strconcat (path, ".conf", NULL);
  if (!g_key_file_load_from_file (config, config_path, 0, &error))
    goto out;

  *require_checkbox = g_key_file_get_boolean (config, "Requirements",
                                              "require-checkbox", NULL);

  *require_scroll = g_key_file_get_boolean (config, "Requirements",
                                            "require-scroll", NULL);

 out:
  g_clear_error (&error);
  g_key_file_unref (config);
}

static void
gis_eula_page_constructed (GObject *object)
{
  GisEulaPage *page = GIS_EULA_PAGE (object);
  GisEulaPagePrivate *priv = gis_eula_page_get_instance_private (page);

  gboolean require_checkbox = FALSE;
  gboolean require_scroll = FALSE;

  GFile *eula = priv->eula;
  GtkTextBuffer *buffer;
  GError *error = NULL;

  G_OBJECT_CLASS (gis_eula_page_parent_class)->constructed (object);

  if (!build_eula_text_buffer (eula, &buffer, &error))
    goto out;

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (priv->text_view), buffer);

  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->text_view), GTK_TEXT_WINDOW_TOP, 16);
  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->text_view), GTK_TEXT_WINDOW_LEFT, 16);
  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->text_view), GTK_TEXT_WINDOW_RIGHT, 16);
  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->text_view), GTK_TEXT_WINDOW_BOTTOM, 16);

  get_config (eula, &require_checkbox, &require_scroll);

  priv->require_checkbox = require_checkbox;
  priv->require_scroll = require_scroll;

  gtk_widget_set_visible (priv->checkbox, require_checkbox);

  if (require_scroll) {
    GtkAdjustment *vadjust;
    vadjust = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window));
    g_signal_connect_swapped (vadjust, "changed",
                              G_CALLBACK (sync_page_complete),
                              page);
    g_signal_connect_swapped (vadjust, "value-changed",
                              G_CALLBACK (sync_page_complete),
                              page);
  }

  sync_page_complete (page);

  gtk_widget_show (GTK_WIDGET (page));

 out:
  if (error)
    g_printerr ("Error while reading EULA: %s", error->message);
  g_clear_error (&error);
}

static void
gis_eula_page_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GisEulaPage *page = GIS_EULA_PAGE (object);
  GisEulaPagePrivate *priv = gis_eula_page_get_instance_private (page);

  switch (prop_id)
    {
    case PROP_EULA:
      g_value_set_object (value, priv->eula);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
    }
}

static void
gis_eula_page_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GisEulaPage *page = GIS_EULA_PAGE (object);
  GisEulaPagePrivate *priv = gis_eula_page_get_instance_private (page);

  switch (prop_id)
    {
    case PROP_EULA:
      priv->eula = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
    }
}

static void
gis_eula_page_dispose (GObject *object)
{
  GisEulaPage *page = GIS_EULA_PAGE (object);
  GisEulaPagePrivate *priv = gis_eula_page_get_instance_private (page);

  g_clear_object (&priv->eula);

  G_OBJECT_CLASS (gis_eula_page_parent_class)->dispose (object);
}

static void
gis_eula_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("License Agreements"));
}

static void
gis_eula_page_class_init (GisEulaPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-eula-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEulaPage, checkbox);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEulaPage, scrolled_window);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEulaPage, text_view);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_eula_page_locale_changed;
  object_class->get_property = gis_eula_page_get_property;
  object_class->set_property = gis_eula_page_set_property;
  object_class->constructed = gis_eula_page_constructed;
  object_class->dispose = gis_eula_page_dispose;

  obj_props[PROP_EULA] =
    g_param_spec_object ("eula", "", "",
                         G_TYPE_FILE,
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
gis_eula_page_init (GisEulaPage *page)
{
  g_resources_register (eulas_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}
