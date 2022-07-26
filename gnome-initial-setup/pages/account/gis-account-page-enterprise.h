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

/* For GisPageApplyCallback */
#include "gis-page.h"

G_BEGIN_DECLS

#define GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE (gis_account_page_enterprise_get_type ())
G_DECLARE_FINAL_TYPE (GisAccountPageEnterprise, gis_account_page_enterprise, GIS, ACCOUNT_PAGE_ENTERPRISE, AdwBin)

gboolean gis_account_page_enterprise_validate (GisAccountPageEnterprise *enterprise);
gboolean gis_account_page_enterprise_apply (GisAccountPageEnterprise *enterprise,
                                            GCancellable             *cancellable,
                                            GisPageApplyCallback      callback,
                                            gpointer                  data);
void     gis_account_page_enterprise_shown (GisAccountPageEnterprise *enterprise);

G_END_DECLS

