/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2015-2016 Red Hat
 * Copyright (C) 2015-2017 Endless OS Foundation LLC
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
 *     Dan Nicholson <dbn@endlessos.org>
 *     Will Thompson <wjt@endlessos.org>
 */

#ifndef __GIS_ELEVATE_H__
#define __GIS_ELEVATE_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

gboolean
gis_elevate (const char  *command,
             const char  *arg1,
             const char  *user,
             GError     **error);

G_END_DECLS

#endif /* __GIS_ELEVATE_H__ */
