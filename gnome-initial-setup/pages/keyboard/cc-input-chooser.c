/*
 * Copyright (C) 2013 Red Hat, Inc.
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
#include "cc-input-chooser.h"

#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#include "cc-ibus-utils.h"
#endif

#include "cc-common-language.h"

#include <glib-object.h>

#define INPUT_SOURCE_TYPE_XKB "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

#define MIN_ROWS 5

struct _CcInputChooserPrivate
{
        GtkWidget *filter_entry;
        GtkWidget *input_list;
        GPtrArray *input_widget_boxes;
	GHashTable *inputs;

        GtkWidget *no_results;
        GtkWidget *more_item;

        gboolean showing_extra;
	gchar *locale;
        gchar *id;
	gchar *type;
	GnomeXkbInfo *xkb_info;
#ifdef HAVE_IBUS
        IBusBus *ibus;
        GHashTable *ibus_engines;
        GCancellable *ibus_cancellable;
#endif
};
typedef struct _CcInputChooserPrivate CcInputChooserPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (CcInputChooser, cc_input_chooser, GTK_TYPE_BOX);

enum {
        PROP_0,
        PROP_SHOWING_EXTRA,
        PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

enum {
	CHANGED,
        CONFIRM,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *checkmark;

        gchar *id;
        gchar *type;
        gchar *name;
        gboolean is_extra;
} InputWidget;

/*
 * Invariant: for each box in priv->input_widget_boxes,
 * get_input_widget (row) is non-null and
 * get_input_widget (row)->box == box
 */
static InputWidget *
get_input_widget (GtkWidget *widget)
{
        return g_object_get_data (G_OBJECT (widget), "input-widget");
}

static GtkWidget *
padded_label_new (char *text)
{
        GtkWidget *widget;
        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top (widget, 10);
        gtk_widget_set_margin_bottom (widget, 10);
        gtk_box_append (GTK_BOX (widget), gtk_label_new (text));
        return widget;
}

static void
input_widget_free (gpointer data)
{
        InputWidget *widget = data;

        g_free (widget->id);
        g_free (widget->type);
        g_free (widget->name);
        g_free (widget);
}

static gboolean
get_layout (CcInputChooser *chooser,
            const gchar    *type,
	    const gchar	   *id,
	    const gchar   **layout,
            const gchar   **variant)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

	if (g_strcmp0 (type, INPUT_SOURCE_TYPE_XKB) == 0) {
		gnome_xkb_info_get_layout_info (priv->xkb_info,
						id, NULL, NULL,
						layout, variant);
                return TRUE;
        }
#ifdef HAVE_IBUS
	if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) == 0) {
                IBusEngineDesc *engine_desc = NULL;

		if (priv->ibus_engines)
			engine_desc = g_hash_table_lookup (priv->ibus_engines, id);

		if (!engine_desc)
                        return FALSE;

                *layout = ibus_engine_desc_get_layout (engine_desc);
                *variant = "";
                return TRUE;
	}
#endif
	return FALSE;
}

static gboolean
preview_cb (GtkLabel       *label,
	    const gchar    *uri,
	    CcInputChooser *chooser)
{
	GtkWidget *row;
	InputWidget *widget;
	const gchar *layout;
	const gchar *variant;
	gchar *commandline;

	row = gtk_widget_get_parent (GTK_WIDGET (label));
	widget = get_input_widget (row);

	if (!get_layout (chooser, widget->type, widget->id, &layout, &variant))
		return TRUE;

	if (variant[0])
		commandline = g_strdup_printf ("tecla \"%s+%s\"", layout, variant);
	else
		commandline = g_strdup_printf ("tecla %s", layout);
	g_spawn_command_line_async (commandline, NULL);
	g_free (commandline);

	return TRUE;
}

