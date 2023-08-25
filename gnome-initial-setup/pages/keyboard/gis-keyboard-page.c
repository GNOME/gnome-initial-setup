/*
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Sergey Udaltsov <svu@gnome.org>
 *         Michael Wood <michael.g.wood@intel.com>
 *
 * Based on gnome-control-center cc-region-panel.c
 */

#define PAGE_ID "keyboard"

#include "config.h"

#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "gis-keyboard-page.h"
#include "keyboard-resources.h"
#include "cc-input-chooser.h"

#include "cc-common-language.h"

#include "gis-page-header.h"

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_CURRENT_INPUT_SOURCE "current"
#define KEY_INPUT_SOURCES        "sources"
#define KEY_MRU_SOURCES          "mru-sources"
#define KEY_INPUT_OPTIONS        "xkb-options"

struct _GisKeyboardPagePrivate {
        GtkWidget *input_chooser;

	GDBusProxy *localed;
	GCancellable *cancellable;
	GPermission *permission;
        GSettings *input_settings;
        char **default_input_source_ids;
        char **default_input_source_types;
        char **default_options;
        char **system_layouts;
        char **system_variants;
        char **system_options;

        gboolean should_skip;
};
typedef struct _GisKeyboardPagePrivate GisKeyboardPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisKeyboardPage, gis_keyboard_page, GIS_TYPE_PAGE);

static void
gis_keyboard_page_finalize (GObject *object)
{
	GisKeyboardPage *self = GIS_KEYBOARD_PAGE (object);
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);

	if (priv->cancellable)
		g_cancellable_cancel (priv->cancellable);
	g_clear_object (&priv->cancellable);

	g_clear_object (&priv->permission);
	g_clear_object (&priv->localed);
	g_clear_object (&priv->input_settings);
        g_clear_pointer (&priv->default_input_source_ids, g_strfreev);
        g_clear_pointer (&priv->default_input_source_types, g_strfreev);
        g_clear_pointer (&priv->default_options, g_strfreev);
        g_clear_pointer (&priv->system_layouts, g_strfreev);
        g_clear_pointer (&priv->system_variants, g_strfreev);
        g_clear_pointer (&priv->system_options, g_strfreev);

	G_OBJECT_CLASS (gis_keyboard_page_parent_class)->finalize (object);
}

static void
add_defaults_to_variant_builder (GisKeyboardPage  *self,
                                  const char       *already_added_type,
                                  const char       *already_added_id,
                                  GVariantBuilder  *input_source_builder,
                                  char            **default_layout)

{
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        size_t i;

        for (i = 0; priv->default_input_source_ids && priv->default_input_source_ids[i] != NULL; i++) {
                if (g_strcmp0 (already_added_id, priv->default_input_source_ids[i]) == 0 && g_strcmp0 (already_added_type, priv->default_input_source_types[i]) == 0) {
                        continue;
                }

                g_variant_builder_add (input_source_builder, "(ss)", priv->default_input_source_types[i], priv->default_input_source_ids[i]);

                if (*default_layout != NULL) {
                        if (!gnome_input_source_is_non_latin (priv->default_input_source_types[i], priv->default_input_source_ids[i])) {
                                *default_layout = g_strdup (priv->default_input_source_ids[i]);
                        }
                }
        }
}


static void
add_input_source_to_arrays (GisKeyboardPage *self,
                            const char      *type,
                            const char      *id,
                            GPtrArray       *layouts_array,
                            GPtrArray       *variants_array)
{
        g_auto(GStrv) layout_and_variant = NULL;
        const char *layout, *variant;

        if (!g_str_equal (type, "xkb")) {
                return;
        }

        layout_and_variant = g_strsplit (id, "+", -1);

        layout = layout_and_variant[0];
        variant = layout_and_variant[1]?: "";

        if (g_ptr_array_find_with_equal_func (layouts_array, layout, g_str_equal, NULL) &&
            g_ptr_array_find_with_equal_func (variants_array, variant, g_str_equal, NULL)) {
                return;
        }

        g_ptr_array_add (layouts_array, g_strdup (layout));
        g_ptr_array_add (variants_array, g_strdup (variant));
}

