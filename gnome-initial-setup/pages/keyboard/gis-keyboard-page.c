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

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_CURRENT_INPUT_SOURCE "current"
#define KEY_INPUT_SOURCES        "sources"

struct _GisKeyboardPagePrivate {
        GtkWidget *input_chooser;

	GDBusProxy *localed;
	GCancellable *cancellable;
	GPermission *permission;
        GSettings *input_settings;

        GSList *system_sources;
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

        g_slist_free_full (priv->system_sources, g_free);

	G_OBJECT_CLASS (gis_keyboard_page_parent_class)->finalize (object);
}

static void
set_input_settings (GisKeyboardPage *self)
{
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        const gchar *type;
        const gchar *id;
        GVariantBuilder builder;
        GSList *l;
        gboolean is_xkb_source = FALSE;

        type = cc_input_chooser_get_input_type (CC_INPUT_CHOOSER (priv->input_chooser));
        id = cc_input_chooser_get_input_id (CC_INPUT_CHOOSER (priv->input_chooser));

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));

        if (g_str_equal (type, "xkb")) {
                g_variant_builder_add (&builder, "(ss)", type, id);
                is_xkb_source = TRUE;
        }

        for (l = priv->system_sources; l; l = l->next) {
                const gchar *sid = l->data;

                if (g_str_equal (id, sid) && g_str_equal (type, "xkb"))
                        continue;

                g_variant_builder_add (&builder, "(ss)", "xkb", sid);
        }

        if (!is_xkb_source)
                g_variant_builder_add (&builder, "(ss)", type, id);

	g_settings_set_value (priv->input_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
	g_settings_set_uint (priv->input_settings, KEY_CURRENT_INPUT_SOURCE, 0);

	g_settings_apply (priv->input_settings);
}

