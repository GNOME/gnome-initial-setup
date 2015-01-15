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

#include "config.h"

#include "gnome-initial-setup.h"

#include <stdlib.h>
#include <locale.h>

#include "gis-assistant.h"

#define GIS_TYPE_DRIVER_MODE (gis_driver_mode_get_type ())

/* Statically include this for now. Maybe later
 * we'll generate this from glib-mkenums. */
GType
gis_driver_mode_get_type (void) {
  static GType enum_type_id = 0;
  if (G_UNLIKELY (!enum_type_id))
    {
      static const GEnumValue values[] = {
        { GIS_DRIVER_MODE_NEW_USER, "GIS_DRIVER_MODE_NEW_USER", "new_user" },
        { GIS_DRIVER_MODE_EXISTING_USER, "GIS_DRIVER_MODE_EXISTING_USER", "existing_user" },
        { 0, NULL, NULL }
      };
      enum_type_id = g_enum_register_static("GisDriverMode", values);
    }
  return enum_type_id;
}

enum {
  REBUILD_PAGES,
  LOCALE_CHANGED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

enum {
  PROP_0,
  PROP_MODE,
  PROP_USERNAME,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

struct _GisDriverPrivate {
  GtkWindow *main_window;
  GisAssistant *assistant;

  ActUser *user_account;
  const gchar *user_password;

  gchar *lang_id;
  gchar *username;

  GisDriverMode mode;
};
typedef struct _GisDriverPrivate GisDriverPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GisDriver, gis_driver, GTK_TYPE_APPLICATION)

static void
gis_driver_finalize (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  g_free (priv->lang_id);
  g_free (priv->username);

  G_OBJECT_CLASS (gis_driver_parent_class)->finalize (object);
}

static void
prepare_main_window (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GdkGeometry size_hints;

  if (gis_driver_is_small_screen (driver))
    {
      GtkWidget *child, *sw;

      child = g_object_ref (gtk_bin_get_child (GTK_BIN (priv->main_window)));
      gtk_container_remove (GTK_CONTAINER (priv->main_window), child);
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_show (sw);
      gtk_container_add (GTK_CONTAINER (priv->main_window), sw);
      gtk_container_add (GTK_CONTAINER (sw), child);
      g_object_unref (child);

      gtk_window_maximize (priv->main_window);
    }
  else
    {
      size_hints.min_width = 1024;
      size_hints.min_height = 768;
      size_hints.win_gravity = GDK_GRAVITY_CENTER;

      gtk_window_set_geometry_hints (priv->main_window,
                                     NULL,
                                     &size_hints,
                                     GDK_HINT_MIN_SIZE | GDK_HINT_WIN_GRAVITY);
      gtk_window_set_resizable (priv->main_window, FALSE);
      gtk_window_set_position (priv->main_window, GTK_WIN_POS_CENTER_ALWAYS);
    }

  gtk_window_set_titlebar (priv->main_window,
                           gis_assistant_get_titlebar (priv->assistant));
}

static void
rebuild_pages (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[REBUILD_PAGES], 0);
}

GisAssistant *
gis_driver_get_assistant (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->assistant;
}

void
gis_driver_set_user_language (GisDriver *driver, const gchar *lang_id)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  g_free (priv->lang_id);
  priv->lang_id = g_strdup (lang_id);
}

const gchar *
gis_driver_get_user_language (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->lang_id;
}

void
gis_driver_set_username (GisDriver *driver, const gchar *username)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  g_free (priv->username);
  priv->username = g_strdup (username);
  g_object_notify (G_OBJECT (driver), "username");
}

const gchar *
gis_driver_get_username (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->username;
}
void
gis_driver_set_user_permissions (GisDriver   *driver,
                                 ActUser     *user,
                                 const gchar *password)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  priv->user_account = user;
  priv->user_password = password;
}

void
gis_driver_get_user_permissions (GisDriver    *driver,
                                 ActUser     **user,
                                 const gchar **password)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  *user = priv->user_account;
  *password = priv->user_password;
}

