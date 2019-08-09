/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <utmp.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "um-utils.h"

void
set_entry_validation_checkmark (GtkEntry *entry)
{
        g_object_set (entry, "caps-lock-warning", FALSE, NULL);
        gtk_entry_set_icon_from_icon_name (entry,
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "object-select-symbolic");
}

void
set_entry_validation_error (GtkEntry    *entry,
                            const gchar *text)
{
        g_object_set (entry, "caps-lock-warning", FALSE, NULL);
        gtk_entry_set_icon_from_stock (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       GTK_STOCK_CAPS_LOCK_WARNING);
        gtk_entry_set_icon_tooltip_text (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         text);
}

void
clear_entry_validation_error (GtkEntry *entry)
{
        gboolean warning;

        g_object_get (entry, "caps-lock-warning", &warning, NULL);

        if (warning)
                return;

        gtk_entry_set_icon_from_pixbuf (entry,
                                        GTK_ENTRY_ICON_SECONDARY,
                                        NULL);
        g_object_set (entry, "caps-lock-warning", TRUE, NULL);
}

void
popup_menu_below_button (GtkMenu   *menu,
                         gint      *x,
                         gint      *y,
                         gboolean  *push_in,
                         GtkWidget *button)
{
        GtkRequisition menu_req;
        GtkTextDirection direction;
        GtkAllocation allocation;

        gtk_widget_get_preferred_size (GTK_WIDGET (menu), NULL, &menu_req);

        direction = gtk_widget_get_direction (button);

        gdk_window_get_origin (gtk_widget_get_window (button), x, y);
        gtk_widget_get_allocation (button, &allocation);
        *x += allocation.x;
        *y += allocation.y + allocation.height;

        if (direction == GTK_TEXT_DIR_LTR)
                *x += MAX (allocation.width - menu_req.width, 0);
        else if (menu_req.width > allocation.width)
                *x -= menu_req.width - allocation.width;

        *push_in = TRUE;
}

void
down_arrow (GtkStyleContext *context,
            cairo_t         *cr,
            gint             x,
            gint             y,
            gint             width,
            gint             height)
{
        GtkStateFlags flags;
        GdkRGBA fg_color;
        GdkRGBA outline_color;
        gdouble vertical_overshoot;
        gint diameter;
        gdouble radius;
        gdouble x_double, y_double;
        gdouble angle;
        gint line_width;

        flags = gtk_style_context_get_state (context);

        gtk_style_context_get_color (context, flags, &fg_color);
        gtk_style_context_get_border_color (context, flags, &outline_color);

        line_width = 1;
        angle = G_PI / 2;
        vertical_overshoot = line_width / 2.0 * (1. / tan (G_PI / 8));
        if (line_width % 2 == 1)
                vertical_overshoot = ceil (0.5 + vertical_overshoot) - 0.5;
        else
                vertical_overshoot = ceil (vertical_overshoot);
        diameter = (gint) MAX (3, width - 2 * vertical_overshoot);
        diameter -= (1 - (diameter + line_width) % 2);
        radius = diameter / 2.;
        x_double = floor ((x + width / 2) - (radius + line_width) / 2.) + (radius + line_width) / 2.;

        y_double = (y + height / 2) - 0.5;

        cairo_save (cr);

        cairo_translate (cr, x_double, y_double);
        cairo_rotate (cr, angle);

        cairo_move_to (cr, - radius / 2., - radius);
        cairo_line_to (cr,   radius / 2.,   0);
        cairo_line_to (cr, - radius / 2.,   radius);

        cairo_close_path (cr);

        cairo_set_line_width (cr, line_width);

        gdk_cairo_set_source_rgba (cr, &fg_color);

        cairo_fill_preserve (cr);

        gdk_cairo_set_source_rgba (cr, &outline_color);
        cairo_stroke (cr);

        cairo_restore (cr);
}

#define MAXNAMELEN  (UT_NAMESIZE - 1)

static gboolean
is_username_used (const gchar *username)
{
        struct passwd *pwent;

        if (username == NULL || username[0] == '\0') {
                return FALSE;
        }

        pwent = getpwnam (username);

        return pwent != NULL;
}