static void
add_defaults_to_arrays (GisKeyboardPage   *self,
                        GPtrArray         *layouts_array,
                        GPtrArray         *variants_array)
{
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        size_t i;

        for (i = 0; priv->default_input_source_ids && priv->default_input_source_ids[i] != NULL; i++) {
                add_input_source_to_arrays (self, priv->default_input_source_types[i], priv->default_input_source_ids[i], layouts_array, variants_array);
        }
}

static void
set_input_settings (GisKeyboardPage *self,
                    const char *type,
                    const char *id)
{
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        g_autofree char *layout = NULL;
        g_autofree char *variant = NULL;
        g_autoptr(GVariant) default_input_sources = NULL;
        g_autoptr(GVariant) input_sources = NULL;
        g_autoptr(GPtrArray) layouts_array = NULL;
        g_autoptr(GPtrArray) variants_array = NULL;
        GVariantBuilder input_source_builder;
        GVariantBuilder input_options_builder;
        g_autoptr(GVariant) input_options = NULL;
        gboolean is_system_mode;
        size_t i;
        g_autofree char *default_input_source_id = NULL;

        default_input_sources = g_settings_get_default_value (priv->input_settings, KEY_INPUT_SOURCES);
        input_sources = g_settings_get_value (priv->input_settings, KEY_INPUT_SOURCES);

        if (!g_variant_equal (default_input_sources, input_sources))
                return;

        g_clear_pointer (&input_sources, g_variant_unref);

        g_variant_builder_init (&input_options_builder, G_VARIANT_TYPE ("as"));

        is_system_mode = gis_driver_get_mode (GIS_PAGE (self)->driver) == GIS_DRIVER_MODE_NEW_USER;

        layouts_array = g_ptr_array_new ();
        variants_array = g_ptr_array_new ();

        /* Notice the added latin layout (if relevant) gets put first for gsettings
         * (input_source_builder and last for localed (layouts_array/variants_array)
         * This ensures we get a cyrillic layout on ttys, but a latin layout by default
         * in the UI.
         */
        g_variant_builder_init (&input_source_builder, G_VARIANT_TYPE ("a(ss)"));
        if (type != NULL && id != NULL) {
                add_input_source_to_arrays (self, type, id, layouts_array, variants_array);

                if (gnome_input_source_is_non_latin (type, id)) {
                        default_input_source_id = g_strdup ("us");
                        add_input_source_to_arrays (self, "xkb", default_input_source_id, layouts_array, variants_array);
                        g_variant_builder_add (&input_source_builder, "(ss)", "xkb", default_input_source_id);
                } else {
                        default_input_source_id = g_strdup (id);
                }

                g_variant_builder_add (&input_source_builder, "(ss)", type, id);
        }

        if (default_input_source_id == NULL || !is_system_mode) {
                add_defaults_to_variant_builder (self, type, id, &input_source_builder, &default_input_source_id);
        }
        input_sources = g_variant_builder_end (&input_source_builder);

        for (i = 0; priv->default_options[i] != NULL; i++) {
                g_variant_builder_add (&input_options_builder, "s", priv->default_options[i]);
        }
        input_options = g_variant_builder_end (&input_options_builder);

        add_defaults_to_arrays (self, layouts_array, variants_array);
        g_ptr_array_add (layouts_array, NULL);
        g_ptr_array_add (variants_array, NULL);

        priv->system_layouts = (char **) g_ptr_array_steal (layouts_array, NULL);
        priv->system_variants =  (char **) g_ptr_array_steal (variants_array, NULL);

        g_variant_get (input_options, "^as", &priv->system_options);

        g_settings_set_value (priv->input_settings, KEY_INPUT_SOURCES, g_steal_pointer (&input_sources));
        g_settings_set_value (priv->input_settings, KEY_INPUT_OPTIONS, g_steal_pointer (&input_options));

        if (default_input_source_id != NULL) {
                GVariantBuilder mru_input_source_builder;

                g_variant_builder_init (&mru_input_source_builder, G_VARIANT_TYPE ("a(ss)"));
                g_variant_builder_add (&mru_input_source_builder, "(ss)", type, default_input_source_id);
                g_settings_set_value (priv->input_settings, KEY_MRU_SOURCES, g_variant_builder_end (&mru_input_source_builder));
        }

        g_settings_apply (priv->input_settings);
}

