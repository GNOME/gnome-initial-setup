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

#include "config.h"

#include <string.h>

#include <gio/gio.h>

#include "gis-keyring.h"

#include <libsecret/secret.h>

#define DUMMY_PWD "gis"

/* We never want to see a keyring dialog, but we need to make
 * sure a keyring is present.
 *
 * To achieve this, install a prompter for gnome-keyring that
 * never shows any UI, and create a keyring, if one does not
 * exist yet.
 */

void
gis_ensure_login_keyring ()
{
	g_autoptr(GSubprocess) subprocess = NULL;
	g_autoptr(GSubprocessLauncher) launcher = NULL;
	g_autoptr(GError) error = NULL;

	g_debug ("launching gnome-keyring-daemon --unlock");
	launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
	subprocess = g_subprocess_launcher_spawn (launcher, &error, "gnome-keyring-daemon", "--unlock", NULL);
	if (subprocess == NULL) {
		g_warning ("Failed to spawn gnome-keyring-daemon --unlock: %s", error->message);
		return;
	}

	if (!g_subprocess_communicate_utf8 (subprocess, DUMMY_PWD, NULL, NULL, NULL, &error)) {
		g_warning ("Failed to communicate with gnome-keyring-daemon: %s", error->message);
		return;
	}
}

void
gis_update_login_keyring_password (const gchar *new_)
{
	g_autoptr(GDBusConnection) bus = NULL;
	g_autoptr(SecretService) service = NULL;
	g_autoptr(SecretValue) old_secret = NULL;
	g_autoptr(SecretValue) new_secret = NULL;
	g_autoptr(GError) error = NULL;
	
	service = secret_service_get_sync (SECRET_SERVICE_OPEN_SESSION, NULL, &error);
	if (service == NULL) {
		g_warning ("Failed to get secret service: %s", error->message);
		return;
	}

	bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (bus == NULL) {
		g_warning ("Failed to get session bus: %s", error->message);
		return;
	}

	old_secret = secret_value_new (DUMMY_PWD, strlen (DUMMY_PWD), "text/plain");
	new_secret = secret_value_new (new_, strlen (new_), "text/plain");

	g_dbus_connection_call_sync (bus,
                                     "org.gnome.keyring",
                                     "/org/freedesktop/secrets",
                                     "org.gnome.keyring.InternalUnsupportedGuiltRiddenInterface",
                                     "ChangeWithMasterPassword",
                                     g_variant_new ("(o@(oayays)@(oayays))",
                                                    "/org/freedesktop/secrets/collection/login",
                                                    secret_service_encode_dbus_secret (service, old_secret),
                                                    secret_service_encode_dbus_secret (service, new_secret)),
                                     NULL,
                                     0,
                                     G_MAXINT,
                                     NULL, &error);

        if (error != NULL) {
          g_warning ("Failed to change keyring password: %s", error->message);
        }
}