gboolean
is_valid_name (const gchar *name)
{
        gboolean is_empty = TRUE;
        const gchar *c;

        /* Valid names must contain:
         *   1) at least one character.
         *   2) at least one non-"space" character.
         */
        for (c = name; *c; c++) {
                gunichar unichar;

                unichar = g_utf8_get_char_validated (c, -1);

                /* Partial UTF-8 sequence or end of string */
                if (unichar == (gunichar) -1 || unichar == (gunichar) -2)
                        break;

                /* Check for non-space character */
                if (!g_unichar_isspace (unichar)) {
                        is_empty = FALSE;
                        break;
                }
        }

        return !is_empty;
}

gboolean
is_valid_username (const gchar *username, gboolean parental_controls_enabled, gchar **tip)
{
        gboolean empty;
        gboolean in_use;
        gboolean too_long;
        gboolean valid;
        gboolean parental_controls_conflict;
        const gchar *c;

        if (username == NULL || username[0] == '\0') {
                empty = TRUE;
                in_use = FALSE;
                too_long = FALSE;
        } else {
                empty = FALSE;
                in_use = is_username_used (username);
                too_long = strlen (username) > MAXNAMELEN;
        }
        valid = TRUE;

        if (!in_use && !empty && !too_long) {
                /* First char must be a letter, and it must only composed
                 * of ASCII letters, digits, and a '.', '-', '_'
                 */
                for (c = username; *c; c++) {
                        if (! ((*c >= 'a' && *c <= 'z') ||
                               (*c >= 'A' && *c <= 'Z') ||
                               (*c >= '0' && *c <= '9') ||
                               (*c == '_') || (*c == '.') ||
                               (*c == '-' && c != username)))
                           valid = FALSE;
                }
        }

        parental_controls_conflict = (parental_controls_enabled && g_strcmp0 (username, "administrator") == 0);

        valid = !empty && !in_use && !too_long && !parental_controls_conflict && valid;

        if (!empty && (in_use || too_long || parental_controls_conflict || !valid)) {
                if (in_use) {
                        *tip = g_strdup (_("Sorry, that user name isn’t available. Please try another."));
                }
                else if (too_long) {
                        *tip = g_strdup_printf (_("The username is too long."));
                }
                else if (username[0] == '-') {
                        *tip = g_strdup (_("The username cannot start with a “-”."));
                }
                else if (parental_controls_conflict) {
                        *tip = g_strdup (_("That username isn’t available. Please try another."));
                }
                else {
                        *tip = g_strdup (_("The username should only consist of upper and lower case letters from a-z, digits and the following characters: . - _"));
                }
        }
        else {
                *tip = g_strdup (_("This will be used to name your home folder and can’t be changed."));
        }

        return valid;
}

