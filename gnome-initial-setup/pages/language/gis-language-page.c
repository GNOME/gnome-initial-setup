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

/* Language page {{{1 */

#define PAGE_ID "language"

#include "config.h"
#include "gis-language-page.h"

#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#include "cc-common-language.h"
#include "gdm-languages.h"

#include <glib-object.h>

#include <egg-list-box.h>

#include "gis-language-page.h"

G_DEFINE_TYPE (GisLanguagePage, gis_language_page, GIS_TYPE_PAGE);

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_LANGUAGE_PAGE, GisLanguagePagePrivate))

enum {
  COL_LOCALE_ID,
  COL_LOCALE_NAME,
  COL_IS_EXTRA,
  NUM_COLS,
};

struct _GisLanguagePagePrivate
{
  GtkWidget *more_item;
  GtkWidget *page;
  GtkWidget *filter_entry;
  GtkWidget *language_list;
  gboolean adding_languages;
  gboolean showing_extra;
};

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE (page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
set_locale_id (GisLanguagePage *page,
               gchar           *new_locale_id)
{
  gchar *old_locale_id = cc_common_language_get_current_language ();
  if (g_strcmp0 (old_locale_id, new_locale_id) != 0) {
    setlocale (LC_MESSAGES, new_locale_id);
    gis_driver_locale_changed (GIS_PAGE (page)->driver);
  }
  g_free (old_locale_id);
}

static gint
sort_languages (gconstpointer a,
                gconstpointer b,
                gpointer      data)
{
  GisLanguagePage *page = GIS_LANGUAGE_PAGE (data);
  GisLanguagePagePrivate *priv = page->priv;

  if (a == priv->more_item)
    return 1;

  if (b == priv->more_item)
    return -1;

  const char *la = g_object_get_data (G_OBJECT (a), "locale-name");
  const char *lb = g_object_get_data (G_OBJECT (b), "locale-name");

  gboolean iea = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (a), "is-extra"));
  gboolean ieb = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (b), "is-extra"));

  if (iea != ieb) {
    return ieb - iea;
  } else {
    return strcmp (la, lb);
  }
}

static char *
use_language (char *locale_id)
{
  char *use, *language;

  /* Translators: the parameter here is your language's name, like
   * "Use English", "Deutsch verwenden", etc. */
  setlocale (LC_MESSAGES, locale_id);
  use = _("Use %s");

  language = gdm_get_language_from_name (locale_id, locale_id);

  return g_strdup_printf (use, language);
}

static GtkWidget *
language_widget_new (char     *locale_id,
                     gboolean  is_extra)
{
  gchar *locale_name;
  GtkWidget *widget;

  locale_name = use_language (locale_id);

  widget = gtk_label_new (locale_name);
  g_object_set_data (G_OBJECT (widget), "locale-id",
                     locale_id);
  g_object_set_data (G_OBJECT (widget), "locale-name",
                     locale_name);
  g_object_set_data (G_OBJECT (widget), "is-extra",
                     GUINT_TO_POINTER (is_extra));

  return widget;
}

static GtkWidget *
more_widget_new (void)
{
  GtkWidget *widget = gtk_label_new ("…");
  gtk_widget_set_tooltip_text (widget, _("More…"));
  return widget;
}

static void
add_languages (GisLanguagePage *page,
               char           **locale_ids,
               GHashTable      *initial)
{
  GisLanguagePagePrivate *priv = page->priv;
  char *orig_locale_id = cc_common_language_get_current_language ();

  priv->adding_languages = TRUE;

  while (*locale_ids) {
    gchar *locale_id;
    gboolean is_extra;
    GtkWidget *widget;

    locale_id = *locale_ids;

    locale_ids ++;

    if (!cc_common_language_has_font (locale_id))
      continue;

    is_extra = (g_hash_table_lookup (initial, locale_id) != NULL);

    widget = language_widget_new (locale_id, is_extra);

    gtk_container_add (GTK_CONTAINER (priv->language_list),
                       widget);

    if (strcmp (locale_id, orig_locale_id) == 0)
      egg_list_box_select_child (EGG_LIST_BOX (priv->language_list), widget);
  }

  gtk_container_add (GTK_CONTAINER (priv->language_list),
                     priv->more_item);

  gtk_widget_show_all (priv->language_list);

  priv->adding_languages = FALSE;

  setlocale (LC_MESSAGES, orig_locale_id);
  g_free (orig_locale_id);
}

static void
add_all_languages (GisLanguagePage *page)
{
  char **locale_ids = gdm_get_all_language_names ();
  GHashTable *initial =  cc_common_language_get_initial_languages ();

  add_languages (page, locale_ids, initial);
}

static gboolean
language_visible (GtkWidget *child,
                  gpointer   user_data)
{
  GisLanguagePage *page = user_data;
  GisLanguagePagePrivate *priv = page->priv;
  gchar *locale_name;
  const gchar *filter_contents;
  gboolean is_extra;

  if (child == page->priv->more_item)
    return !page->priv->showing_extra;

  is_extra = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (child), "is-extra"));
  locale_name = g_object_get_data (G_OBJECT (child), "locale-name");

  filter_contents = gtk_entry_get_text (GTK_ENTRY (priv->filter_entry));
  if (*filter_contents && strcasestr (locale_name, filter_contents) == NULL)
    return FALSE;

  if (!page->priv->showing_extra && !is_extra)
    return FALSE;

  return TRUE;
}

static void
show_more (GisLanguagePage *page)
{
  GisLanguagePagePrivate *priv = page->priv;

  gtk_widget_show (priv->filter_entry);

  page->priv->showing_extra = TRUE;

  egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));
}

static void
selection_changed (EggListBox      *box,
                   GtkWidget       *child,
                   GisLanguagePage *page)
{
  gchar *new_locale_id;

  if (page->priv->adding_languages)
    return;

  if (child == NULL)
    return;

  if (child == page->priv->more_item)
    {
      show_more (page);
      return;
    }

  new_locale_id = g_object_get_data (G_OBJECT (child), "locale-id");
  set_locale_id (page, new_locale_id);
}

static void
gis_language_page_constructed (GObject *object)
{
  GisLanguagePage *page = GIS_LANGUAGE_PAGE (object);
  GisLanguagePagePrivate *priv = page->priv;

  G_OBJECT_CLASS (gis_language_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("language-page"));

  priv->filter_entry = WID ("language-filter-entry");
  priv->language_list = WID ("language-list");
  priv->more_item = more_widget_new ();

  egg_list_box_set_sort_func (EGG_LIST_BOX (priv->language_list),
                              sort_languages, page, NULL);
  egg_list_box_set_filter_func (EGG_LIST_BOX (priv->language_list),
                                language_visible, page, NULL);
  add_all_languages (page);

  g_signal_connect_swapped (priv->filter_entry, "changed",
                            G_CALLBACK (egg_list_box_refilter),
                            priv->language_list);

  g_signal_connect (priv->language_list, "child-selected",
                    G_CALLBACK (selection_changed), page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);
  gis_page_set_use_arrow_buttons (GIS_PAGE (page), TRUE);
  gis_page_set_title (GIS_PAGE (page), _("Welcome"));

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_language_page_class_init (GisLanguagePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  object_class->constructed = gis_language_page_constructed;

  g_type_class_add_private (object_class, sizeof(GisLanguagePagePrivate));
}

static void
gis_language_page_init (GisLanguagePage *page)
{
  page->priv = GET_PRIVATE (page);
}

void
gis_prepare_language_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_LANGUAGE_PAGE,
                                     "driver", driver,
                                     NULL));
}
