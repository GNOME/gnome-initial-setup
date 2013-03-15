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
#include "language-resources.h"
#include "gis-language-page.h"

#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "cc-common-language.h"
#include "cc-util.h"

#include <glib-object.h>

#include <egg-list-box.h>

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
  GtkWidget *no_results;
  GtkWidget *more_item;
  GtkWidget *page;
  GtkWidget *filter_entry;
  GtkWidget *language_list;
  gboolean adding_languages;
  gboolean showing_extra;
};

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE (page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

typedef struct {
  GtkWidget *box;
  GtkWidget *checkmark;

  gchar *locale_id;
  gchar *locale_name;
  gchar *locale_current_name;
  gchar *locale_untranslated_name;
  gboolean is_extra;
} LanguageWidget;

static LanguageWidget *
get_language_widget (GtkWidget *widget)
{
  return g_object_get_data (G_OBJECT (widget), "language-widget");
}

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
  LanguageWidget *la, *lb;

  la = get_language_widget (GTK_WIDGET (a));
  lb = get_language_widget (GTK_WIDGET (b));

  if (la == NULL)
    return 1;

  if (lb == NULL)
    return -1;

  return strcmp (la->locale_name, lb->locale_name);
}

static GtkWidget *
padded_label_new (char *text)
{
  GtkWidget *widget;
  widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (widget, 10);
  gtk_widget_set_margin_bottom (widget, 10);
  gtk_box_pack_start (GTK_BOX (widget), gtk_label_new (text), FALSE, FALSE, 0);
  gtk_widget_show_all (widget);
  return widget;
}

static void
language_widget_free (gpointer data)
{
  LanguageWidget *widget = data;

  /* This is called when the box is destroyed,
   * so don't bother destroying the widget and
   * children again. */
  g_free (widget->locale_id);
  g_free (widget->locale_name);
  g_free (widget->locale_current_name);
  g_free (widget->locale_untranslated_name);
  g_free (widget);
}

static void
language_widget_sync_show_checkmark (LanguageWidget *widget)
{
  gchar *current_locale_id = cc_common_language_get_current_language ();
  gboolean should_be_visible = g_str_equal (widget->locale_id, current_locale_id);
  gtk_widget_set_visible (widget->checkmark, should_be_visible);
  g_free (current_locale_id);
}

static GtkWidget *
language_widget_new (const char *locale_id,
                     gboolean    is_extra)
{
  gchar *locale_name, *locale_current_name, *locale_untranslated_name;
  LanguageWidget *widget = g_new0 (LanguageWidget, 1);

  locale_name = gnome_get_language_from_locale (locale_id, locale_id);
  locale_current_name = gnome_get_language_from_locale (locale_id, NULL);
  locale_untranslated_name = gnome_get_language_from_locale (locale_id, "C");

  widget->box = padded_label_new (locale_name);
  widget->locale_id = g_strdup (locale_id);
  widget->locale_name = locale_name;
  widget->locale_current_name = locale_current_name;
  widget->locale_untranslated_name = locale_untranslated_name;
  widget->is_extra = is_extra;

  widget->checkmark = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (widget->box), widget->checkmark,
                      FALSE, FALSE, 0);

  language_widget_sync_show_checkmark (widget);

  g_object_set_data_full (G_OBJECT (widget->box), "language-widget", widget,
                          language_widget_free);

  return widget->box;
}

static GtkWidget *
more_widget_new (void)
{
  GtkWidget *widget = padded_label_new ("…");
  gtk_widget_set_tooltip_text (widget, _("More…"));
  return widget;
}

static GtkWidget *
no_results_widget_new (void)
{
  GtkWidget *widget = padded_label_new (_("No languages found"));
  gtk_widget_set_sensitive (widget, FALSE);
  return widget;
}

static void
add_languages (GisLanguagePage *page,
               char           **locale_ids,
               GHashTable      *initial)
{
  GisLanguagePagePrivate *priv = page->priv;

  priv->adding_languages = TRUE;

  while (*locale_ids) {
    const gchar *locale_id;
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
  }

  gtk_container_add (GTK_CONTAINER (priv->language_list),
                     priv->more_item);
  gtk_container_add (GTK_CONTAINER (priv->language_list),
                     priv->no_results);

  gtk_widget_show (priv->language_list);

  priv->adding_languages = FALSE;
}

static void
add_all_languages (GisLanguagePage *page)
{
  char **locale_ids = gnome_get_all_locales ();
  GHashTable *initial =  cc_common_language_get_initial_languages ();

  add_languages (page, locale_ids, initial);

  g_hash_table_destroy (initial);
  g_strfreev (locale_ids);
}

