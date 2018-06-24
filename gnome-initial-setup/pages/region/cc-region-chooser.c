/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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
 *     Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"
#include "cc-region-chooser.h"

#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "cc-common-language.h"

#include <glib-object.h>

#define MIN_ROWS 6

struct _CcRegionChooserPrivate
{
        GtkWidget *filter_entry;
        GtkWidget *region_list;

        GtkWidget *scrolled_window;
        GtkWidget *no_results;
        GtkWidget *more_item;

	GHashTable *regions;

        gboolean showing_extra;
        gchar *locale;
	gchar *lang;
};
typedef struct _CcRegionChooserPrivate CcRegionChooserPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (CcRegionChooser, cc_region_chooser, GTK_TYPE_BOX);

enum {
        PROP_0,
        PROP_LOCALE,
        PROP_SHOWING_EXTRA,
        PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

enum {
        CONFIRM,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
        GtkWidget *box;
        GtkWidget *checkmark;

        gchar *locale_id;
        gchar *locale_name;
        gchar *locale_current_name;
        gchar *locale_untranslated_name;
        gboolean is_extra;
} RegionWidget;

static RegionWidget *
get_region_widget (GtkWidget *widget)
{
        return g_object_get_data (G_OBJECT (widget), "region-widget");
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
        return widget;
}

static void
region_widget_free (gpointer data)
{
        RegionWidget *widget = data;

        /* This is called when the box is destroyed,
         * so don't bother destroying the widget and
         * children again. */
        g_free (widget->locale_id);
        g_free (widget->locale_name);
        g_free (widget->locale_current_name);
        g_free (widget->locale_untranslated_name);
        g_free (widget);
}

static char *
get_country_name (const char *locale,
		  const char *translation)
{
	char *name;
	char *p;

        name = gnome_get_country_from_locale (locale, translation);
	p = strstr (name, " (");
	if (p)
		*p = '\0';

	return name;
}

static GtkWidget *
region_widget_new (const char *locale_id,
		   const char *region,
                   gboolean    is_extra)
{
        gchar *locale_name, *locale_current_name, *locale_untranslated_name;
	GtkWidget *label;
        RegionWidget *widget = g_new0 (RegionWidget, 1);

        locale_name = get_country_name (locale_id, locale_id);
        locale_current_name = get_country_name (locale_id, NULL);
        locale_untranslated_name = get_country_name (locale_id, "C");

        widget->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_top (widget->box, 10);
        gtk_widget_set_margin_bottom (widget->box, 10);
        gtk_widget_set_margin_start (widget->box, 10);
        gtk_widget_set_margin_end (widget->box, 10);
        gtk_widget_set_halign (widget->box, GTK_ALIGN_FILL);
	label = gtk_label_new (locale_name);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_label_set_width_chars (GTK_LABEL (label), 40);
        gtk_box_pack_start (GTK_BOX (widget->box), label, FALSE, FALSE, 0);
        widget->locale_id = g_strdup (locale_id);
        widget->locale_name = locale_name;
        widget->locale_current_name = locale_current_name;
        widget->locale_untranslated_name = locale_untranslated_name;
        widget->is_extra = is_extra;

        widget->checkmark = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
        gtk_box_pack_start (GTK_BOX (widget->box), widget->checkmark, TRUE, TRUE, 0);
	gtk_widget_set_halign (widget->checkmark, GTK_ALIGN_END);
        gtk_widget_show_all (widget->box);

        g_object_set_data_full (G_OBJECT (widget->box), "region-widget", widget,
                                region_widget_free);

        return widget->box;
}

static void
sync_checkmark (GtkWidget *row,
                gpointer   user_data)
{
        GtkWidget *child;
        RegionWidget *widget;
        gchar *locale_id;
        gboolean should_be_visible;

        child = gtk_bin_get_child (GTK_BIN (row));
        widget = get_region_widget (child);

        if (widget == NULL)
                return;

        locale_id = user_data;
        should_be_visible = g_str_equal (widget->locale_id, locale_id);
        gtk_widget_set_opacity (widget->checkmark, should_be_visible ? 1.0 : 0.0);
}

static void
sync_all_checkmarks (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);

