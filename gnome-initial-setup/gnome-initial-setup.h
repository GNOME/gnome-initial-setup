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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __GNOME_INITIAL_SETUP_H__
#define __GNOME_INITIAL_SETUP_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

typedef struct _GisDriver    GisDriver;
typedef struct _GisAssistant GisAssistant;
typedef struct _GisPage      GisPage;

#include "gis-driver.h"
#include "gis-assistant.h"
#include "gis-page.h"
#include "gis-pkexec.h"
#include "gis-util.h"

void gis_ensure_stamp_files (GisDriver *driver);
gboolean gis_get_mock_mode (void);

#endif /* __GNOME_INITIAL_SETUP_H__ */