static void
input_widget_set_selected (InputWidget *widget, gboolean selected)
{
        gtk_widget_set_opacity (widget->checkmark, selected ? 1.0 : 0.0);
        gtk_accessible_update_state (GTK_ACCESSIBLE (widget->label),
                                     GTK_ACCESSIBLE_STATE_CHECKED, selected,
                                     -1);
}

static GtkWidget *
input_widget_new (CcInputChooser *chooser,
		   const char *type,
		   const char *id,
                   gboolean    is_extra)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
	GtkWidget *label;
        InputWidget *widget = g_new0 (InputWidget, 1);
	gchar *text;

	if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
		const gchar *name;

		gnome_xkb_info_get_layout_info (priv->xkb_info, id, &name, NULL, NULL, NULL);
		widget->name = g_strdup (name);
	}
#ifdef HAVE_IBUS
        else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
                if (priv->ibus_engines)
                        widget->name = engine_get_display_name (g_hash_table_lookup (priv->ibus_engines, id));
                else
                        widget->name = g_strdup (id);
	}
#endif
	else {
		widget->name = g_strdup ("ERROR");
	}

        widget->id = g_strdup (id);
	widget->type = g_strdup (type);
	widget->is_extra = is_extra;

	widget->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_top (widget->box, 12);
        gtk_widget_set_margin_bottom (widget->box, 12);
        gtk_widget_set_margin_start (widget->box, 12);
        gtk_widget_set_margin_end (widget->box, 12);
        gtk_widget_set_halign (widget->box, GTK_ALIGN_FILL);

	widget->label = gtk_label_new (widget->name);
        gtk_label_set_ellipsize (GTK_LABEL (widget->label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars (GTK_LABEL (widget->label), 40);
        gtk_label_set_xalign (GTK_LABEL (widget->label), 0);
        // this allows to have the "checked" and "unchecked" states
        g_object_set (G_OBJECT (widget->label),
                      "accessible-role",
                      GTK_ACCESSIBLE_ROLE_CHECKBOX,
                      NULL);
	gtk_box_append (GTK_BOX (widget->box), widget->label);

        widget->checkmark = gtk_image_new_from_icon_name ("object-select-symbolic");
	gtk_box_append (GTK_BOX (widget->box), widget->checkmark);

	text = g_strdup_printf ("<a href='preview'>%s</a>", _("Preview"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), text);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_widget_set_hexpand (label, TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_END);
	g_free (text);
	g_signal_connect (label, "activate-link",
			  G_CALLBACK (preview_cb), chooser);
	gtk_box_append (GTK_BOX (widget->box), label);

	g_object_set_data_full (G_OBJECT (widget->box), "input-widget", widget,
				input_widget_free);

	return widget->box;
}

static void
sync_all_checkmarks (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv;
        gboolean invalidate = FALSE;
        gsize i;

        priv = cc_input_chooser_get_instance_private (chooser);

        for (i = 0; i < priv->input_widget_boxes->len; i++) {
                InputWidget *widget;
                GtkWidget *child;
                gboolean should_be_visible;

                child = g_ptr_array_index (priv->input_widget_boxes, i);
                widget = get_input_widget (child);
                g_assert (widget != NULL);
                g_assert (widget->box == child);

	        if (priv->id == NULL || priv->type == NULL)
		        should_be_visible = FALSE;
	        else
	                should_be_visible = g_strcmp0 (widget->id, priv->id) == 0 &&
                                            g_strcmp0 (widget->type, priv->type) == 0;
                input_widget_set_selected (widget, should_be_visible);

                if (widget->is_extra && should_be_visible) {
                        g_debug ("Marking selected layout %s (%s:%s) as non-extra",
                                 widget->name, widget->type, widget->id);
                        widget->is_extra = FALSE;
                        invalidate = TRUE;
                }
        }

        if (invalidate) {
                gtk_list_box_invalidate_sort (GTK_LIST_BOX (priv->input_list));
                gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->input_list));
        }
}

