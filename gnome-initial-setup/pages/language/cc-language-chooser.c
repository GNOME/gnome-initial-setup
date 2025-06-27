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
#include "cc-language-chooser.h"

#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "cc-common-language.h"
#include "cc-util.h"

#include <glib-object.h>

struct _CcLanguageChooserPrivate
{
        GtkWidget *filter_entry;
        GtkWidget *language_list;

        GtkWidget *no_results;
        GtkWidget *more_item;

        gboolean showing_extra;
        gchar *language;
};
typedef struct _CcLanguageChooserPrivate CcLanguageChooserPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (CcLanguageChooser, cc_language_chooser, GTK_TYPE_BOX);

enum {
        PROP_0,
        PROP_LANGUAGE,
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
        GtkWidget *a11y_label;

        gchar *locale_id;
        gchar *locale_name;
        gchar *locale_current_name;
        gchar *locale_untranslated_name;
        gchar *sort_key;
        gboolean is_extra;
} LanguageWidget;

static LanguageWidget *
get_language_widget (GtkWidget *widget)
{
        return g_object_get_data (G_OBJECT (widget), "language-widget");
}

static GtkWidget *
padded_label_new (char *text)
{
        GtkWidget *widget;
        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top (widget, 12);
        gtk_widget_set_margin_bottom (widget, 12);
        gtk_box_append (GTK_BOX (widget), gtk_label_new (text));
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
        g_free (widget->sort_key);
        g_free (widget);
}

static void
language_widget_set_selected (LanguageWidget *widget, gboolean selected)
{
        gtk_widget_set_opacity (widget->checkmark, selected ? 1.0 : 0.0);
        gtk_accessible_update_state (GTK_ACCESSIBLE (widget->a11y_label),
                                     GTK_ACCESSIBLE_STATE_CHECKED, selected,
                                     -1);
}

static GtkWidget *
language_widget_new (const char *locale_id,
                     gboolean    is_extra)
{
	GtkWidget *label;
        gchar *locale_name, *locale_current_name, *locale_untranslated_name;
        gchar *language = NULL;
        gchar *language_name;
        gchar *country = NULL;
        gchar *country_name = NULL;
        LanguageWidget *widget = g_new0 (LanguageWidget, 1);

        if (!gnome_parse_locale (locale_id, &language, &country, NULL, NULL))
                return NULL;

        language_name = gnome_get_language_from_code (language, locale_id);
        if (language_name == NULL)
                language_name = gnome_get_language_from_code (language, NULL);

        if (country) {
                country_name = gnome_get_country_from_code (country, locale_id);
                if (country_name == NULL)
                        country_name = gnome_get_country_from_code (country, NULL);
        }

        locale_name = gnome_get_language_from_locale (locale_id, locale_id);
        locale_current_name = gnome_get_language_from_locale (locale_id, NULL);
        locale_untranslated_name = gnome_get_language_from_locale (locale_id, "C");

        widget->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_top (widget->box, 12);
        gtk_widget_set_margin_bottom (widget->box, 12);
        gtk_widget_set_margin_start (widget->box, 12);
        gtk_widget_set_margin_end (widget->box, 12);
        gtk_widget_set_halign (widget->box, GTK_ALIGN_FILL);

        label = gtk_label_new (language_name);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars (GTK_LABEL (label), 30);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_box_append (GTK_BOX (widget->box), label);

        widget->checkmark = gtk_image_new_from_icon_name ("object-select-symbolic");
        gtk_box_append (GTK_BOX (widget->box), widget->checkmark);

        if (country_name) {
                label = gtk_label_new (country_name);
                gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
                gtk_label_set_max_width_chars (GTK_LABEL (label), 30);
                gtk_widget_add_css_class (label, "dim-label");
                gtk_label_set_xalign (GTK_LABEL (label), 0);
                gtk_widget_set_hexpand (label, TRUE);
                gtk_widget_set_halign (label, GTK_ALIGN_END);
                gtk_box_append (GTK_BOX (widget->box), label);
        }

        widget->locale_id = g_strdup (locale_id);
        widget->locale_name = locale_name;
        widget->locale_current_name = locale_current_name;
        widget->locale_untranslated_name = locale_untranslated_name;
        widget->is_extra = is_extra;
        widget->sort_key = cc_util_normalize_casefold_and_unaccent (locale_name);

        g_object_set_data_full (G_OBJECT (widget->box), "language-widget", widget,
                                language_widget_free);

        g_free (language);
        g_free (language_name);
        g_free (country);
        g_free (country_name);
        widget->a11y_label = label;
        // this allows to have the "checked" and "unchecked" states
        g_object_set (G_OBJECT (label),
                      "accessible-role",
                      GTK_ACCESSIBLE_ROLE_CHECKBOX,
                      NULL);

        return widget->box;
}

