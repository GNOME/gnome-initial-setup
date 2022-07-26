/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
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
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GIS_TYPE_ACCOUNT_PAGE_LOCAL (gis_account_page_local_get_type ())
G_DECLARE_FINAL_TYPE (GisAccountPageLocal, gis_account_page_local, GIS, ACCOUNT_PAGE_LOCAL, AdwBin)

gboolean gis_account_page_local_validate (GisAccountPageLocal *local);
gboolean gis_account_page_local_apply (GisAccountPageLocal *local, GisPage *page);
gboolean gis_account_page_local_create_user (GisAccountPageLocal  *local,
                                             GisPage              *page,
                                             GError              **error);
void gis_account_page_local_shown (GisAccountPageLocal *local);

G_END_DECLS