void
generate_username_choices (const gchar  *name,
                           GtkListStore *store)
{
        gboolean in_use, same_as_initial;
        char *lc_name, *ascii_name, *stripped_name;
        char **words1;
        char **words2 = NULL;
        char **w1, **w2;
        char *c;
        char *unicode_fallback = "?";
        GString *first_word, *last_word;
        GString *item0, *item1, *item2, *item3, *item4;
        int len;
        int nwords1, nwords2, i;
        GHashTable *items;
        GtkTreeIter iter;

        gtk_list_store_clear (store);

        ascii_name = g_convert_with_fallback (name, -1, "ASCII//TRANSLIT", "UTF-8",
                                              unicode_fallback, NULL, NULL, NULL);

        lc_name = g_ascii_strdown (ascii_name, -1);

        /* Remove all non ASCII alphanumeric chars from the name,
         * apart from the few allowed symbols.
         *
         * We do remove '.', even though it is usually allowed,
         * since it often comes in via an abbreviated middle name,
         * and the dot looks just wrong in the proposals then.
         */
        stripped_name = g_strnfill (strlen (lc_name) + 1, '\0');
        i = 0;
        for (c = lc_name; *c; c++) {
                if (!(g_ascii_isdigit (*c) || g_ascii_islower (*c) ||
                    *c == ' ' || *c == '-' || *c == '_' ||
                    /* used to track invalid words, removed below */
                    *c == '?') )
                        continue;

                    stripped_name[i] = *c;
                    i++;
        }

        if (strlen (stripped_name) == 0) {
                g_free (ascii_name);
                g_free (lc_name);
                g_free (stripped_name);
                return;
        }

        /* we split name on spaces, and then on dashes, so that we can treat
         * words linked with dashes the same way, i.e. both fully shown, or
         * both abbreviated
         */
        words1 = g_strsplit_set (stripped_name, " ", -1);
        len = g_strv_length (words1);

        /* The default item is a concatenation of all words without ? */
        item0 = g_string_sized_new (strlen (stripped_name));

        g_free (ascii_name);
        g_free (lc_name);
        g_free (stripped_name);

        /* Concatenate the whole first word with the first letter of each
         * word (item1), and the last word with the first letter of each
         * word (item2). item3 and item4 are symmetrical respectively to
         * item1 and item2.
         *
         * Constant 5 is the max reasonable number of words we may get when
         * splitting on dashes, since we can't guess it at this point,
         * and reallocating would be too bad.
         */
        item1 = g_string_sized_new (strlen (words1[0]) + len - 1 + 5);
        item3 = g_string_sized_new (strlen (words1[0]) + len - 1 + 5);

        item2 = g_string_sized_new (strlen (words1[len - 1]) + len - 1 + 5);
        item4 = g_string_sized_new (strlen (words1[len - 1]) + len - 1 + 5);

        /* again, guess at the max size of names */
        first_word = g_string_sized_new (20);
        last_word = g_string_sized_new (20);

        nwords1 = 0;
        nwords2 = 0;
        for (w1 = words1; *w1; w1++) {
                if (strlen (*w1) == 0)
                        continue;

                /* skip words with string '?', most likely resulting
                 * from failed transliteration to ASCII
                 */
                if (strstr (*w1, unicode_fallback) != NULL)
                        continue;

                nwords1++; /* count real words, excluding empty string */

                item0 = g_string_append (item0, *w1);

                words2 = g_strsplit_set (*w1, "-", -1);
                /* reset last word if a new non-empty word has been found */
                if (strlen (*words2) > 0)
                        last_word = g_string_set_size (last_word, 0);

                for (w2 = words2; *w2; w2++) {
                        if (strlen (*w2) == 0)
                                continue;

                        nwords2++;

                        /* part of the first "toplevel" real word */
                        if (nwords1 == 1) {
                                item1 = g_string_append (item1, *w2);
                                first_word = g_string_append (first_word, *w2);
                        }
                        else {
                                item1 = g_string_append_unichar (item1,
                                                                 g_utf8_get_char (*w2));
                                item3 = g_string_append_unichar (item3,
                                                                 g_utf8_get_char (*w2));
                        }

                        /* not part of the last "toplevel" word */
                        if (w1 != words1 + len - 1) {
                                item2 = g_string_append_unichar (item2,
                                                                 g_utf8_get_char (*w2));
                                item4 = g_string_append_unichar (item4,
                                                                 g_utf8_get_char (*w2));
                        }

                        /* always save current word so that we have it if last one reveals empty */
                        last_word = g_string_append (last_word, *w2);
                }

                g_strfreev (words2);
        }
        item2 = g_string_append (item2, last_word->str);
        item3 = g_string_append (item3, first_word->str);
        item4 = g_string_prepend (item4, last_word->str);

        items = g_hash_table_new (g_str_hash, g_str_equal);

        in_use = is_username_used (item0->str);
        if (!in_use && !g_ascii_isdigit (item0->str[0])) {
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, 0, item0->str, -1);
                g_hash_table_insert (items, item0->str, item0->str);
        }

        in_use = is_username_used (item1->str);
        same_as_initial = (g_strcmp0 (item0->str, item1->str) == 0);
        if (!same_as_initial && nwords2 > 0 && !in_use && !g_ascii_isdigit (item1->str[0])) {
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, 0, item1->str, -1);
                g_hash_table_insert (items, item1->str, item1->str);
        }

        /* if there's only one word, would be the same as item1 */
        if (nwords2 > 1) {
                /* add other items */
                in_use = is_username_used (item2->str);
                if (!in_use && !g_ascii_isdigit (item2->str[0]) &&
                    !g_hash_table_lookup (items, item2->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, item2->str, -1);
                        g_hash_table_insert (items, item2->str, item2->str);
                }

                in_use = is_username_used (item3->str);
                if (!in_use && !g_ascii_isdigit (item3->str[0]) &&
                    !g_hash_table_lookup (items, item3->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, item3->str, -1);
                        g_hash_table_insert (items, item3->str, item3->str);
                }

                in_use = is_username_used (item4->str);
                if (!in_use && !g_ascii_isdigit (item4->str[0]) &&
                    !g_hash_table_lookup (items, item4->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, item4->str, -1);
                        g_hash_table_insert (items, item4->str, item4->str);
                }

                /* add the last word */
                in_use = is_username_used (last_word->str);
                if (!in_use && !g_ascii_isdigit (last_word->str[0]) &&
                    !g_hash_table_lookup (items, last_word->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, last_word->str, -1);
                        g_hash_table_insert (items, last_word->str, last_word->str);
                }

                /* ...and the first one */
                in_use = is_username_used (first_word->str);
                if (!in_use && !g_ascii_isdigit (first_word->str[0]) &&
                    !g_hash_table_lookup (items, first_word->str)) {
                        gtk_list_store_append (store, &iter);
                        gtk_list_store_set (store, &iter, 0, first_word->str, -1);
                        g_hash_table_insert (items, first_word->str, first_word->str);
                }
        }

        g_hash_table_destroy (items);
        g_strfreev (words1);
        g_string_free (first_word, TRUE);
        g_string_free (last_word, TRUE);
        g_string_free (item0, TRUE);
        g_string_free (item1, TRUE);
        g_string_free (item2, TRUE);
        g_string_free (item3, TRUE);
        g_string_free (item4, TRUE);
}

