/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __GNOME_INITIAL_SETUP_H__
#define __GNOME_INITIAL_SETUP_H__

#include <gtk/gtk.h>

#include "gis-assistant.h"
#include "gis-page.h"

#include <act/act-user-manager.h>

G_BEGIN_DECLS

typedef struct _SetupData SetupData;

GtkWindow *gis_get_main_window (SetupData *setup);
GisAssistant * gis_get_assistant (SetupData *setup);
void gis_locale_changed (SetupData *setup);

void gis_set_user_permissions (SetupData   *setup,
                               ActUser     *user,
                               const gchar *password);

void gis_get_user_permissions (SetupData    *setup,
                               ActUser     **user,
                               const gchar **password);

G_END_DECLS

#endif /* __GNOME_INITIAL_SETUP_H__ */