static GtkWidget *
more_widget_new (void)
{
        GtkWidget *widget;
        GtkWidget *arrow;

        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        g_object_set (G_OBJECT (widget),
                      "accessible-role",
                      GTK_ACCESSIBLE_ROLE_GROUP,
                      NULL);
        gtk_widget_set_tooltip_text (widget, _("More…"));

        arrow = gtk_image_new_from_icon_name ("view-more-symbolic");
        gtk_widget_add_css_class (arrow, "dim-label");
        gtk_widget_set_margin_top (widget, 12);
        gtk_widget_set_margin_bottom (widget, 12);
        gtk_widget_set_hexpand (arrow, TRUE);
        gtk_widget_set_halign (arrow, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (arrow, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (widget), arrow);

        return widget;
}

static GtkWidget *
no_results_widget_new (void)
{
        GtkWidget *widget;

        /* Translators: a search for input methods or keyboard layouts
         * did not yield any results
         */
        widget = padded_label_new (_("No inputs found"));
        gtk_widget_set_sensitive (widget, FALSE);
        return widget;
}

static void
choose_non_extras (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv;
        guint count = 0;
        gsize i;

        priv = cc_input_chooser_get_instance_private (chooser);

        for (i = 0; i < priv->input_widget_boxes->len; i++) {
                InputWidget *widget;
                GtkWidget *child;

                if (++count > MIN_ROWS)
                        break;

                child = g_ptr_array_index (priv->input_widget_boxes, i);
                widget = get_input_widget (child);
                g_assert (widget != NULL);
                g_assert (widget->box == child);

                g_debug ("Picking %s (%s:%s) as non-extra",
                         widget->name, widget->type, widget->id);
                widget->is_extra = FALSE;
        }

        /* Changing is_extra above affects the ordering and the visibility
         * of the newly non-extra rows.
         */
        gtk_list_box_invalidate_sort (GTK_LIST_BOX (priv->input_list));
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->input_list));
}

static void
add_rows_to_list (CcInputChooser  *chooser,
	          GList            *list,
	          const gchar      *type,
	          const gchar      *default_id)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
	const gchar *id;
	GtkWidget *widget;
	gchar *key;

	for (; list; list = list->next) {
		id = (const gchar *) list->data;

		if (g_strcmp0 (id, default_id) == 0)
			continue;

		key = g_strdup_printf ("%s::%s", type, id);
		if (g_hash_table_contains (priv->inputs, key)) {
			g_free (key);
			continue;
		}
		g_hash_table_add (priv->inputs, key);

		widget = input_widget_new (chooser, type, id, TRUE);
		g_ptr_array_add (priv->input_widget_boxes, g_object_ref_sink (widget));
		gtk_list_box_append (GTK_LIST_BOX (priv->input_list), widget);
	}
}

static void
add_row_to_list (CcInputChooser *chooser,
		 const gchar     *type,
		 const gchar     *id)
{
	GList tmp = { 0 };
	tmp.data = (gpointer)id;
	add_rows_to_list (chooser, &tmp, type, NULL);
}

static void
get_locale_infos (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
	const gchar *type = NULL;
	const gchar *id = NULL;
	g_autofree gchar *lang = NULL;
	g_autofree gchar *country = NULL;
	GList *list;

	if (gnome_get_input_source_from_locale (priv->locale, &type, &id)) {
                add_row_to_list (chooser, type, id);
		if (!priv->id) {
			priv->id = g_strdup (id);
			priv->type = g_strdup (type);
		}
	}

	if (!gnome_parse_locale (priv->locale, &lang, &country, NULL, NULL))
		return;

	list = gnome_xkb_info_get_layouts_for_language (priv->xkb_info, lang);
	add_rows_to_list (chooser, list, INPUT_SOURCE_TYPE_XKB, id);
	g_list_free (list);

	if (country != NULL) {
		list = gnome_xkb_info_get_layouts_for_country (priv->xkb_info, country);
		add_rows_to_list (chooser, list, INPUT_SOURCE_TYPE_XKB, id);
		g_list_free (list);
	}

        choose_non_extras (chooser);

	list = gnome_xkb_info_get_all_layouts (priv->xkb_info);
	add_rows_to_list (chooser, list, INPUT_SOURCE_TYPE_XKB, id);
	g_list_free (list);
}

