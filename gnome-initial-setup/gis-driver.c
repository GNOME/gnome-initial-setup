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

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <gdm/gdm-client.h>

#include "cc-common-language.h"
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
  PROP_SMALL_SCREEN,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

struct _GisDriverPrivate {
  GtkWindow *main_window;
  GisAssistant *assistant;

  GdmClient *client;
  GdmGreeter *greeter;
  GdmUserVerifier *user_verifier;

  ActUser *user_account;
  gchar *user_password;

  gchar *lang_id;
  gchar *username;

  GisDriverMode mode;
  UmAccountMode account_mode;
  gboolean small_screen;

  locale_t locale;
};
typedef struct _GisDriverPrivate GisDriverPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GisDriver, gis_driver, GTK_TYPE_APPLICATION)

static void
gis_driver_dispose (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  g_clear_object (&priv->user_verifier);
  g_clear_object (&priv->greeter);
  g_clear_object (&priv->client);

  G_OBJECT_CLASS (gis_driver_parent_class)->dispose (object);
}

static void
gis_driver_finalize (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  g_free (priv->lang_id);
  g_free (priv->username);
  g_free (priv->user_password);

  g_clear_object (&priv->user_account);

  if (priv->locale != (locale_t) 0)
    {
      uselocale (LC_GLOBAL_LOCALE);
      freelocale (priv->locale);
    }

  G_OBJECT_CLASS (gis_driver_parent_class)->finalize (object);
}

static void
assistant_page_changed (GtkScrolledWindow *sw)
{
  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (sw), 0);
}

static void
prepare_main_window (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GtkWidget *child, *sw;

  child = g_object_ref (gtk_bin_get_child (GTK_BIN (priv->main_window)));
  gtk_container_remove (GTK_CONTAINER (priv->main_window), child);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_container_add (GTK_CONTAINER (priv->main_window), sw);
  gtk_container_add (GTK_CONTAINER (sw), child);
  g_object_unref (child);

  g_signal_connect_swapped (priv->assistant,
                            "page-changed",
                            G_CALLBACK (assistant_page_changed),
                            sw);

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

static void
gis_driver_real_locale_changed (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GtkTextDirection direction;

  direction = gtk_get_locale_direction ();
  gtk_widget_set_default_direction (direction);

  rebuild_pages (driver);
  gis_assistant_locale_changed (priv->assistant);
}

static void
gis_driver_locale_changed (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[LOCALE_CHANGED], 0);
}

void
gis_driver_set_user_language (GisDriver *driver, const gchar *lang_id, gboolean update_locale)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  g_free (priv->lang_id);
  priv->lang_id = g_strdup (lang_id);

  cc_common_language_set_current_language (lang_id);

  if (update_locale)
    {
      locale_t locale = newlocale (LC_MESSAGES_MASK, lang_id, (locale_t) 0);
      if (locale == (locale_t) 0)
        {
          g_warning ("Failed to create locale %s: %s", lang_id, g_strerror (errno));
          return;
        }

      uselocale (locale);

      if (priv->locale != (locale_t) 0 && priv->locale != LC_GLOBAL_LOCALE)
        freelocale (priv->locale);
      priv->locale = locale;

      gis_driver_locale_changed (driver);
    }
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
  g_set_object (&priv->user_account, user);
  priv->user_password = g_strdup (password);
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
gis_driver_set_account_mode (GisDriver     *driver,
                             UmAccountMode  mode)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  priv->account_mode = mode;
}

UmAccountMode
gis_driver_get_account_mode (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->account_mode;
}

gboolean
gis_driver_get_gdm_objects (GisDriver        *driver,
                            GdmGreeter      **greeter,
                            GdmUserVerifier **user_verifier)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  if (priv->greeter == NULL || priv->user_verifier == NULL)
    return FALSE;

  *greeter = priv->greeter;
  *user_verifier = priv->user_verifier;

  return TRUE;
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

GisDriverMode
gis_driver_get_mode (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->mode;
}

gboolean
gis_driver_is_small_screen (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->small_screen;
}

