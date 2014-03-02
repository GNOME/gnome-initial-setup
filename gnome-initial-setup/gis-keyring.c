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

#include <gio/gio.h>

#include "gis-keyring.h"

#include <libsecret/secret.h>
#include <gcr/gcr.h>

#include "gis-prompt.h"

/* We never want to see a keyring dialog, but we need to make
 * sure a keyring is present.
 *
 * To achieve this, install a prompter for gnome-keyring that
 * never shows any UI, and create a keyring, if one does not
 * exist yet.
 */

#define GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME "org.gnome.keyring.SystemPrompter"

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GcrSystemPrompter *prompter;

  prompter = gcr_system_prompter_new (GCR_SYSTEM_PROMPTER_SINGLE, GIS_TYPE_PROMPT);
  gcr_system_prompter_register (prompter, connection);
}

static void
created_collection (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  SecretCollection *collection;
  GError *error = NULL;

  collection = secret_collection_create_finish (result, &error);
  if (collection)
    {
      g_debug ("Created keyring '%s', %s\n",
               secret_collection_get_label (collection),
               secret_collection_get_locked (collection) ? "locked" : "unlocked");
      g_object_unref (collection);
    }
  else
    {
      g_warning ("Failed to create keyring: %s\n", error->message);
      g_error_free (error);
    }
}

static void
got_alias (GObject      *source,
           GAsyncResult *result,
           gpointer      user_data)
{
  SecretCollection *collection;

  collection = secret_collection_for_alias_finish (result, NULL);
  if (collection)
    {
      g_debug ("Found default keyring '%s', %s\n",
               secret_collection_get_label (collection),
               secret_collection_get_locked (collection) ? "locked" : "unlocked");
      g_object_unref (collection);
    }
  else
    {
      secret_collection_create (NULL, "login", SECRET_COLLECTION_DEFAULT, 0, NULL, created_collection, NULL);
    }
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_debug ("Got " GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME "\n");

  secret_collection_for_alias (NULL, SECRET_COLLECTION_DEFAULT, SECRET_COLLECTION_NONE, NULL, got_alias, NULL);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_debug ("Lost " GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME "\n");
}

void
gis_ensure_keyring (void)
{
  g_bus_own_name (G_BUS_TYPE_SESSION,
                  GCR_DBUS_PROMPTER_SYSTEM_BUS_NAME,
                  G_BUS_NAME_OWNER_FLAGS_REPLACE,
                  on_bus_acquired,
                  on_name_acquired,
                  on_name_lost,
                  NULL, NULL);
}