        gtk_container_foreach (GTK_CONTAINER (priv->region_list),
                               sync_checkmark, priv->locale);
}

static GtkWidget *
more_widget_new (void)
{
        GtkWidget *widget;
        GtkWidget *arrow;

        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_tooltip_text (widget, _("More…"));

        arrow = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_MENU);
        gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
        gtk_widget_set_margin_top (widget, 10);
        gtk_widget_set_margin_bottom (widget, 10);
        gtk_box_pack_start (GTK_BOX (widget), arrow, TRUE, TRUE, 0);
        gtk_widget_show_all (widget);

        return widget;
}

static GtkWidget *
no_results_widget_new (void)
{
        GtkWidget *widget;

        widget = padded_label_new (_("No regions found"));
        gtk_widget_set_sensitive (widget, FALSE);
        return widget;
}

static void
add_one_region (CcRegionChooser *chooser,
                const char        *locale_id)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
	GtkWidget *widget;
	gchar *lang = NULL;
	gchar *region = NULL;
	gboolean is_extra;
	
	if (!g_str_has_suffix (locale_id, "utf8")) {
		return;
	}

	if (!cc_common_language_has_font (locale_id)) {
		return;
	}

	if (!gnome_parse_locale (locale_id, &lang, &region, NULL, NULL)) {
		goto out;
	}

	if (g_strcmp0 (priv->lang, lang) != 0) {
		goto out;
	}

 	if (region == NULL) {
		goto out;
	}

	if (g_hash_table_contains (priv->regions, region)) {
		goto out;
	}
	g_hash_table_add (priv->regions, g_strdup (region));
	if (g_hash_table_size (priv->regions) > MIN_ROWS)
		is_extra = TRUE;
	else
		is_extra = FALSE;

	widget = region_widget_new (locale_id, region, is_extra);
	gtk_container_add (GTK_CONTAINER (priv->region_list), widget);

out:
	g_free (lang);
	g_free (region);
}

static void
add_regions (CcRegionChooser  *chooser,
               char               **locale_ids,
               GHashTable          *initial)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
	GHashTableIter iter;
	gchar *key;

	add_one_region (chooser, priv->locale);

	g_hash_table_iter_init (&iter, initial);
	while (g_hash_table_iter_next (&iter, (gpointer *)&key, NULL)) {
		add_one_region (chooser, key);
	}

        while (*locale_ids) {
                const gchar *locale_id;

                locale_id = *locale_ids;
                locale_ids ++;

		add_one_region (chooser, locale_id);
        }

        gtk_widget_show_all (priv->region_list);
}

static void
add_all_regions (CcRegionChooser *chooser)
{
        char **locale_ids;
        GHashTable *initial;

        locale_ids = gnome_get_all_locales ();
        initial = cc_common_language_get_initial_languages ();
        add_regions (chooser, locale_ids, initial);
        g_hash_table_destroy (initial);
        g_strfreev (locale_ids);
}

static gboolean
region_visible (GtkListBoxRow *row,
                  gpointer       user_data)
{
        CcRegionChooser *chooser = user_data;
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
        RegionWidget *widget;
        gboolean visible;
        GtkWidget *child;
        const char *search_term;

        child = gtk_bin_get_child (GTK_BIN (row));
        if (child == priv->more_item)
                return !priv->showing_extra && g_hash_table_size (priv->regions) > MIN_ROWS;

        widget = get_region_widget (child);

        if (!priv->showing_extra && widget->is_extra)
                return FALSE;

        search_term = gtk_entry_get_text (GTK_ENTRY (priv->filter_entry));
        if (!search_term || !*search_term)
                return TRUE;

        visible = FALSE;

        visible = g_str_match_string (search_term, widget->locale_name, TRUE);
        if (visible)
                goto out;

        visible = g_str_match_string (search_term, widget->locale_current_name, TRUE);
        if (visible)
                goto out;

        visible = g_str_match_string (search_term, widget->locale_untranslated_name, TRUE);
        if (visible)
                goto out;

 out:
        return visible;
}

