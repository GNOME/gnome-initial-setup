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

#ifndef __GIS_ASSISTANT_PRIVATE_H__
#define __GIS_ASSISTANT_PRIVATE_H__

#include "gis-assistant.h"

G_BEGIN_DECLS

GtkWidget *_gis_assistant_get_frame (GisAssistant *assistant);

void _gis_assistant_current_page_changed (GisAssistant *assistant,
                                          GisPage      *page);

G_END_DECLS

#endif /* __GIS_ASSISTANT_PRIVATE_H__ */
