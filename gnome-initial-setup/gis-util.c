/*
 * Copyright 2023 Endless OS Foundation LLC
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
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gis-util.h"

void
gis_add_style_from_resource (const char *resource_path)
{
  g_autoptr(GtkCssProvider) provider = gtk_css_provider_new ();

  gtk_css_provider_load_from_resource (provider, resource_path);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

gboolean
gis_kernel_command_line_has_argument (const char *arguments[])
{
  GError *error = NULL;
  g_autofree char *contents = NULL;
  g_autoptr (GString) pattern = NULL;
  gboolean has_argument = FALSE;
  size_t i;

  if (!g_file_get_contents ("/proc/cmdline", &contents, NULL, &error)) {
    g_error_free (error);
    return FALSE;
  }

  /* Build up the pattern by iterating through the alternatives,
   * escaping all dots so they don't match any character but period,
   * and adding word boundary specifiers around the arguments so
   * substrings don't get matched.
   *
   * Also, add a | between each alternative.
   */
  pattern = g_string_new (NULL);
  for (i = 0; arguments[i] != NULL; i++) {
    g_autofree char *escaped_argument = g_regex_escape_string (arguments[i], -1);

    if (i > 0) {
      g_string_append (pattern, "|");
    }

    g_string_append (pattern, "\\b");

    g_string_append (pattern, escaped_argument);

    g_string_append (pattern, "\\b");
  }

  has_argument = g_regex_match_simple (pattern->str, contents, 0, 0);

  return has_argument;
}

static gboolean
is_valid_shell_identifier_character (char     c,
                                     gboolean first)
{
  return (!first && g_ascii_isdigit (c)) ||
         c == '_' ||
         g_ascii_isalpha (c);
}

void
gis_substitute_variables_in_text (char **text,
                                  GisVariableLookupFunc lookup_func,
                                  gpointer user_data)
{
  GString *s = g_string_new ("");
  const char *p, *start;
  char c;

  p = *text;
  while (*p) {
    c = *p;
    if (c == '\\') {
      p++;
      c = *p;
      if (c != '\0') {
        p++;
        switch (c) {
        case '\\':
          g_string_append_c (s, '\\');
          break;
        case '$':
          g_string_append_c (s, '$');
          break;
        default:
          g_string_append_c (s, '\\');
          g_string_append_c (s, c);
          break;
        }
      }
    } else if (c == '$') {
      gboolean brackets = FALSE;
      p++;
      if (*p == '{') {
        brackets = TRUE;
        p++;
      }
      start = p;
      while (*p != '\0' &&
             is_valid_shell_identifier_character (*p, p == start)) {
        p++;
      }
      if (p == start || (brackets && *p != '}')) {
        g_string_append_c (s, '$');
        if (brackets)
          g_string_append_c (s, '{');
        g_string_append_len (s, start, p - start);
      } else {
        g_autofree char *variable = NULL;
        g_autofree char *value = NULL;

        variable = g_strndup (start, p - start);

        if (brackets && *p == '}')
          p++;

        if (lookup_func)
            value = lookup_func (variable, user_data);
        if (value) {
          g_string_append (s, value);
        }
      }
    } else {
      p++;
      g_string_append_c (s, c);
    }
  }
  g_free (*text);
  *text = g_string_free (s, FALSE);
}