static gboolean
monitor_is_small (GdkMonitor *monitor)
{
  GdkRectangle geom;

  if (g_getenv ("GIS_SMALL_SCREEN"))
    return TRUE;

  gdk_monitor_get_geometry (monitor, &geom);
  return geom.height < 800;
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
    case PROP_SMALL_SCREEN:
      g_value_set_boolean (value, priv->small_screen);
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

static gboolean
maximize (gpointer data)
{
  GtkWindow *window = data;

  gtk_window_maximize (window);
  gtk_window_present (window);

  return G_SOURCE_REMOVE;
}

static gboolean
unmaximize (gpointer data)
{
  GtkWindow *window = data;

  gtk_window_unmaximize (window);
  gtk_window_present (window);

  return G_SOURCE_REMOVE;
}

static void
set_small_screen_based_on_primary_monitor (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GdkDisplay *default_display = gdk_display_get_default ();
  GdkMonitor *primary_monitor = gdk_display_get_primary_monitor (default_display);

  priv->small_screen = monitor_is_small (primary_monitor);
}

/* Recompute priv->small_screen based on the monitor where the window is
 * located, if the window is actually realized. If not, recompute it based on
 * the primary monitor of the default display. */
static void
recompute_small_screen (GisDriver *driver) {
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GdkWindow *window;
  GdkDisplay *default_display = gdk_display_get_default ();
  GdkMonitor *active_monitor;
  gboolean old_value = priv->small_screen;

  if (!gtk_widget_get_realized (GTK_WIDGET (priv->main_window)))
    {
      set_small_screen_based_on_primary_monitor (driver);
    }
  else
    {
      window = gtk_widget_get_window (GTK_WIDGET (priv->main_window));
      active_monitor = gdk_display_get_monitor_at_window (default_display, window);
      priv->small_screen = monitor_is_small (active_monitor);
    }

  if (priv->small_screen != old_value)
    g_object_notify (G_OBJECT (driver), "small-screen");
}

static void
update_screen_size (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GdkWindow *window;
  GdkGeometry size_hints;
  GtkWidget *sw;

  recompute_small_screen (driver);

  if (!gtk_widget_get_realized (GTK_WIDGET (priv->main_window)))
    return;

  sw = gtk_bin_get_child (GTK_BIN (priv->main_window));
  window = gtk_widget_get_window (GTK_WIDGET (priv->main_window));

  if (priv->small_screen)
    {
      if (window)
        gdk_window_set_functions (window,
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);

      gtk_window_set_geometry_hints (priv->main_window, NULL, NULL, 0);
      gtk_window_set_resizable (priv->main_window, TRUE);
      gtk_window_set_position (priv->main_window, GTK_WIN_POS_NONE);

      g_idle_add (maximize, priv->main_window);
    }
  else
    {
      if (window)
        gdk_window_set_functions (window,
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE |
                                  GDK_FUNC_RESIZE | GDK_FUNC_MOVE | GDK_FUNC_MAXIMIZE);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_NEVER);

      size_hints.min_width = size_hints.max_width = 1024;
      size_hints.min_height = size_hints.max_height = 768;
      size_hints.win_gravity = GDK_GRAVITY_CENTER;

      gtk_window_set_geometry_hints (priv->main_window,
                                     NULL,
                                     &size_hints,
                                     GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_WIN_GRAVITY);
      gtk_window_set_resizable (priv->main_window, FALSE);
      gtk_window_set_position (priv->main_window, GTK_WIN_POS_CENTER_ALWAYS);

      g_idle_add (unmaximize, priv->main_window);
    }
}

static void
screen_size_changed (GdkScreen *screen, GisDriver *driver)
{
  update_screen_size (driver);
}

static void
window_realize_cb (GtkWidget *widget, gpointer user_data)
{
  update_screen_size (GIS_DRIVER (user_data));
}

static void
connect_to_gdm (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  g_autoptr(GError) error = NULL;

  priv->client = gdm_client_new ();

  priv->greeter = gdm_client_get_greeter_sync (priv->client, NULL, &error);
  if (error == NULL)
    priv->user_verifier = gdm_client_get_user_verifier_sync (priv->client, NULL, &error);

  if (error != NULL) {
    g_warning ("Failed to open connection to GDM: %s", error->message);
    g_clear_object (&priv->user_verifier);
    g_clear_object (&priv->greeter);
    g_clear_object (&priv->client);
  }
}

static void
gis_driver_startup (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  G_APPLICATION_CLASS (gis_driver_parent_class)->startup (app);

  if (priv->mode == GIS_DRIVER_MODE_NEW_USER)
    connect_to_gdm (driver);

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

  gis_driver_set_user_language (driver, setlocale (LC_MESSAGES, NULL), FALSE);

  prepare_main_window (driver);
  rebuild_pages (driver);
}

static void
gis_driver_init (GisDriver *driver)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  set_small_screen_based_on_primary_monitor (driver);

  g_signal_connect (screen, "size-changed",
                    G_CALLBACK (screen_size_changed), driver);
}

static void
gis_driver_class_init (GisDriverClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gis_driver_get_property;
  gobject_class->set_property = gis_driver_set_property;
  gobject_class->dispose = gis_driver_dispose;
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

  obj_props[PROP_SMALL_SCREEN] =
    g_param_spec_boolean ("small-screen", "", "",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

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