static gboolean
input_visible (GtkListBoxRow *row,
                  gpointer       user_data)
{
        CcInputChooser *chooser = user_data;
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
        InputWidget *widget;
        gboolean visible;
        GtkWidget *child;
        const char *search_term;

        child = gtk_list_box_row_get_child (row);
        if (child == priv->more_item)
                return !priv->showing_extra && g_hash_table_size (priv->inputs) > MIN_ROWS;

        widget = get_input_widget (child);

        if (!priv->showing_extra && widget->is_extra)
                return FALSE;

        search_term = gtk_editable_get_text (GTK_EDITABLE (priv->filter_entry));
        if (!search_term || !*search_term)
                return TRUE;

        visible = g_str_match_string (search_term, widget->name, TRUE);
        return visible;
}

static gint
sort_inputs (GtkListBoxRow *a,
                GtkListBoxRow *b,
                gpointer       data)
{
        InputWidget *la, *lb;

        la = get_input_widget (gtk_list_box_row_get_child (a));
        lb = get_input_widget (gtk_list_box_row_get_child (b));

        if (la == NULL)
                return 1;

        if (lb == NULL)
                return -1;

        if (la->is_extra && !lb->is_extra)
                return 1;

        if (!la->is_extra && lb->is_extra)
                return -1;

        return strcmp (la->name, lb->name);
}

static void
filter_changed (GtkEntry        *entry,
                CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->input_list));
}

static void
show_more (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

	if (g_hash_table_size (priv->inputs) <= MIN_ROWS)
		return;

        gtk_widget_grab_focus (priv->filter_entry);

	gtk_widget_set_valign (GTK_WIDGET (chooser), GTK_ALIGN_FILL);

        priv->showing_extra = TRUE;
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->input_list));
        g_object_notify_by_pspec (G_OBJECT (chooser), obj_props[PROP_SHOWING_EXTRA]);
}

static void
set_input (CcInputChooser *chooser,
           const gchar    *id,
	   const gchar    *type)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

        if (g_strcmp0 (priv->id, id) == 0 &&
            g_strcmp0 (priv->type, type) == 0)
                return;

        g_free (priv->id);
	g_free (priv->type);
        priv->id = g_strdup (id);
	priv->type = g_strdup (type);

        sync_all_checkmarks (chooser);

	g_signal_emit (chooser, signals[CHANGED], 0);
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
               CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
        GtkWidget *child;
        InputWidget *widget;

        if (row == NULL)
                return;

        child = gtk_list_box_row_get_child (row);
        if (child == priv->more_item) {
                show_more (chooser);
        } else {
                widget = get_input_widget (child);
                if (widget == NULL)
                        return;
                input_widget_set_selected (widget, TRUE);
                if (g_strcmp0 (priv->id, widget->id) == 0 &&
                    g_strcmp0 (priv->type, widget->type) == 0)
                        confirm_choice (chooser);
                else
                        set_input (chooser, widget->id, widget->type);
        }
}

#ifdef HAVE_IBUS
static void
update_ibus_active_sources (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv;
        gboolean invalidate = FALSE;
        IBusEngineDesc *engine_desc;
        const gchar *type;
        const gchar *id;
        gchar *name;
        gsize i;

        priv = cc_input_chooser_get_instance_private (chooser);

        for (i = 0; i < priv->input_widget_boxes->len; i++) {
                GtkWidget *child;
                InputWidget *row;

                child = g_ptr_array_index (priv->input_widget_boxes, i);
                row = get_input_widget (child);
                g_assert (row != NULL);
                g_assert (row->box == child);

                type = row->type;
                id = row->id;
                if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) != 0)
                        continue;

                engine_desc = g_hash_table_lookup (priv->ibus_engines, id);
                if (engine_desc) {
                        name = engine_get_display_name (engine_desc);
                        gtk_label_set_text (GTK_LABEL (row->label), name);
                        g_clear_pointer (&row->name, g_free);
                        row->name = g_steal_pointer (&name);
                        invalidate = TRUE;
                }
        }

        if (invalidate) {
                gtk_list_box_invalidate_sort (GTK_LIST_BOX (priv->input_list));
                gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->input_list));
        }
}