static gint
sort_regions (GtkListBoxRow *a,
                GtkListBoxRow *b,
                gpointer       data)
{
        RegionWidget *la, *lb;

        la = get_region_widget (gtk_bin_get_child (GTK_BIN (a)));
        lb = get_region_widget (gtk_bin_get_child (GTK_BIN (b)));

        if (la == NULL)
                return 1;

        if (lb == NULL)
                return -1;

        if (la->is_extra && !lb->is_extra)
                return 1;

        if (!la->is_extra && lb->is_extra)
                return -1;

        return strcmp (la->locale_name, lb->locale_name);
}

static void
filter_changed (GtkEntry        *entry,
                CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->region_list));
}

static void
show_more (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
	if (g_hash_table_size (priv->regions) <= MIN_ROWS)
		return;

        gtk_widget_show (priv->filter_entry);
        gtk_widget_grab_focus (priv->filter_entry);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_valign (GTK_WIDGET (chooser), GTK_ALIGN_FILL);

        priv->showing_extra = TRUE;
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->region_list));
        g_object_notify_by_pspec (G_OBJECT (chooser), obj_props[PROP_SHOWING_EXTRA]);
}

static void
show_less (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);

        gtk_widget_hide (priv->filter_entry);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_widget_set_valign (GTK_WIDGET (chooser), GTK_ALIGN_START);

        priv->showing_extra = FALSE;
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->region_list));
        g_object_notify_by_pspec (G_OBJECT (chooser), obj_props[PROP_SHOWING_EXTRA]);
}

static void
remove_regions (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
	GtkContainer *container;
	GList *children, *l;

	container = GTK_CONTAINER (priv->region_list);

	children = gtk_container_get_children (container);
	for (l = children; l; l = l->next) {
		if (l->data != gtk_widget_get_parent (priv->more_item))
			gtk_container_remove (container, l->data);
	}
	g_list_free (children);

	g_hash_table_remove_all (priv->regions);
}

static void
set_locale_id (CcRegionChooser *chooser,
               const gchar       *new_locale_id)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
	gchar *new_lang;

        if (g_strcmp0 (priv->locale, new_locale_id) == 0)
                return;

        g_free (priv->locale);
        priv->locale = g_strdup (new_locale_id);

	gnome_parse_locale (new_locale_id, &new_lang, NULL, NULL, NULL);
	if (g_strcmp0 (priv->lang, new_lang) != 0) {
		g_free (priv->lang);
		priv->lang = g_strdup (new_lang);

		remove_regions (chooser);
		show_less (chooser);
	        add_all_regions (chooser);
	}
	g_free (new_lang);

        sync_all_checkmarks (chooser);

        g_object_notify_by_pspec (G_OBJECT (chooser), obj_props[PROP_LOCALE]);
}

static gboolean
confirm_choice (gpointer data)
{
        GtkWidget *widget = data;

        g_signal_emit (widget, signals[CONFIRM], 0);

        return G_SOURCE_REMOVE;
}

static void
row_activated (GtkListBox        *box,
               GtkListBoxRow     *row,
               CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
        GtkWidget *child;
        RegionWidget *widget;

        if (row == NULL)
                return;

        child = gtk_bin_get_child (GTK_BIN (row));
        if (child == priv->more_item) {
                show_more (chooser);
        } else {
                widget = get_region_widget (child);
                if (widget == NULL)
                        return;
                if (g_strcmp0 (priv->locale, widget->locale_id) == 0)
                        g_idle_add (confirm_choice, chooser);
                else
                        set_locale_id (chooser, widget->locale_id);
        }
}

static void
update_header_func (GtkListBoxRow *child,
                    GtkListBoxRow *before,
                    gpointer       user_data)
{
        GtkWidget *header;

        if (before == NULL)
                return;

        header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_list_box_row_set_header (child, header);
        gtk_widget_show (header);
}