static void
sync_all_checkmarks (CcLanguageChooser *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
        GtkWidget *row;

        row = gtk_widget_get_first_child (priv->language_list);
        while (row) {
                LanguageWidget *widget;
                GtkWidget *child;
                gboolean should_be_visible;

                child = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row));
                widget = get_language_widget (child);

                if (widget == NULL)
                        return;

                should_be_visible = g_str_equal (widget->locale_id, priv->language);
                language_widget_set_selected (widget, should_be_visible);

                row = gtk_widget_get_next_sibling (row);
        }
}

static GtkWidget *
more_widget_new (void)
{
        GtkWidget *widget;
        GtkWidget *arrow;

        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        g_object_set (G_OBJECT (widget),
                      "accessible-role",
                      GTK_ACCESSIBLE_ROLE_GROUP,
                      NULL);
        gtk_widget_set_tooltip_text (widget, _("More…"));

        arrow = g_object_new (GTK_TYPE_IMAGE,
                              "icon-name", "view-more-symbolic",
                              "hexpand", TRUE,
                              "halign", GTK_ALIGN_CENTER,
                              NULL);
        gtk_widget_add_css_class (arrow, "dim-label");
        gtk_widget_set_margin_top (widget, 12);
        gtk_widget_set_margin_bottom (widget, 12);
        gtk_box_append (GTK_BOX (widget), arrow);

        return widget;
}

static GtkWidget *
no_results_widget_new (void)
{
        GtkWidget *widget;

        widget = padded_label_new (_("No languages found"));
        gtk_widget_set_sensitive (widget, FALSE);

        return widget;
}

static void
add_one_language (CcLanguageChooser *chooser,
                  const char        *locale_id,
                  gboolean           is_initial)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
	GtkWidget *widget;

	if (!cc_common_language_has_font (locale_id)) {
		return;
	}

	widget = language_widget_new (locale_id, !is_initial);
        if (widget)
                gtk_list_box_append (GTK_LIST_BOX (priv->language_list), widget);
}

static void
add_languages (CcLanguageChooser  *chooser,
               char               **locale_ids,
               GHashTable          *initial)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
	GHashTableIter iter;
	gchar *key;

	g_hash_table_iter_init (&iter, initial);
	while (g_hash_table_iter_next (&iter, (gpointer *)&key, NULL)) {
		add_one_language (chooser, key, TRUE);
	}

        while (*locale_ids) {
                const gchar *locale_id;

                locale_id = *locale_ids;
                locale_ids ++;

                if (!g_hash_table_lookup (initial, locale_id))
                        add_one_language (chooser, locale_id, FALSE);
        }

        gtk_list_box_append (GTK_LIST_BOX (priv->language_list), priv->more_item);
        gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->language_list), priv->no_results);
}

static void
add_all_languages (CcLanguageChooser *chooser)
{
        g_auto(GStrv) locale_ids = NULL;
        g_autoptr(GHashTable) initial = NULL;

        locale_ids = gnome_get_all_locales ();
        initial = cc_common_language_get_initial_languages ();
        add_languages (chooser, locale_ids, initial);
}

static gboolean
language_visible (GtkListBoxRow *row,
                  gpointer       user_data)
{
        CcLanguageChooser *chooser = user_data;
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
        LanguageWidget *widget;
        gboolean visible;
        GtkWidget *child;
        const char *search_term;

        child = gtk_list_box_row_get_child (row);
        if (child == priv->more_item)
                return !priv->showing_extra;

        widget = get_language_widget (child);

        if (!priv->showing_extra && widget->is_extra)
                return FALSE;

        search_term = gtk_editable_get_text (GTK_EDITABLE (priv->filter_entry));
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
sort_languages (GtkListBoxRow *a,
                GtkListBoxRow *b,
                gpointer       data)
{
        LanguageWidget *la, *lb;
        int ret;

        la = get_language_widget (gtk_list_box_row_get_child (a));
        lb = get_language_widget (gtk_list_box_row_get_child (b));

        if (la == NULL)
                return 1;

        if (lb == NULL)
                return -1;

        if (la->is_extra && !lb->is_extra)
                return 1;

        if (!la->is_extra && lb->is_extra)
                return -1;

        ret = g_strcmp0 (la->sort_key, lb->sort_key);
        if (ret != 0)
                return ret;

        return g_strcmp0 (la->locale_id, lb->locale_id);
}

static void
filter_changed (GtkEntry        *entry,
                CcLanguageChooser *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->language_list));
}

static void
show_more (CcLanguageChooser *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);

        gtk_widget_grab_focus (priv->filter_entry);

	gtk_widget_set_valign (GTK_WIDGET (chooser), GTK_ALIGN_FILL);

        priv->showing_extra = TRUE;
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->language_list));
        g_object_notify_by_pspec (G_OBJECT (chooser), obj_props[PROP_SHOWING_EXTRA]);
}