#define IMAGE_SIZE 512
#define IMAGE_BORDER_WIDTH 7.2  /* At least 1px when avatar rendered at 72px */

/* U+1F464 "bust in silhouette"
 * U+FE0E Variant Selector 15 to force text style (monochrome) emoji
 */
#define PLACEHOLDER "\U0001F464\U0000FE0E"

static gchar *
extract_initials_from_name (const gchar *name)
{
        GString *initials = g_string_new ("");
        gchar *p;
        gchar *normalized;
        gunichar unichar;

        if (name == NULL || name[0] == '\0') {
            g_string_free (initials, TRUE);
            return g_strdup (PLACEHOLDER);
        }

        p = g_utf8_strup (name, -1);
        normalized = g_utf8_normalize (g_strstrip (p), -1, G_NORMALIZE_DEFAULT_COMPOSE);
        g_clear_pointer (&p, g_free);
        if (normalized == NULL) {
                g_free (normalized);
                g_string_free (initials, TRUE);
                return g_strdup (PLACEHOLDER);
        }

        unichar = g_utf8_get_char (normalized);
        g_string_append_unichar (initials, unichar);

        p = g_utf8_strrchr (normalized, -1, ' ');
        if (p != NULL && g_utf8_next_char (p) != NULL) {
                p = g_utf8_next_char (p);

                unichar = g_utf8_get_char (p);
                g_string_append_unichar (initials, unichar);
        }

        g_free (normalized);

        return g_string_free (initials, FALSE);
}