static void
get_ibus_locale_infos (CcInputChooser *chooser)
{
	CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
	GHashTableIter iter;
	const gchar *engine_id;
	IBusEngineDesc *engine;

	if (!priv->ibus_engines)
		return;

	g_hash_table_iter_init (&iter, priv->ibus_engines);
	while (g_hash_table_iter_next (&iter, (gpointer *) &engine_id, (gpointer *) &engine))
                add_row_to_list (chooser, INPUT_SOURCE_TYPE_IBUS, engine_id);
}

static void
fetch_ibus_engines_result (GObject       *object,
                           GAsyncResult  *result,
                           CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv;
        GList *list, *l;
        GError *error;

        error = NULL;
        list = ibus_bus_list_engines_async_finish (IBUS_BUS (object), result, &error);
        if (!list && error) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't finish IBus request: %s", error->message);
                g_error_free (error);
                return;
        }

        priv = cc_input_chooser_get_instance_private (chooser);
        g_clear_object (&priv->ibus_cancellable);

        /* Maps engine ids to engine description objects */
        priv->ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

        for (l = list; l; l = l->next) {
                IBusEngineDesc *engine = l->data;
                const gchar *engine_id;

		engine_id = ibus_engine_desc_get_name (engine);
                if (g_str_has_prefix (engine_id, "xkb:"))
                        g_object_unref (engine);
                else
			g_hash_table_replace (priv->ibus_engines, (gpointer)engine_id, engine);
	}
	g_list_free (list);

	update_ibus_active_sources (chooser);
	get_ibus_locale_infos (chooser);

        sync_all_checkmarks (chooser);
}

static void
fetch_ibus_engines (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

        priv->ibus_cancellable = g_cancellable_new ();

        ibus_bus_list_engines_async (priv->ibus,
                                     -1,
                                     priv->ibus_cancellable,
                                     (GAsyncReadyCallback)fetch_ibus_engines_result,
                                     chooser);

	/* We've got everything we needed, don't want to be called again. */
	g_signal_handlers_disconnect_by_func (priv->ibus, fetch_ibus_engines, chooser);
}

static void
maybe_start_ibus (void)
{
        /* IBus doesn't export API in the session bus. The only thing
	 * we have there is a well known name which we can use as a
	 * sure-fire way to activate it.
	 */
        g_bus_unwatch_name (g_bus_watch_name (G_BUS_TYPE_SESSION,
                                              IBUS_SERVICE_IBUS,
                                              G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL));
}
#endif

static void
cc_input_chooser_constructed (GObject *object)
{
        CcInputChooser *chooser = CC_INPUT_CHOOSER (object);
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

        G_OBJECT_CLASS (cc_input_chooser_parent_class)->constructed (object);

	priv->xkb_info = gnome_xkb_info_new ();

#ifdef HAVE_IBUS
        ibus_init ();
        if (!priv->ibus) {
                priv->ibus = ibus_bus_new_async ();
                if (ibus_bus_is_connected (priv->ibus))
                        fetch_ibus_engines (chooser);
                else
                        g_signal_connect_swapped (priv->ibus, "connected",
                                                  G_CALLBACK (fetch_ibus_engines), chooser);
        }
        maybe_start_ibus ();
#endif

	priv->inputs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        priv->more_item = more_widget_new ();
        priv->no_results = no_results_widget_new ();

        gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->input_list),
                                    sort_inputs, chooser, NULL);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (priv->input_list),
                                      input_visible, chooser, NULL);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->input_list),
                                         GTK_SELECTION_NONE);

	if (priv->locale == NULL) {
		priv->locale = cc_common_language_get_current_language ();
	}

        get_locale_infos (chooser);