static void
set_localed_input (GisKeyboardPage *self)
{
	GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        g_autofree char *layouts = NULL;
        g_autofree char *variants = NULL;
        g_autofree char *options = NULL;

        if (!priv->localed)
                return;

        layouts = g_strjoinv (",", priv->system_layouts);
        variants = g_strjoinv (",", priv->system_variants);
        options = g_strjoinv (",", priv->system_options);

        g_dbus_proxy_call (priv->localed,
                           "SetX11Keyboard",
                           g_variant_new ("(ssssbb)", layouts, "", variants, options, TRUE, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
change_locale_permission_acquired (GObject      *source,
				   GAsyncResult *res,
				   gpointer      data)
{
	GisKeyboardPage *page = GIS_KEYBOARD_PAGE (data);
	GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (page);
	GError *error = NULL;
	gboolean allowed;

	allowed = g_permission_acquire_finish (priv->permission, res, &error);
	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to acquire permission: %s", error->message);
		g_error_free (error);
		return;
	}

	if (allowed)
		set_localed_input (GIS_KEYBOARD_PAGE (data));
}

static void
update_input (GisKeyboardPage *self)
{
	GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        const gchar *type;
        const gchar *id;

        type = cc_input_chooser_get_input_type (CC_INPUT_CHOOSER (priv->input_chooser));
        id = cc_input_chooser_get_input_id (CC_INPUT_CHOOSER (priv->input_chooser));

	set_input_settings (self, type, id);

	if (gis_driver_get_mode (GIS_PAGE (self)->driver) == GIS_DRIVER_MODE_NEW_USER) {
		if (g_permission_get_allowed (priv->permission)) {
			set_localed_input (self);
		} else if (g_permission_get_can_acquire (priv->permission)) {
			g_permission_acquire_async (priv->permission,
						    NULL,
						    change_locale_permission_acquired,
						    self);
		}
	}
}

static gboolean
gis_keyboard_page_apply (GisPage      *page,
                         GCancellable *cancellable)
{
	update_input (GIS_KEYBOARD_PAGE (page));
        return FALSE;
}

static void
add_default_input_sources (GisKeyboardPage *self)
{
        set_input_settings (self, NULL, NULL);
}

static void
gis_keyboard_page_skip (GisPage *page)
{
	GisKeyboardPage *self = GIS_KEYBOARD_PAGE (page);
	GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);

	priv->should_skip = TRUE;

	if (priv->default_input_source_ids != NULL)
		add_default_input_sources (self);
}

static void
preselect_input_source (GisKeyboardPage *self)
{
        const char *language = NULL;

        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);

        language = cc_common_language_get_current_language ();

        /* We deduce the initial input source from language if we're in a system mode
         * (where preexisting system configuration may be stale) or if the language
         * requires an input method (because there is no way for system configuration
         * to denote the need for an input method)
         *
         * If it's a non-system mode we can trust the system configuration is probably
         * a better bet than a heuristic based on locale.
         */
        if (language != NULL) {
                gboolean got_input_source;
                const char *id, *type;

                got_input_source = gnome_get_input_source_from_locale (language, &type, &id);

                if (got_input_source) {
                        gboolean is_system_mode = gis_driver_get_mode (GIS_PAGE (self)->driver) == GIS_DRIVER_MODE_NEW_USER;
                        if (is_system_mode || g_str_equal (type, "ibus")) {
                                cc_input_chooser_set_input (CC_INPUT_CHOOSER (priv->input_chooser),
                                                            id,
                                                            type);
                                return;
                        }
                }
        }

        cc_input_chooser_set_input (CC_INPUT_CHOOSER (priv->input_chooser),
                                    priv->default_input_source_ids[0],
                                    priv->default_input_source_types[0]);
}

static void
update_page_complete (GisKeyboardPage *self)
{
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        gboolean complete;

        if (gis_driver_get_mode (GIS_PAGE (self)->driver) == GIS_DRIVER_MODE_NEW_USER) {
                complete = (priv->localed != NULL &&
                            cc_input_chooser_get_input_id (CC_INPUT_CHOOSER (priv->input_chooser)) != NULL);
        }  else {
                complete = cc_input_chooser_get_input_id (CC_INPUT_CHOOSER (priv->input_chooser)) != NULL;
        }

        gis_page_set_complete (GIS_PAGE (self), complete);
}