GdkRGBA
get_color_for_name (const gchar *name)
{
        // https://gitlab.gnome.org/Community/Design/HIG-app-icons/blob/master/GNOME%20HIG.gpl
        static gdouble gnome_color_palette[][3] = {
                {  98, 160, 234 },
                {  53, 132, 228 },
                {  28, 113, 216 },
                {  26,  95, 180 },
                {  87, 227, 137 },
                {  51, 209, 122 },
                {  46, 194, 126 },
                {  38, 162, 105 },
                { 248, 228,  92 },
                { 246, 211,  45 },
                { 245, 194,  17 },
                { 229, 165,  10 },
                { 255, 163,  72 },
                { 255, 120,   0 },
                { 230,  97,   0 },
                { 198,  70,   0 },
                { 237,  51,  59 },
                { 224,  27,  36 },
                { 192,  28,  40 },
                { 165,  29,  45 },
                { 192,  97, 203 },
                { 163,  71, 186 },
                { 129,  61, 156 },
                {  97,  53, 131 },
                { 181, 131,  90 },
                { 152, 106,  68 },
                { 134,  94,  60 },
                {  99,  69,  44 }
        };

        GdkRGBA color = { 255, 255, 255, 1.0 };
        guint hash;
        gint number_of_colors;
        gint idx;

        if (name == NULL || name[0] == '\0') {
                idx = 5;
        } else {
                hash = g_str_hash (name);
                number_of_colors = G_N_ELEMENTS (gnome_color_palette);
                idx = hash % number_of_colors;
        }

        color.red   = gnome_color_palette[idx][0];
        color.green = gnome_color_palette[idx][1];
        color.blue  = gnome_color_palette[idx][2];

        return color;
}

static void
darken_color (GdkRGBA *color, gdouble fraction) {
        color->red = CLAMP (color->red + color->red * fraction, 0, 255);
        color->green = CLAMP (color->green + color->green * fraction, 0, 255);
        color->blue = CLAMP (color->blue + color->blue * fraction, 0, 255);
}

cairo_surface_t *
generate_user_picture (const gchar *name) {
        PangoFontDescription *font_desc;
        g_autofree gchar *initials = extract_initials_from_name (name);
        g_autofree gchar *font = g_strdup_printf ("Sans %d", (int)ceil (IMAGE_SIZE / 2.5));
        PangoLayout *layout;
        GdkRGBA color = get_color_for_name (name);
        cairo_surface_t *surface;
        gint width, height;
        cairo_t *cr;

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                              IMAGE_SIZE,
                                              IMAGE_SIZE);
        cr = cairo_create (surface);

        cairo_arc (cr, IMAGE_SIZE/2, IMAGE_SIZE/2, IMAGE_SIZE/2, 0, 2 * G_PI);
        cairo_set_source_rgb (cr, color.red/255.0, color.green/255.0, color.blue/255.0);
        cairo_fill (cr);

        cairo_arc (cr, IMAGE_SIZE / 2, IMAGE_SIZE / 2, IMAGE_SIZE / 2 - IMAGE_BORDER_WIDTH / 2,
                   0, 2 * G_PI);
        darken_color (&color, -0.3);
        cairo_set_source_rgb (cr, color.red / 255.0, color.green / 255.0, color.blue / 255.0);
        cairo_set_line_width (cr, IMAGE_BORDER_WIDTH);
        cairo_stroke (cr);

        /* Draw the initials on top */
        cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
        layout = pango_cairo_create_layout (cr);
        pango_layout_set_text (layout, initials, -1);
        font_desc = pango_font_description_from_string (font);
        pango_layout_set_font_description (layout, font_desc);
        pango_font_description_free (font_desc);

        pango_layout_get_size (layout, &width, &height);
        cairo_translate (cr, IMAGE_SIZE/2, IMAGE_SIZE/2);
        cairo_move_to (cr, - ((double)width / PANGO_SCALE)/2, - ((double)height/PANGO_SCALE)/2);
        pango_cairo_show_layout (cr, layout);
        cairo_destroy (cr);

        return surface;
}

GdkPixbuf *
round_image (GdkPixbuf *image)
{
        GdkPixbuf *dest = NULL;
        cairo_surface_t *surface;
        cairo_t *cr;
        gint size;

        size = gdk_pixbuf_get_width (image);
        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
        cr = cairo_create (surface);

        /* Clip a circle */
        cairo_arc (cr, size/2, size/2, size/2, 0, 2 * G_PI);
        cairo_clip (cr);
        cairo_new_path (cr);

        gdk_cairo_set_source_pixbuf (cr, image, 0, 0);
        cairo_paint (cr);

        dest = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
        cairo_surface_destroy (surface);
        cairo_destroy (cr);

        return dest;
}
