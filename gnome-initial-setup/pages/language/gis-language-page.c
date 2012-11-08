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

#define OBJ(type,name) ((type)gtk_builder_get_object(data->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

typedef struct _LanguageData LanguageData;

struct _LanguageData {
  GisDriver *driver;
  GtkWidget *widget;
  GtkBuilder *builder;

  GtkWidget *show_all;
  GtkWidget *page;
  GtkWidget *filter_entry;
  GtkTreeModel *liststore;
};

enum {
  COL_LOCALE_ID,
  COL_LOCALE_NAME,
  COL_IS_EXTRA,
  NUM_COLS,
};

static void
set_locale_id (LanguageData *data,
               gchar        *new_locale_id)
{
  gchar *old_locale_id = cc_common_language_get_current_language ();
  if (g_strcmp0 (old_locale_id, new_locale_id) != 0) {
    setlocale (LC_MESSAGES, new_locale_id);
    gis_driver_locale_changed (data->driver);
  }
  g_free (old_locale_id);
}

static gint
sort_languages (GtkTreeModel *model,
                GtkTreeIter  *a,
                GtkTreeIter  *b,
                gpointer      data)
{
  char *la, *lb;
  gboolean iea, ieb;
  gint result;

  gtk_tree_model_get (model, a,
                      COL_LOCALE_NAME, &la,
                      COL_IS_EXTRA, &iea,
                      -1);
  gtk_tree_model_get (model, b,
                      COL_LOCALE_NAME, &lb,
                      COL_IS_EXTRA, &ieb,
                      -1);

  if (iea != ieb) {
    return ieb - iea;
  } else {
    result = strcmp (la, lb);
  }

  g_free (la);
  g_free (lb);

  return result;
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

static void
select_locale_id (GtkTreeView *treeview,
                  char        *locale_id)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean cont;

  model = gtk_tree_view_get_model (treeview);
  cont = gtk_tree_model_get_iter_first (model, &iter);
  while (cont) {
    char *iter_locale_id;

    gtk_tree_model_get (model, &iter,
                        COL_LOCALE_ID, &iter_locale_id,
                        -1);

    if (iter_locale_id == NULL)
      continue;

    if (g_str_equal (locale_id, iter_locale_id)) {
      GtkTreeSelection *selection;
      selection = gtk_tree_view_get_selection (treeview);
      gtk_tree_selection_select_iter (selection, &iter);
      g_free (iter_locale_id);
      break;
    }

    g_free (iter_locale_id);
    cont = gtk_tree_model_iter_next (model, &iter);
  }
}

static void
select_current_locale (GtkTreeView *treeview)
{
  gchar *current_locale_id = cc_common_language_get_current_language ();
  select_locale_id (treeview, current_locale_id);
  g_free (current_locale_id);
}

static void
add_languages (GtkListStore *liststore,
               char        **locale_ids,
               GHashTable   *initial)
{
  char *orig_locale_id = cc_common_language_get_current_language ();

  while (*locale_ids) {
    gchar *locale_id;
    gchar *locale_name;
    gboolean is_extra;
    GtkTreeIter iter;

    locale_id = *locale_ids;

    locale_ids ++;

    if (!cc_common_language_has_font (locale_id))
      continue;

    is_extra = (g_hash_table_lookup (initial, locale_id) != NULL);
    locale_name = use_language (locale_id);

    gtk_list_store_insert_with_values (liststore, &iter, -1,
                                       COL_LOCALE_ID, locale_id,
                                       COL_LOCALE_NAME, locale_name,
                                       COL_IS_EXTRA, is_extra,
                                       -1);
  }

  setlocale (LC_MESSAGES, orig_locale_id);
  g_free (orig_locale_id);
}

static void
add_all_languages (GtkListStore *liststore)
{
  char **locale_ids = gdm_get_all_language_names ();
  GHashTable *initial =  cc_common_language_get_initial_languages ();

  add_languages (liststore, locale_ids, initial);
}

static gboolean
language_visible (GtkTreeModel *model,
                  GtkTreeIter  *iter,
                  gpointer      user_data)
{
  gchar *locale_name;
  const gchar *filter_contents;
  LanguageData *data = user_data;
  gboolean visible = TRUE;
  gboolean is_extra;

  gtk_tree_model_get (model, iter,
                      COL_LOCALE_NAME, &locale_name,
                      COL_IS_EXTRA, &is_extra,
                      -1);

  filter_contents = gtk_entry_get_text (GTK_ENTRY (data->filter_entry));
  if (*filter_contents && strcasestr (locale_name, filter_contents) == NULL)
    {
      visible = FALSE;
      goto out;
    }

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->show_all)) && !is_extra)
    {
      visible = FALSE;
      goto out;
    }

 out:
  g_free (locale_name);
  return visible;
}

static void
selection_changed (GtkTreeSelection *selection,
                   LanguageData     *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *new_locale_id;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      COL_LOCALE_ID, &new_locale_id,
                      -1);

  set_locale_id (data, new_locale_id);
}

void
gis_prepare_language_page (GisDriver *driver)
{
  LanguageData *data;
  GisAssistant *assistant = gis_driver_get_assistant (driver);
  GtkListStore *liststore;
  GtkTreeModel *filter;
  GtkTreeView *treeview;

  data = g_slice_new0 (LanguageData);
  data->driver = driver;
  data->builder = gis_builder (PAGE_ID);
  data->widget = WID ("language-page");

  liststore = gtk_list_store_new (NUM_COLS,
                                  G_TYPE_STRING,
                                  G_TYPE_STRING,
                                  G_TYPE_BOOLEAN);

  data->show_all = WID ("language-show-all");
  data->filter_entry = WID ("language-filter-entry");
  data->liststore = GTK_TREE_MODEL (liststore);
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (liststore),
                                           sort_languages, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (liststore),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);

  treeview = OBJ (GtkTreeView *, "language-list");

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (liststore), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          language_visible, data, NULL);
  gtk_tree_view_set_model (treeview, filter);

  add_all_languages (GTK_LIST_STORE (data->liststore));

  g_signal_connect_swapped (data->show_all, "toggled",
                            G_CALLBACK (gtk_tree_model_filter_refilter),
                            filter);

  g_signal_connect_swapped (data->filter_entry, "changed",
                            G_CALLBACK (gtk_tree_model_filter_refilter),
                            filter);

  g_signal_connect (gtk_tree_view_get_selection (treeview), "changed",
                    G_CALLBACK (selection_changed), data);

  gis_assistant_add_page (assistant, data->widget);
  gis_assistant_set_use_arrow_buttons (assistant, data->widget, TRUE);
  gis_assistant_set_page_complete (assistant, data->widget, TRUE);

  gis_assistant_set_page_title (assistant, data->widget, _("Welcome"));
  select_current_locale (treeview);
}
