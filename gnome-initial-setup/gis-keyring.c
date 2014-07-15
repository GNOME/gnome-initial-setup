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

/* We never want to see a keyring dialog, but we need to make
 * sure a keyring is present.
 *
 * To achieve this, install a prompter for gnome-keyring that
 * never shows any UI, and create a keyring, if one does not
 * exist yet.
 */

void
gis_ensure_login_keyring (const gchar *pwd)
{
	GSubprocess *subprocess = NULL;
	GSubprocessLauncher *launcher = NULL;
	GError *error = NULL;

	g_debug ("launching gnome-keyring-daemon --login");
	launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
	subprocess = g_subprocess_launcher_spawn (launcher, &error, "gnome-keyring-daemon", "--unlock", NULL);
	if (subprocess == NULL) {
		g_warning ("Failed to spawn gnome-keyring-daemon --unlock: %s", error->message);
		g_error_free (error);
		goto out;
	}

	if (!g_subprocess_communicate_utf8 (subprocess, "gis", NULL, NULL, NULL, &error)) {
		g_warning ("Failed to communicate with gnome-keyring-daemon: %s", error->message);
		g_error_free (error);
		goto out;
	}

out:
	if (subprocess)
		g_object_unref (subprocess);
	if (launcher)
		g_object_unref (launcher);
}

void
gis_update_login_keyring_password (const gchar *old_, const gchar *new_)
{
	GDBusConnection *bus = NULL;
	SecretService *service = NULL;
	SecretValue *old_secret = NULL;
	SecretValue *new_secret = NULL;
	GError *error = NULL;
	
	service = secret_service_get_sync (SECRET_SERVICE_OPEN_SESSION, NULL, &error);
	if (service == NULL) {
		g_warning ("Failed to get secret service: %s", error->message);
		g_error_free (error);
		goto out;
	}

	bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (bus == NULL) {
		g_warning ("Failed to get session bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	old_secret = secret_value_new (old_, strlen (old_), "text/plain");
	new_secret = secret_value_new (new_, strlen (new_), "text/plain");

	g_dbus_connection_call (bus,
				"org.gnome.keyring",
				"/org/gnome/keyring",
			        "org.gnome.keyring.InternalUnsupportedGuiltRiddenInterface",
				"ChangeWithMasterPassword",
				g_variant_new ("(o@(oayays)@(oayays))",
					       "/org/freedesktop/secrets/collection/login",
					       secret_service_encode_dbus_secret (service, old_secret),
					       secret_service_encode_dbus_secret (service, new_secret)),
				NULL,
				0,
				G_MAXINT,
				NULL, NULL, NULL);

out:

	if (service)
		g_object_unref (service);
	if (bus)
		g_object_unref (bus);
	if (old_secret)
		secret_value_unref (old_secret);
	if (new_secret)
		secret_value_unref (new_secret);
}
