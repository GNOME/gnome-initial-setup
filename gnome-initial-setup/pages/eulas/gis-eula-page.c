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

/* EULA pages {{{1 */

#define PAGE_ID "eula"

#include "config.h"
#include "eulas-resources.h"
#include "gis-eula-page.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_DEFINE_TYPE (GisEulaPage, gis_eula_page, GIS_TYPE_PAGE);

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_EULA_PAGE, GisEulaPagePrivate))

struct _GisEulaPagePrivate
{
  GFile *eula;

  GtkWidget *checkbox;
  GtkWidget *scrolled_window;

  gboolean require_checkbox;
  gboolean require_scroll;
};

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

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

static GtkTextBuffer *
build_eula_text_buffer (GFile     *file,
                        GError   **error_out)
{
  GtkTextBuffer *buffer = NULL;
  GtkTextIter start, end;
  GError *error = NULL;
  GInputStream *input_stream = NULL;
  FileType type = get_file_type (file);

  if (type == SKIP)
    return NULL;

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

  return buffer;

 error_out:
  g_propagate_error (error_out, error);
  if (buffer != NULL)
    g_object_unref (buffer);
  return NULL;
}

static GtkWidget *
build_eula_text_view (GFile *eula)
{
  GtkWidget *widget = NULL;
  GtkTextBuffer *buffer;
  GError *error = NULL;

  buffer = build_eula_text_buffer (eula, &error);

  if (buffer == NULL)
    goto out;

  widget = gtk_text_view_new_with_buffer (buffer);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (widget), FALSE);

 out:
  if (error != NULL) {
    g_printerr ("Error while reading EULA: %s", error->message);
    g_error_free (error);
  }

  return widget;
}

static gboolean
get_page_complete (GisEulaPage *page)
{
  GisEulaPagePrivate *priv = page->priv;

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
  GisEulaPagePrivate *priv = page->priv;
  GtkWidget *text_view;

  gboolean require_checkbox = FALSE;
  gboolean require_scroll = FALSE;

  GFile *eula = priv->eula;

  G_OBJECT_CLASS (gis_eula_page_parent_class)->constructed (object);

  text_view = build_eula_text_view (eula);
  if (text_view == NULL)
    return;

  priv->scrolled_window = WID ("scrolledwindow");
  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), text_view);

  gtk_widget_show (text_view);

  get_config (eula, &require_checkbox, &require_scroll);

  priv->require_checkbox = require_checkbox;
  priv->require_scroll = require_scroll;

  priv->checkbox = WID ("checkbox");
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

  gtk_container_add (GTK_CONTAINER (page), WID ("eula-page"));

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_eula_page_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GisEulaPage *page = GIS_EULA_PAGE (object);
  GisEulaPagePrivate *priv = page->priv;
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
  GisEulaPagePrivate *priv = page->priv;
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
  GisEulaPagePrivate *priv = page->priv;

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

  g_type_class_add_private (object_class, sizeof(GisEulaPagePrivate));
}

static void
gis_eula_page_init (GisEulaPage *page)
{
  g_resources_register (eulas_get_resource ());
  page->priv = GET_PRIVATE (page);
}
