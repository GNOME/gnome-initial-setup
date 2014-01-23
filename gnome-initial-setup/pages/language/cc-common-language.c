/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <fontconfig/fontconfig.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "cc-common-language.h"

static char *get_lang_for_user_object_path (const char *path);

gboolean
cc_common_language_has_font (const gchar *locale)
{
        const FcCharSet *charset;
        FcPattern       *pattern;
        FcObjectSet     *object_set;
        FcFontSet       *font_set;
        gchar           *language_code;
        gboolean         is_displayable;

        is_displayable = FALSE;
        pattern = NULL;
        object_set = NULL;
        font_set = NULL;

        if (!gnome_parse_locale (locale, &language_code, NULL, NULL, NULL))
                return FALSE;

        charset = FcLangGetCharSet ((FcChar8 *) language_code);
        if (!charset) {
                /* fontconfig does not know about this language */
                is_displayable = TRUE;
        }
        else {
                /* see if any fonts support rendering it */
                pattern = FcPatternBuild (NULL, FC_LANG, FcTypeString, language_code, NULL);

                if (pattern == NULL)
                        goto done;

                object_set = FcObjectSetCreate ();

                if (object_set == NULL)
                        goto done;

                font_set = FcFontList (NULL, pattern, object_set);

                if (font_set == NULL)
                        goto done;

                is_displayable = (font_set->nfont > 0);
        }

 done:
        if (font_set != NULL)
                FcFontSetDestroy (font_set);

        if (object_set != NULL)
                FcObjectSetDestroy (object_set);

        if (pattern != NULL)
                FcPatternDestroy (pattern);

        g_free (language_code);

        return is_displayable;
}

gchar *
cc_common_language_get_current_language (void)
{
        gchar *language;
        char *path;
        const gchar *locale;

	path = g_strdup_printf ("/org/freedesktop/Accounts/User%d", getuid ());
        language = get_lang_for_user_object_path (path);
        g_free (path);
        if (language != NULL && *language != '\0')
                return language;

        locale = (const gchar *) setlocale (LC_MESSAGES, NULL);
        if (locale)
                language = gnome_normalize_locale (locale);
        else
                language = NULL;

        return language;
}

static gboolean
user_language_has_translations (const char *locale)
{
        char *name, *language_code, *territory_code;
        gboolean ret;

        gnome_parse_locale (locale,
                            &language_code,
                            &territory_code,
                            NULL, NULL);
        name = g_strdup_printf ("%s%s%s",
                                language_code,
                                territory_code != NULL? "_" : "",
                                territory_code != NULL? territory_code : "");
        g_free (language_code);
        g_free (territory_code);
        ret = gnome_language_has_translations (name);
        g_free (name);

        return ret;
}

static char *
get_lang_for_user_object_path (const char *path)
{
	GError *error = NULL;
	GDBusProxy *user;
	GVariant *props;
	char *lang;

	user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					      G_DBUS_PROXY_FLAGS_NONE,
					      NULL,
					      "org.freedesktop.Accounts",
					      path,
					      "org.freedesktop.Accounts.User",
					      NULL,
					      &error);
	if (user == NULL) {
		g_warning ("Failed to get proxy for user '%s': %s",
			   path, error->message);
		g_error_free (error);
		return NULL;
	}
	props = g_dbus_proxy_get_cached_property (user, "Language");
	lang = g_variant_dup_string (props, NULL);

	g_variant_unref (props);
	g_object_unref (user);
	return lang;
}

static void
add_other_users_language (GHashTable *ht)
{
        GVariant *variant;
        GVariantIter *vi;
        GError *error = NULL;
        const char *str;
        GDBusProxy *proxy;

        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL,
                                               "org.freedesktop.Accounts",
                                               "/org/freedesktop/Accounts",
                                               "org.freedesktop.Accounts",
                                               NULL,
                                               NULL);

        if (proxy == NULL)
                return;

        variant = g_dbus_proxy_call_sync (proxy,
                                          "ListCachedUsers",
                                          NULL,
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
        if (variant == NULL) {
                g_warning ("Failed to list existing users: %s", error->message);
                g_error_free (error);
                g_object_unref (proxy);
                return;
        }
        g_variant_get (variant, "(ao)", &vi);
        while (g_variant_iter_loop (vi, "o", &str)) {
                char *lang;
                char *name;
                char *language;

                lang = get_lang_for_user_object_path (str);
                if (lang != NULL && *lang != '\0' &&
                    cc_common_language_has_font (lang) &&
                    user_language_has_translations (lang)) {
                        name = gnome_normalize_locale (lang);
                        if (!g_hash_table_lookup (ht, name)) {
                                language = gnome_get_language_from_locale (name, NULL);
                                g_hash_table_insert (ht, name, language);
                        }
                        else {
                                g_free (name);
                        }
                }
                g_free (lang);
        }
        g_variant_iter_free (vi);
        g_variant_unref (variant);

        g_object_unref (proxy);
}

static void
insert_language_internal (GHashTable *ht,
                          const char *lang)
{
        gboolean has_translations;
        char *label_own_lang;
        char *label_current_lang;
        char *label_untranslated;
        char *key;

        has_translations = gnome_language_has_translations (lang);
        if (!has_translations) {
                char *lang_code = g_strndup (lang, 2);
                has_translations = gnome_language_has_translations (lang_code);
                g_free (lang_code);

                if (!has_translations)
                        return;
        }

        g_debug ("We have translations for %s", lang);

        key = g_strdup (lang);

        label_own_lang = gnome_get_language_from_locale (key, key);
        label_current_lang = gnome_get_language_from_locale (key, NULL);
        label_untranslated = gnome_get_language_from_locale (key, "C");

        /* We don't have a translation for the label in
         * its own language? */
        if (g_strcmp0 (label_own_lang, label_untranslated) == 0) {
                if (g_strcmp0 (label_current_lang, label_untranslated) == 0)
                        g_hash_table_insert (ht, key, g_strdup (label_untranslated));
                else
                        g_hash_table_insert (ht, key, g_strdup (label_current_lang));
        } else {
                g_hash_table_insert (ht, key, g_strdup (label_own_lang));
        }

        g_free (label_own_lang);
        g_free (label_current_lang);
        g_free (label_untranslated);
}

static void
insert_language (GHashTable *ht,
                 const char *lang)
{
        char *utf8_variant = g_strconcat (lang, ".utf8", NULL);
        insert_language_internal (ht, lang);
        insert_language_internal (ht, utf8_variant);
        g_free (utf8_variant);
}

static void
insert_user_languages (GHashTable *ht)
{
        char *name;
        char *language;

        /* Add the languages used by other users on the system */
        add_other_users_language (ht);

        /* Add current locale */
        name = cc_common_language_get_current_language ();
        if (g_hash_table_lookup (ht, name) == NULL) {
                language = gnome_get_language_from_locale (name, NULL);
                g_hash_table_insert (ht, name, language);
        } else {
                g_free (name);
        }
}

GHashTable *
cc_common_language_get_initial_languages (void)
{
        GHashTable *ht;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        insert_language (ht, "en_US");
        insert_language (ht, "en_GB");
        insert_language (ht, "de_DE");
        insert_language (ht, "fr_FR");
        insert_language (ht, "es_ES");
        insert_language (ht, "zh_CN");
        insert_language (ht, "ja_JP");
        insert_language (ht, "ru_RU");
        insert_language (ht, "ar_EG");

        insert_user_languages (ht);

        return ht;
}