static void
localed_proxy_ready (GObject      *source,
		     GAsyncResult *res,
		     gpointer      data)
{
	GisKeyboardPage *self = data;
	GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
	GDBusProxy *proxy;
	GError *error = NULL;

	proxy = g_dbus_proxy_new_finish (res, &error);

	if (!proxy) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to contact localed: %s", error->message);
		g_error_free (error);
		return;
	}

	priv->localed = proxy;
	update_page_complete (self);
}

static void
input_confirmed (CcInputChooser  *chooser,
                 GisKeyboardPage *self)
{
        gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (self)->driver));
}

static void
input_changed (CcInputChooser  *chooser,
               GisKeyboardPage *self)
{
        update_page_complete (self);
}

static void
on_got_default_sources (GObject      *source,
                        GAsyncResult *res,
                        gpointer      data)
{
        GisKeyboardPage *self = data;
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        g_autoptr (GError) error = NULL;
        gboolean success = FALSE;
        g_auto (GStrv) ids = NULL;
        g_auto (GStrv) types = NULL;
        g_auto (GStrv) options = NULL;

        success = gnome_get_default_input_sources_finish (res, &ids, &types, &options, NULL, &error);

        if (!success) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    g_warning ("Failed to fetch default input sources: %s", error->message);
                return;
        }

        priv->default_input_source_ids = g_steal_pointer (&ids);
        priv->default_input_source_types = g_steal_pointer (&types);
        priv->default_options = g_steal_pointer (&options);

        if (priv->should_skip) {
                add_default_input_sources (self);
                return;
        }

        preselect_input_source (self);
        update_page_complete (self);
}

static void
gis_keyboard_page_constructed (GObject *object)
{
        GisKeyboardPage *self = GIS_KEYBOARD_PAGE (object);
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);

        G_OBJECT_CLASS (gis_keyboard_page_parent_class)->constructed (object);

        g_signal_connect (priv->input_chooser, "confirm",
                          G_CALLBACK (input_confirmed), self);
        g_signal_connect (priv->input_chooser, "changed",
                          G_CALLBACK (input_changed), self);

	priv->input_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
	g_settings_delay (priv->input_settings);

	priv->cancellable = g_cancellable_new ();

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
				  NULL,
				  "org.freedesktop.locale1",
				  "/org/freedesktop/locale1",
				  "org.freedesktop.locale1",
				  priv->cancellable,
				  (GAsyncReadyCallback) localed_proxy_ready,
				  self);

	gnome_get_default_input_sources (priv->cancellable, on_got_default_sources, self);

	/* If we're in new user mode then we're manipulating system settings */
	if (gis_driver_get_mode (GIS_PAGE (self)->driver) == GIS_DRIVER_MODE_NEW_USER)
		priv->permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-keyboard", NULL, NULL, NULL);

        update_page_complete (self);

        gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
}

static void
gis_keyboard_page_locale_changed (GisPage *page)
{
        gis_page_set_title (GIS_PAGE (page), _("Typing"));
}

static void
gis_keyboard_page_class_init (GisKeyboardPageClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GisPageClass * page_class = GIS_PAGE_CLASS (klass);

        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-keyboard-page.ui");

        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisKeyboardPage, input_chooser);

        page_class->page_id = PAGE_ID;
        page_class->apply = gis_keyboard_page_apply;
        page_class->skip = gis_keyboard_page_skip;
        page_class->locale_changed = gis_keyboard_page_locale_changed;
        object_class->constructed = gis_keyboard_page_constructed;
	object_class->finalize = gis_keyboard_page_finalize;
}

static void
gis_keyboard_page_init (GisKeyboardPage *self)
{
        g_type_ensure (GIS_TYPE_PAGE_HEADER);
	g_type_ensure (CC_TYPE_INPUT_CHOOSER);

        gtk_widget_init_template (GTK_WIDGET (self));
}

GisPage *
gis_prepare_keyboard_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_KEYBOARD_PAGE,
                       "driver", driver,
                       NULL);
}