static void
set_localed_input (GisKeyboardPage *self)
{
	GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
	const gchar *layout, *variant;
        GString *layouts;
        GString *variants;
        GSList *l;

        if (!priv->localed)
                return;

	cc_input_chooser_get_layout (CC_INPUT_CHOOSER (priv->input_chooser), &layout, &variant);
        if (layout == NULL)
                layout = "";
        if (variant == NULL)
                variant = "";

        layouts = g_string_new (layout);
        variants = g_string_new (variant);

#define LAYOUT(a) (a[0])
#define VARIANT(a) (a[1] ? a[1] : "")
        for (l = priv->system_sources; l; l = l->next) {
                const gchar *sid = l->data;
                gchar **lv = g_strsplit (sid, "+", -1);

                if (!g_str_equal (LAYOUT (lv), layout) ||
                    !g_str_equal (VARIANT (lv), variant)) {
                        if (layouts->str[0]) {
                                g_string_append_c (layouts, ',');
                                g_string_append_c (variants, ',');
                        }
                        g_string_append (layouts, LAYOUT (lv));
                        g_string_append (variants, VARIANT (lv));
                }
                g_strfreev (lv);
        }
#undef LAYOUT
#undef VARIANT

        g_dbus_proxy_call (priv->localed,
                           "SetX11Keyboard",
                           g_variant_new ("(ssssbb)", layouts->str, "", variants->str, "", TRUE, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
        g_string_free (layouts, TRUE);
        g_string_free (variants, TRUE);
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

	set_input_settings (self);

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

static GSList *
get_localed_input (GDBusProxy *proxy)
{
        GVariant *v;
        const gchar *s;
        gchar *id;
        guint i, n;
        gchar **layouts = NULL;
        gchar **variants = NULL;
        GSList *sources = NULL;

        v = g_dbus_proxy_get_cached_property (proxy, "X11Layout");
        if (v) {
                s = g_variant_get_string (v, NULL);
                layouts = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        v = g_dbus_proxy_get_cached_property (proxy, "X11Variant");
        if (v) {
                s = g_variant_get_string (v, NULL);
                if (s && *s)
                        variants = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        if (variants && variants[0])
                n = MIN (g_strv_length (layouts), g_strv_length (variants));
        else if (layouts && layouts[0])
                n = g_strv_length (layouts);
        else
                n = 0;

        for (i = 0; i < n && layouts[i][0]; i++) {
                if (variants && variants[i] && variants[i][0])
                        id = g_strdup_printf ("%s+%s", layouts[i], variants[i]);
                else
                        id = g_strdup (layouts[i]);
                sources = g_slist_prepend (sources, id);
        }

        g_strfreev (variants);
        g_strfreev (layouts);

	return sources;
}

static void
add_default_keyboard_layout (GDBusProxy      *proxy,
                             GVariantBuilder *builder)
{
	GSList *sources = get_localed_input (proxy);
	sources = g_slist_reverse (sources);

	for (; sources; sources = sources->next)
		g_variant_builder_add (builder, "(ss)", "xkb",
				       (const gchar *) sources->data);

	g_slist_free_full (sources, g_free);
}

static void
add_default_input_sources (GisKeyboardPage *self,
                           GDBusProxy      *proxy)
{
	const gchar *type;
	const gchar *id;
	const gchar * const *locales;
	const gchar *language;
	GVariantBuilder builder;
	GSettings *input_settings;

	input_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));

	add_default_keyboard_layout (proxy, &builder);

	/* add other input sources */
	locales = g_get_language_names ();
	language = locales[0];
	if (gnome_get_input_source_from_locale (language, &type, &id)) {
		if (!g_str_equal (type, "xkb"))
			g_variant_builder_add (&builder, "(ss)", type, id);
	}

	g_settings_delay (input_settings);
	g_settings_set_value (input_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
	g_settings_set_uint (input_settings, KEY_CURRENT_INPUT_SOURCE, 0);
	g_settings_apply (input_settings);

	g_object_unref (input_settings);
}

static void
skip_proxy_ready (GObject      *source,
                  GAsyncResult *res,
                  gpointer      data)
{
	GisKeyboardPage *self = data;
	GDBusProxy *proxy;
	GError *error = NULL;

	proxy = g_dbus_proxy_new_finish (res, &error);

	if (!proxy) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to contact localed: %s", error->message);
		g_error_free (error);
		return;
	}

	add_default_input_sources (self, proxy);

	g_object_unref (proxy);
}

static gboolean
gis_keyboard_page_skip (GisPage *page)
{
	GisKeyboardPage *self = GIS_KEYBOARD_PAGE (page);
	GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
				  NULL,
				  "org.freedesktop.locale1",
				  "/org/freedesktop/locale1",
				  "org.freedesktop.locale1",
				  priv->cancellable,
				  (GAsyncReadyCallback) skip_proxy_ready,
				  self);
	return TRUE;
}

static void
load_localed_input (GisKeyboardPage *self)
{
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        GSList *sources = get_localed_input (priv->localed);

        /* These will be added silently after the user selection when
         * writing out the settings. */
        g_slist_free_full (priv->system_sources, g_free);
        priv->system_sources = g_slist_reverse (sources);

        /* We only pre-select the first system layout. */
        if (priv->system_sources)
                cc_input_chooser_set_input (CC_INPUT_CHOOSER (priv->input_chooser),
                                            (const gchar *) priv->system_sources->data,
                                            "xkb");
}

static void
update_page_complete (GisKeyboardPage *self)
{
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);
        gboolean complete;

        complete = (priv->localed != NULL &&
                    cc_input_chooser_get_input_id (CC_INPUT_CHOOSER (priv->input_chooser)) != NULL);
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

        load_localed_input (self);
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
gis_keyboard_page_constructed (GObject *object)
{
        GisKeyboardPage *self = GIS_KEYBOARD_PAGE (object);
        GisKeyboardPagePrivate *priv = gis_keyboard_page_get_instance_private (self);

	g_type_ensure (CC_TYPE_INPUT_CHOOSER);

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

	/* If we're in new user mode then we're manipulating system settings */
	if (gis_driver_get_mode (GIS_PAGE (self)->driver) == GIS_DRIVER_MODE_NEW_USER)
		priv->permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-keyboard", NULL, NULL, NULL);

        update_page_complete (self);

        gtk_widget_show (GTK_WIDGET (self));
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
        g_resources_register (keyboard_get_resource ());
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
