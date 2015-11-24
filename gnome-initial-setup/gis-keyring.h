
/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2014 Red Hat
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
 *     Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GIS_KEYRING_H__
#define __GIS_KEYRING_H__

#include <glib-object.h>

G_BEGIN_DECLS

void	gis_ensure_login_keyring	  ();
void	gis_update_login_keyring_password (const gchar *new_);

G_END_DECLS

#endif /* __GIS_KEYRING_H__ */