static void
cc_region_chooser_constructed (GObject *object)
{
        CcRegionChooser *chooser = CC_REGION_CHOOSER (object);
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);

        G_OBJECT_CLASS (cc_region_chooser_parent_class)->constructed (object);

        priv->more_item = more_widget_new ();

        priv->no_results = no_results_widget_new ();

	priv->regions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->region_list),
                                    sort_regions, chooser, NULL);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (priv->region_list),
                                      region_visible, chooser, NULL);
        gtk_list_box_set_header_func (GTK_LIST_BOX (priv->region_list),
                                      update_header_func, chooser, NULL);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->region_list),
                                         GTK_SELECTION_NONE);

        gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->region_list), priv->no_results);

        if (priv->locale == NULL) {
		priv->locale = cc_common_language_get_current_language ();
		gnome_parse_locale (priv->locale, &priv->lang, NULL, NULL, NULL);
	}

	add_all_regions (chooser);

	gtk_container_add (GTK_CONTAINER (priv->region_list), priv->more_item);

        g_signal_connect (priv->filter_entry, "changed",
                          G_CALLBACK (filter_changed),
                          chooser);

        g_signal_connect (priv->region_list, "row-activated",
                          G_CALLBACK (row_activated), chooser);

        sync_all_checkmarks (chooser);
}

static void
cc_region_chooser_finalize (GObject *object)
{
	CcRegionChooser *chooser = CC_REGION_CHOOSER (object);
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);

	g_hash_table_unref (priv->regions);

	G_OBJECT_CLASS (cc_region_chooser_parent_class)->finalize (object);
}

static void
cc_region_chooser_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
        CcRegionChooser *chooser = CC_REGION_CHOOSER (object);
        switch (prop_id) {
        case PROP_LOCALE:
                g_value_set_string (value, cc_region_chooser_get_locale (chooser));
                break;
        case PROP_SHOWING_EXTRA:
                g_value_set_boolean (value, cc_region_chooser_get_showing_extra (chooser));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_region_chooser_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        CcRegionChooser *chooser = CC_REGION_CHOOSER (object);
        switch (prop_id) {
        case PROP_LOCALE:
                cc_region_chooser_set_locale (chooser, g_value_get_string (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_region_chooser_map (GtkWidget *widget)
{
        CcRegionChooser *chooser = CC_REGION_CHOOSER (widget);
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);

        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->region_list));

        GTK_WIDGET_CLASS (cc_region_chooser_parent_class)->map (widget);
}

static void
cc_region_chooser_class_init (CcRegionChooserClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/control-center/region-chooser.ui");

        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), CcRegionChooser, filter_entry);
        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), CcRegionChooser, region_list);
        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), CcRegionChooser, scrolled_window);

	object_class->finalize = cc_region_chooser_finalize;
        object_class->get_property = cc_region_chooser_get_property;
        object_class->set_property = cc_region_chooser_set_property;
        object_class->constructed = cc_region_chooser_constructed;

        signals[CONFIRM] = g_signal_new ("confirm",
                                         G_TYPE_FROM_CLASS (object_class),
                                         G_SIGNAL_RUN_FIRST,
                                         G_STRUCT_OFFSET (CcRegionChooserClass, confirm),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__VOID,
                                         G_TYPE_NONE, 0);

        widget_class->map = cc_region_chooser_map;

        obj_props[PROP_LOCALE] =
                g_param_spec_string ("locale", "", "", "",
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

        obj_props[PROP_SHOWING_EXTRA] =
                g_param_spec_string ("showing-extra", "", "", "",
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
cc_region_chooser_init (CcRegionChooser *chooser)
{
        gtk_widget_init_template (GTK_WIDGET (chooser));
}

void
cc_region_chooser_clear_filter (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
        gtk_entry_set_text (GTK_ENTRY (priv->filter_entry), "");
}

const gchar *
cc_region_chooser_get_locale (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
        return priv->locale;
}

void
cc_region_chooser_set_locale (CcRegionChooser *chooser,
                              const gchar     *locale)
{
        set_locale_id (chooser, locale);
}

gboolean
cc_region_chooser_get_showing_extra (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
        return priv->showing_extra;
}

gint
cc_region_chooser_get_n_regions (CcRegionChooser *chooser)
{
        CcRegionChooserPrivate *priv = cc_region_chooser_get_instance_private (chooser);
	return g_hash_table_size (priv->regions);
}