static gboolean
language_visible (GtkWidget *child,
                  gpointer   user_data)
{
  GisLanguagePage *page = user_data;
  GisLanguagePagePrivate *priv = page->priv;
  gchar *locale_name = NULL;
  gchar *locale_current_name = NULL;
  gchar *locale_untranslated_name = NULL;
  gchar *filter_contents = NULL;
  LanguageWidget *widget;
  gboolean visible;

  if (child == priv->more_item)
    return !priv->showing_extra;

  /* We hide this in the after-refilter handler below. */
  if (child == priv->no_results)
    return TRUE;

  widget = get_language_widget (child);

  if (!priv->showing_extra && !widget->is_extra)
    return FALSE;

  filter_contents =
    cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (priv->filter_entry)));

  if (!filter_contents)
    return TRUE;

  visible = FALSE;

  locale_name = cc_util_normalize_casefold_and_unaccent (widget->locale_name);
  if (strstr (locale_name, filter_contents)) {
    visible = TRUE;
    goto out;
  }

  locale_current_name = cc_util_normalize_casefold_and_unaccent (widget->locale_current_name);
  if (strstr (locale_current_name, filter_contents)) {
    visible = TRUE;
    goto out;
  }

  locale_untranslated_name = cc_util_normalize_casefold_and_unaccent (widget->locale_untranslated_name);
  if (strstr (locale_untranslated_name, filter_contents)) {
    visible = TRUE;
    goto out;
  }

 out:
  g_free (filter_contents);
  g_free (locale_untranslated_name);
  g_free (locale_current_name);
  g_free (locale_name);
  return visible;
}

static void
show_more (GisLanguagePage *page)
{
  GisLanguagePagePrivate *priv = page->priv;

  gtk_widget_show (priv->filter_entry);
  gtk_widget_grab_focus (priv->filter_entry);

  priv->showing_extra = TRUE;

  egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));
}

static void
child_activated (EggListBox      *box,
                 GtkWidget       *child,
                 GisLanguagePage *page)
{
  if (page->priv->adding_languages)
    return;

  if (child == NULL)
    return;
  else if (child == page->priv->no_results)
    return;
  else if (child == page->priv->more_item)
    show_more (page);
  else
    {
      LanguageWidget *widget = get_language_widget (child);
      set_locale_id (page, widget->locale_id);
    }
}

typedef struct {
  gint count;
  GtkWidget *ignore;
} CountChildrenData;

static void
count_visible_children (GtkWidget *widget,
                        gpointer   user_data)
{
  CountChildrenData *data = user_data;
  if (widget != data->ignore &&
      gtk_widget_get_child_visible (widget) &&
      gtk_widget_get_visible (widget))
    data->count++;
}

static void
end_refilter (EggListBox *list_box,
              gpointer    user_data)
{
  GisLanguagePage *page = GIS_LANGUAGE_PAGE (user_data);
  GisLanguagePagePrivate *priv = page->priv;

  CountChildrenData data = { 0 };

  data.ignore = priv->no_results;

  gtk_container_foreach (GTK_CONTAINER (list_box),
                         count_visible_children, &data);

  gtk_widget_set_visible (priv->no_results, (data.count == 0));
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
  if (before == NULL)
    return;

  if (*separator == NULL)
    {
      *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      g_object_ref_sink (*separator);
      gtk_widget_show (*separator);
    }
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
  priv->no_results = no_results_widget_new ();

  egg_list_box_set_adjustment (EGG_LIST_BOX (priv->language_list),
                               gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (WID ("language-scrolledwindow"))));

  egg_list_box_set_sort_func (EGG_LIST_BOX (priv->language_list),
                              sort_languages, page, NULL);
  egg_list_box_set_filter_func (EGG_LIST_BOX (priv->language_list),
                                language_visible, page, NULL);
  egg_list_box_set_separator_funcs (EGG_LIST_BOX (priv->language_list),
                                    update_separator_func, page, NULL);

  egg_list_box_set_selection_mode (EGG_LIST_BOX (priv->language_list),
                                   GTK_SELECTION_NONE);
  add_all_languages (page);

  g_signal_connect_swapped (priv->filter_entry, "changed",
                            G_CALLBACK (egg_list_box_refilter),
                            priv->language_list);

  g_signal_connect (priv->language_list, "child-activated",
                    G_CALLBACK (child_activated), page);

  g_signal_connect_after (priv->language_list, "refilter",
                          G_CALLBACK (end_refilter), page);

  egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
sync_checkmark (GtkWidget *child,
                gpointer   user_data)
{
  LanguageWidget *widget = get_language_widget (child);

  if (widget == NULL)
    return;

  language_widget_sync_show_checkmark (widget);
}

static void
gis_language_page_locale_changed (GisPage *page)
{
  GisLanguagePagePrivate *priv = GIS_LANGUAGE_PAGE (page)->priv;

  gis_page_set_title (GIS_PAGE (page), _("Welcome"));

  if (priv->language_list)
    gtk_container_foreach (GTK_CONTAINER (priv->language_list),
                           sync_checkmark, NULL);
}

static void
gis_language_page_class_init (GisLanguagePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_language_page_locale_changed;
  object_class->constructed = gis_language_page_constructed;

  g_type_class_add_private (object_class, sizeof(GisLanguagePagePrivate));
}

static void
gis_language_page_init (GisLanguagePage *page)
{
  g_resources_register (language_get_resource ());
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