#ifdef HAVE_IBUS
	get_ibus_locale_infos (chooser);
#endif

        gtk_list_box_append (GTK_LIST_BOX (priv->input_list), priv->more_item);
        gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->input_list), priv->no_results);

        g_signal_connect (priv->filter_entry, "changed",
                          G_CALLBACK (filter_changed),
                          chooser);

        g_signal_connect (priv->input_list, "row-activated",
                          G_CALLBACK (row_activated), chooser);

        sync_all_checkmarks (chooser);
}

static void
cc_input_chooser_finalize (GObject *object)
{
	CcInputChooser *chooser = CC_INPUT_CHOOSER (object);
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

	g_clear_object (&priv->xkb_info);
	g_hash_table_unref (priv->inputs);
        g_clear_pointer (&priv->input_widget_boxes, g_ptr_array_unref);
#ifdef HAVE_IBUS
	g_signal_handlers_disconnect_by_func (priv->ibus, fetch_ibus_engines, chooser);
        g_clear_object (&priv->ibus);
        if (priv->ibus_cancellable)
                g_cancellable_cancel (priv->ibus_cancellable);
        g_clear_object (&priv->ibus_cancellable);
        g_clear_pointer (&priv->ibus_engines, g_hash_table_destroy);
#endif

	G_OBJECT_CLASS (cc_input_chooser_parent_class)->finalize (object);
}

static void
cc_input_chooser_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
        CcInputChooser *chooser = CC_INPUT_CHOOSER (object);
        switch (prop_id) {
        case PROP_SHOWING_EXTRA:
                g_value_set_boolean (value, cc_input_chooser_get_showing_extra (chooser));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static gboolean
cc_input_chooser_grab_focus (GtkWidget *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (CC_INPUT_CHOOSER (chooser));
        return gtk_widget_grab_focus (GTK_WIDGET (priv->filter_entry));
}

static void
cc_input_chooser_class_init (CcInputChooserClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/input-chooser.ui");

        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), CcInputChooser, filter_entry);
        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), CcInputChooser, input_list);

        widget_class->grab_focus = cc_input_chooser_grab_focus;
	object_class->finalize = cc_input_chooser_finalize;
        object_class->get_property = cc_input_chooser_get_property;
        object_class->constructed = cc_input_chooser_constructed;

        obj_props[PROP_SHOWING_EXTRA] =
                g_param_spec_string ("showing-extra", "", "", "",
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

        signals[CONFIRM] =
                g_signal_new ("confirm",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
cc_input_chooser_init (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

        gtk_widget_init_template (GTK_WIDGET (chooser));
        priv->input_widget_boxes = g_ptr_array_new_with_free_func (g_object_unref);
}

void
cc_input_chooser_clear_filter (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
        gtk_editable_set_text (GTK_EDITABLE (priv->filter_entry), "");
}

const gchar *
cc_input_chooser_get_input_id (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
        return priv->id;
}

const gchar *
cc_input_chooser_get_input_type (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
        return priv->type;
}

void
cc_input_chooser_get_layout (CcInputChooser *chooser,
			     const gchar    **layout,
			     const gchar    **variant)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);

        if (!get_layout (chooser, priv->type, priv->id, layout, variant)) {
                if (layout != NULL)
                        *layout = NULL;
                if (variant != NULL)
                        *variant = NULL;
        }
}

void
cc_input_chooser_set_input (CcInputChooser *chooser,
                            const gchar    *id,
			    const gchar    *type)
{
        set_input (chooser, id, type);
}

gboolean
cc_input_chooser_get_showing_extra (CcInputChooser *chooser)
{
        CcInputChooserPrivate *priv = cc_input_chooser_get_instance_private (chooser);
        return priv->showing_extra;
}
