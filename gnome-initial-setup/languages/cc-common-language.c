/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include "cc-common-language.h"

#include "gdm-languages.h"

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

        if (!gdm_parse_language_name (locale, &language_code, NULL, NULL, NULL))
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
        const gchar *locale;

        locale = (const gchar *) setlocale (LC_MESSAGES, NULL);
        if (locale)
                language = gdm_normalize_language_name (locale);
        else
                language = NULL;

        return language;
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
                GDBusProxy *user;
                GVariant *props;
                const char *lang;
                char *name;
                char *language;

                user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                      NULL,
                                                      "org.freedesktop.Accounts",
                                                      str,
                                                      "org.freedesktop.Accounts.User",
                                                      NULL,
                                                      &error);
                if (user == NULL) {
                        g_warning ("Failed to get proxy for user '%s': %s",
                                   str, error->message);
                        g_error_free (error);
                        error = NULL;
                        continue;
                }
                props = g_dbus_proxy_get_cached_property (user, "Language");
                lang = g_variant_get_string (props, NULL);
                if (lang != NULL && *lang != '\0' &&
                    cc_common_language_has_font (lang) &&
                    gdm_language_has_translations (lang)) {
                        name = gdm_normalize_language_name (lang);
                        if (!g_hash_table_lookup (ht, name)) {
                                language = gdm_get_language_from_name (name, NULL);
                                g_hash_table_insert (ht, name, language);
                        }
                        else {
                                g_free (name);
                        }
                }
                g_variant_unref (props);
                g_object_unref (user);
        }
        g_variant_iter_free (vi);
        g_variant_unref (variant);

        g_object_unref (proxy);
}

GHashTable *
cc_common_language_get_initial_languages (void)
{
        GHashTable *ht;
        char *name;
        char *language;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        /* Add some common languages first */
        g_hash_table_insert (ht, g_strdup ("en_US.utf8"), g_strdup (_("English")));
        if (gdm_language_has_translations ("en_GB"))
                g_hash_table_insert (ht, g_strdup ("en_GB.utf8"), g_strdup (_("British English")));
        if (gdm_language_has_translations ("de") ||
            gdm_language_has_translations ("de_DE"))
                g_hash_table_insert (ht, g_strdup ("de_DE.utf8"), g_strdup (_("German")));
        if (gdm_language_has_translations ("fr") ||
            gdm_language_has_translations ("fr_FR"))
                g_hash_table_insert (ht, g_strdup ("fr_FR.utf8"), g_strdup (_("French")));
        if (gdm_language_has_translations ("es") ||
            gdm_language_has_translations ("es_ES"))
                g_hash_table_insert (ht, g_strdup ("es_ES.utf8"), g_strdup (_("Spanish")));
        if (gdm_language_has_translations ("zh_CN"))
                g_hash_table_insert (ht, g_strdup ("zh_CN.utf8"), g_strdup (_("Chinese (simplified)")));

        /* Add the languages used by other users on the system */
        add_other_users_language (ht);

        /* Add current locale */
        name = cc_common_language_get_current_language ();
        if (g_hash_table_lookup (ht, name) == NULL) {
                language = gdm_get_language_from_name (name, NULL);
                g_hash_table_insert (ht, name, language);
        } else {
                g_free (name);
        }

        return ht;
}
