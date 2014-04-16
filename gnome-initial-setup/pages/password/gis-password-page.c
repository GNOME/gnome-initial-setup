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

/* Password page {{{1 */

#define PAGE_ID "password"

#include "config.h"
#include "password-resources.h"
#include "gis-password-page.h"

#include "gis-keyring.h"

#include "um-utils.h"
#include "pw-utils.h"
#include "um-utils.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

struct _GisPasswordPagePrivate
{
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  GtkWidget *password_strength;
  gboolean valid_confirm;
  const gchar *password_reason;
  guint reason_timeout;
  const gchar *username;
};
typedef struct _GisPasswordPagePrivate GisPasswordPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisPasswordPage, gis_password_page, GIS_TYPE_PAGE);

static gboolean
page_validate (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  return priv->valid_confirm;
}

static void
update_page_validation (GisPasswordPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
gis_password_page_save_data (GisPage *gis_page)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (gis_page);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  ActUser *act_user;
  const gchar *password;
  const gchar *old_password;

  if (gis_page->driver == NULL)
    return;

  gis_driver_get_user_permissions (gis_page->driver, &act_user, &password);

  if (act_user == NULL) /* enterprise account */
    return;

  if (password)
    old_password = password;
  else
    old_password = "gis";

  password = gtk_entry_get_text (GTK_ENTRY (priv->password_entry));

  if (strlen (password) == 0)
    act_user_set_password_mode (act_user, ACT_USER_PASSWORD_MODE_NONE);
  else
    act_user_set_password (act_user, password, "");

  gis_driver_set_user_permissions (gis_page->driver, act_user, password);

  gis_update_login_keyring_password (old_password, password);
}

static gboolean
reason_timeout_cb (gpointer data)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (data);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  const gchar *password;
  const gchar *verify;

  password = gtk_entry_get_text (GTK_ENTRY (priv->password_entry));
  verify = gtk_entry_get_text (GTK_ENTRY (priv->confirm_entry));

  if (strlen (password) == 0)
    set_entry_validation_error (GTK_ENTRY (priv->password_entry), _("No password"));
  else
    set_entry_validation_error (GTK_ENTRY (priv->password_entry), priv->password_reason);

  if (strlen (verify) > 0 && !priv->valid_confirm)
    set_entry_validation_error (GTK_ENTRY (priv->confirm_entry), _("Passwords do not match"));

  priv->reason_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
refresh_reason_timeout (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  if (priv->reason_timeout != 0)
    g_source_remove (priv->reason_timeout);

  priv->reason_timeout = g_timeout_add (600, reason_timeout_cb, page);
}

static void
update_valid_confirm (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  const gchar *password, *verify;

  password = gtk_entry_get_text (GTK_ENTRY (priv->password_entry));
  verify = gtk_entry_get_text (GTK_ENTRY (priv->confirm_entry));

  priv->valid_confirm = strcmp (password, verify) == 0;
}

static void
update_password_entries (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  const gchar *password;
  gdouble strength;
  gint strength_level;
  const gchar *hint;
  const gchar *long_hint = NULL;

  password = gtk_entry_get_text (GTK_ENTRY (priv->password_entry));

  strength = pw_strength (password, NULL, priv->username, &hint, &long_hint, &strength_level);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (priv->password_strength), strength_level);

  if (strength == 0.0) {
    priv->password_reason = long_hint ? long_hint : hint;
  }
  update_valid_confirm (page);

  if (priv->valid_confirm)
    clear_entry_validation_error (GTK_ENTRY (priv->password_entry));

  gtk_widget_set_sensitive (priv->confirm_entry, TRUE);

  refresh_reason_timeout (page);
}

static gboolean
password_entry_focus_out (GtkWidget      *widget,
                          GdkEventFocus  *event,
                          GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  if (priv->reason_timeout != 0) {
    g_source_remove (priv->reason_timeout);
    priv->reason_timeout = 0;
  }

  return FALSE;
}

static gboolean
confirm_entry_focus_out (GtkWidget      *widget,
                         GdkEventFocus  *event,
                         GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  GtkEntry *entry = GTK_ENTRY (widget);
  const gchar *verify;

  verify = gtk_entry_get_text (entry);

  if (strlen (verify) > 0 && !priv->valid_confirm)
    set_entry_validation_error (entry, _("Passwords do not match"));
  else
    clear_entry_validation_error (entry);

  return FALSE;
}

static void
password_changed (GtkWidget      *w,
                  GParamSpec     *pspec,
                  GisPasswordPage *page)
{
  clear_entry_validation_error (GTK_ENTRY (w));
  update_password_entries (page);
  update_page_validation (page);
}

static void
username_changed (GObject *obj, GParamSpec *pspec, GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  priv->username = gis_driver_get_username (GIS_DRIVER (obj));

  if (priv->username)
    gtk_widget_show (GTK_WIDGET (page));
  else
    gtk_widget_hide (GTK_WIDGET (page));  
}

static void
gis_password_page_constructed (GObject *object)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (object);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_password_page_parent_class)->constructed (object);

  g_signal_connect (priv->password_entry, "notify::text",
                    G_CALLBACK (password_changed), page);
  g_signal_connect (priv->confirm_entry, "notify::text",
                    G_CALLBACK (password_changed), page);
  g_signal_connect_after (priv->password_entry, "focus-out-event",
                          G_CALLBACK (password_entry_focus_out), page);
  g_signal_connect_after (priv->confirm_entry, "focus-out-event",
                          G_CALLBACK (confirm_entry_focus_out), page);

  g_signal_connect (GIS_PAGE (page)->driver, "notify::username",
                    G_CALLBACK (username_changed), page);

  update_page_validation (page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_password_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Password"));
}

static void
gis_password_page_class_init (GisPasswordPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-password-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, confirm_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_strength);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_password_page_locale_changed;
  page_class->save_data = gis_password_page_save_data;
  object_class->constructed = gis_password_page_constructed;
}

static void
gis_password_page_init (GisPasswordPage *page)
{
  g_resources_register (password_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_prepare_password_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_PASSWORD_PAGE,
                                     "driver", driver,
                                     NULL));
}

