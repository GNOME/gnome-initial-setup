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

#include "config.h"
#include "gis-account-pages.h"
#include "gis-account-page.h"

GisPage *
gis_prepare_account_page (GisDriver *driver)
{
  GisDriverMode driver_mode;

  driver_mode = gis_driver_get_mode (driver);

  if (driver_mode == GIS_DRIVER_MODE_LIVE_USER && !gis_kernel_command_line_has_argument ((const char *[]) { "rd.live.overlay", NULL })) {
    ActUserManager *act_client = act_user_manager_get_default ();
    const char *username = "liveuser";
    g_autoptr(ActUser) user = NULL;
    g_autoptr(GError) error = NULL;

    user = act_user_manager_create_user (act_client, username, username, ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR, &error);

    if (user != NULL) {
      act_user_set_password_mode (user, ACT_USER_PASSWORD_MODE_NONE);
      gis_driver_set_username (driver, username);
      gis_driver_set_account_mode (driver, UM_LOCAL);
      gis_driver_set_user_permissions (driver, user, NULL);
    }
    return NULL;
  }

  return g_object_new (GIS_TYPE_ACCOUNT_PAGE,
                       "driver", driver,
                       NULL);
}
