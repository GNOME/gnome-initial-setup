/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015 Lennart Poettering
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "hostname-utils.h"

#include <glib.h>
#include <glib/gi18n.h>


static gboolean
hostname_valid_char(gchar c)
{
        return  (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                (c == '-') || (c == '_') || (c == '.');
}

/**
 * Check if s looks like a valid host name or FQDN. This does not do
 * full DNS validation, but only checks if the name is composed of
 * allowed characters and the length is not above the maximum allowed
 * by Linux (c.f. dns_name_is_valid()). Trailing dot is allowed if
 * allow_trailing_dot is true and at least two components are present
 * in the name. Note that due to the restricted charset and length
 * this call is substantially more conservative than
 * dns_name_is_valid().
 */
gboolean
hostname_is_valid(const gchar *s, gboolean allow_trailing_dot)
{
        unsigned n_dots = 0;
        const gchar *p;
        gboolean dot;

        if (!s || !s[0])
            return FALSE;

        /* Doesn't accept empty hostnames, hostnames with
         * leading dots, and hostnames with multiple dots in a
         * sequence. Also ensures that the length stays below
         * HOST_NAME_MAX. */

        for (p = s, dot = TRUE; *p; p++) {
                if (*p == '.') {
                        if (dot)
                                return FALSE;

                        dot = TRUE;
                        n_dots++;
                } else {
                        if (!hostname_valid_char(*p))
                                return FALSE;

                        dot = FALSE;
                }
        }

        if (dot && (n_dots < 2 || !allow_trailing_dot))
                return FALSE;

        if (p-s > 64)
                return FALSE;

        return TRUE;
}