static void
set_locale_id (CcLanguageChooser *chooser,
               const gchar       *new_locale_id)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);

        if (g_strcmp0 (priv->language, new_locale_id) == 0)
                return;

        g_free (priv->language);
        priv->language = g_strdup (new_locale_id);

        sync_all_checkmarks (chooser);

        g_object_notify_by_pspec (G_OBJECT (chooser), obj_props[PROP_LANGUAGE]);
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
               CcLanguageChooser *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
        GtkWidget *child;
        LanguageWidget *widget;

        if (row == NULL)
                return;

        child = gtk_list_box_row_get_child (row);
        if (child == priv->more_item) {
                show_more (chooser);
        } else {
                widget = get_language_widget (child);
                if (widget == NULL)
                        return;
                // set it here to ensure that screen readers say it correctly
                language_widget_set_selected (widget, TRUE);
                if (g_strcmp0 (priv->language, widget->locale_id) == 0)
			g_idle_add (confirm_choice, chooser);
                else
                        set_locale_id (chooser, widget->locale_id);
        }
}

static void
cc_language_chooser_constructed (GObject *object)
{
        CcLanguageChooser *chooser = CC_LANGUAGE_CHOOSER (object);
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);

        G_OBJECT_CLASS (cc_language_chooser_parent_class)->constructed (object);

        priv->more_item = more_widget_new ();
        priv->no_results = no_results_widget_new ();

        gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->language_list),
                                    sort_languages, chooser, NULL);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (priv->language_list),
                                      language_visible, chooser, NULL);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->language_list),
                                         GTK_SELECTION_NONE);
        add_all_languages (chooser);

        g_signal_connect (priv->filter_entry, "changed",
                          G_CALLBACK (filter_changed),
                          chooser);

        g_signal_connect (priv->language_list, "row-activated",
                          G_CALLBACK (row_activated), chooser);

        if (priv->language == NULL)
                priv->language = cc_common_language_get_current_language ();

        sync_all_checkmarks (chooser);
}

static void
cc_language_chooser_finalize (GObject *object)
{
	CcLanguageChooser *chooser = CC_LANGUAGE_CHOOSER (object);
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);

        g_free (priv->language);

	G_OBJECT_CLASS (cc_language_chooser_parent_class)->finalize (object);
}

static void
cc_language_chooser_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
        CcLanguageChooser *chooser = CC_LANGUAGE_CHOOSER (object);
        switch (prop_id) {
        case PROP_LANGUAGE:
                g_value_set_string (value, cc_language_chooser_get_language (chooser));
                break;
        case PROP_SHOWING_EXTRA:
                g_value_set_boolean (value, cc_language_chooser_get_showing_extra (chooser));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_language_chooser_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        CcLanguageChooser *chooser = CC_LANGUAGE_CHOOSER (object);
        switch (prop_id) {
        case PROP_LANGUAGE:
                cc_language_chooser_set_language (chooser, g_value_get_string (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static gboolean
cc_language_chooser_grab_focus (GtkWidget *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (CC_LANGUAGE_CHOOSER (chooser));
        return gtk_widget_grab_focus (GTK_WIDGET (priv->filter_entry));
}

static void
cc_language_chooser_class_init (CcLanguageChooserClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/control-center/language-chooser.ui");

        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), CcLanguageChooser, filter_entry);
        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), CcLanguageChooser, language_list);

        widget_class->grab_focus = cc_language_chooser_grab_focus;
	object_class->finalize = cc_language_chooser_finalize;
        object_class->get_property = cc_language_chooser_get_property;
        object_class->set_property = cc_language_chooser_set_property;
        object_class->constructed = cc_language_chooser_constructed;

        signals[CONFIRM] = g_signal_new ("confirm",
                                         G_TYPE_FROM_CLASS (object_class),
                                         G_SIGNAL_RUN_FIRST,
                                         G_STRUCT_OFFSET (CcLanguageChooserClass, confirm),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__VOID,
                                         G_TYPE_NONE, 0);

        obj_props[PROP_LANGUAGE] =
                g_param_spec_string ("language", "", "", "",
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

        obj_props[PROP_SHOWING_EXTRA] =
                g_param_spec_string ("showing-extra", "", "", "",
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
cc_language_chooser_init (CcLanguageChooser *chooser)
{
        gtk_widget_init_template (GTK_WIDGET (chooser));
}

void
cc_language_chooser_clear_filter (CcLanguageChooser *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
        gtk_editable_set_text (GTK_EDITABLE (priv->filter_entry), "");
}

const gchar *
cc_language_chooser_get_language (CcLanguageChooser *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
        return priv->language;
}

void
cc_language_chooser_set_language (CcLanguageChooser *chooser,
                                  const gchar        *language)
{
        set_locale_id (chooser, language);
}

gboolean
cc_language_chooser_get_showing_extra (CcLanguageChooser *chooser)
{
        CcLanguageChooserPrivate *priv = cc_language_chooser_get_instance_private (chooser);
        return priv->showing_extra;
}
