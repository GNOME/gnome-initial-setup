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
#pragma once

void gis_add_style_from_resource (const char *path);
gboolean gis_kernel_command_line_has_argument (const char *arguments[]);

typedef char * (* GisVariableLookupFunc) (const char *key, gpointer user_data);
void gis_substitute_variables_in_text (char **text, GisVariableLookupFunc lookup_func, gpointer user_data);
