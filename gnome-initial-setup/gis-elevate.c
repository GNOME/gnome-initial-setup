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

#include "gis-elevate.h"

static gboolean
elevate (const char  *tool,
         const char  *command,
         const char  *arg1,
         const char  *user,
         GError     **error)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;
  const char * const root_argv[] = { tool, command, arg1, NULL };
  const char * const user_argv[] = { tool, "--user", user, command, arg1, NULL };
  const char * const *argv = user == NULL ? root_argv : user_argv;

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);

  if (g_strcmp0 (tool, "pkexec") == 0) {
    /* pkexec won't let us run the program if $SHELL isn't in /etc/shells,
     * so remove it from the environment.
     */
    g_subprocess_launcher_unsetenv (launcher, "SHELL");
  }
  process = g_subprocess_launcher_spawnv (launcher, argv, error);

  if (!process) {
    g_prefix_error (error, "Failed to create %s process: ", command);
    return FALSE;
  }

  if (!g_subprocess_wait_check (process, NULL, error)) {
    g_prefix_error (error, "%s failed: ", command);
    return FALSE;
  }

  return TRUE;
}

gboolean
gis_elevate (const char  *command,
             const char  *arg1,
             const char  *user,
             GError     **error)
{
  g_autoptr(GError) local_error = NULL;

  if (!elevate ("run0", command, arg1, user, &local_error)) {
    /* If run0 is not found, fall back to pkexec */
    if (g_error_matches (local_error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT)) {
      return elevate ("pkexec", command, arg1, user, error);
    }
    g_propagate_error (error, g_steal_pointer (&local_error));
    return FALSE;
  }
  return TRUE;
}