void
gis_driver_add_page (GisDriver *driver,
                     GisPage   *page)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  gis_assistant_add_page (priv->assistant, page);
}

void
gis_driver_hide_window (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  gtk_widget_hide (GTK_WIDGET (priv->main_window));
}

static void
gis_driver_real_locale_changed (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  rebuild_pages (driver);
  gis_assistant_locale_changed (priv->assistant);
}

void
gis_driver_locale_changed (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[LOCALE_CHANGED], 0);
}

GisDriverMode
gis_driver_get_mode (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->mode;
}

gboolean
gis_driver_is_small_screen (GisDriver *driver)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  if (g_getenv ("GIS_SMALL_SCREEN"))
    return TRUE;

  return gdk_screen_get_height (screen) < 800;
}

static void
gis_driver_get_property (GObject      *object,
                         guint         prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  GisDriver *driver = GIS_DRIVER (object);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    case PROP_USERNAME:
      g_value_set_string (value, priv->username);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_driver_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GisDriver *driver = GIS_DRIVER (object);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  switch (prop_id)
    {
    case PROP_MODE:
      priv->mode = g_value_get_enum (value);
      break;
    case PROP_USERNAME:
      g_free (priv->username);
      priv->username = g_value_dup_string (value);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_driver_activate (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  G_APPLICATION_CLASS (gis_driver_parent_class)->activate (app);

  gtk_window_present (GTK_WINDOW (priv->main_window));
}

static void
window_realize_cb (GtkWidget *widget, gpointer user_data)
{
  GisDriver *driver = GIS_DRIVER (user_data);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GdkWindow *window;
  GdkWMFunction funcs;

  window = gtk_widget_get_window (GTK_WIDGET (priv->main_window));
  funcs = GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE;

  if (!gis_driver_is_small_screen (driver))
    funcs |= GDK_FUNC_RESIZE | GDK_FUNC_MOVE | GDK_FUNC_MAXIMIZE;

  gdk_window_set_functions (window, funcs);
}

static void
gis_driver_startup (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  G_APPLICATION_CLASS (gis_driver_parent_class)->startup (app);

  priv->main_window = g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                                    "application", app,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "icon-name", "preferences-system",
                                    "deletable", FALSE,
                                    NULL);

  g_signal_connect (priv->main_window,
                    "realize",
                    G_CALLBACK (window_realize_cb),
                    (gpointer)app);

  priv->assistant = g_object_new (GIS_TYPE_ASSISTANT, NULL);
  gtk_container_add (GTK_CONTAINER (priv->main_window), GTK_WIDGET (priv->assistant));

  gtk_widget_show (GTK_WIDGET (priv->assistant));

  gis_driver_set_user_language (driver, setlocale (LC_MESSAGES, NULL));

  prepare_main_window (driver);
  rebuild_pages (driver);
}

static void
gis_driver_init (GisDriver *driver)
{
}

static void
gis_driver_class_init (GisDriverClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gis_driver_get_property;
  gobject_class->set_property = gis_driver_set_property;
  gobject_class->finalize = gis_driver_finalize;
  application_class->startup = gis_driver_startup;
  application_class->activate = gis_driver_activate;
  klass->locale_changed = gis_driver_real_locale_changed;

  signals[REBUILD_PAGES] =
    g_signal_new ("rebuild-pages",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GisDriverClass, rebuild_pages),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[LOCALE_CHANGED] =
    g_signal_new ("locale-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GisDriverClass, locale_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  obj_props[PROP_MODE] =
    g_param_spec_enum ("mode", "", "",
                       GIS_TYPE_DRIVER_MODE,
                       GIS_DRIVER_MODE_EXISTING_USER,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_USERNAME] =
    g_param_spec_string ("username", "", "",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

void
gis_driver_save_data (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  gis_assistant_save_data (priv->assistant);
}

GisDriver *
gis_driver_new (GisDriverMode mode)
{
  return g_object_new (GIS_TYPE_DRIVER,
                       "application-id", "org.gnome.InitialSetup",
                       "mode", mode,
                       NULL);
}
