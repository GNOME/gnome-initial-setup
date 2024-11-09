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
#include <hb-glib.h>
#include <harfbuzz/hb.h>
#include <pango/pango.h>

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

typedef enum {
  PROP_MODE = 1,
  PROP_USERNAME,
  PROP_SMALL_SCREEN,
  PROP_PARENTAL_CONTROLS_ENABLED,
  PROP_FULL_NAME,
  PROP_AVATAR,
  PROP_HAS_DEFAULT_AVATAR,
} GisDriverProperty;

static GParamSpec *obj_props[PROP_HAS_DEFAULT_AVATAR + 1];

struct _GisDriver {
  AdwApplication  parent_instance;

  GtkWindow *main_window;
  GisAssistant *assistant;

  GdmClient *client;
  GdmGreeter *greeter;
  GdmUserVerifier *user_verifier;

  ActUser *user_account;
  gchar *user_password;

  ActUser *parent_account;  /* (owned) (nullable) */
  gchar *parent_password;  /* (owned) (nullable) */

  gboolean parental_controls_enabled;

  gchar *lang_id;
  gchar *username;
  gchar *full_name;  /* (owned) (nullable) */

  GdkTexture *avatar;  /* (owned) (nullable) */
  gboolean has_default_avatar;

  GisDriverMode mode;
  UmAccountMode account_mode;
  gboolean small_screen;

  locale_t locale;

  const gchar *vendor_conf_file_path;
  GKeyFile *vendor_conf_file;
};

G_DEFINE_TYPE (GisDriver, gis_driver, ADW_TYPE_APPLICATION)

static void
gis_driver_dispose (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);

  g_clear_object (&driver->user_verifier);
  g_clear_object (&driver->greeter);
  g_clear_object (&driver->client);

  G_OBJECT_CLASS (gis_driver_parent_class)->dispose (object);
}

static void
gis_driver_finalize (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);

  g_free (driver->lang_id);
  g_free (driver->username);
  g_free (driver->full_name);
  g_free (driver->user_password);

  g_clear_object (&driver->avatar);

  g_clear_object (&driver->user_account);
  g_clear_pointer (&driver->vendor_conf_file, g_key_file_free);

  g_clear_object (&driver->parent_account);
  g_free (driver->parent_password);

  if (driver->locale != (locale_t) 0)
    {
      uselocale (LC_GLOBAL_LOCALE);
      freelocale (driver->locale);
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
  GtkWidget *child, *sw;

  child = gtk_window_get_child (GTK_WINDOW (driver->main_window));
  g_object_ref (child);
  gtk_window_set_child (GTK_WINDOW (driver->main_window), NULL);
  sw = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (driver->main_window), sw);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), child);
  g_object_unref (child);

  g_signal_connect_swapped (driver->assistant,
                            "page-changed",
                            G_CALLBACK (assistant_page_changed),
                            sw);

  gtk_window_set_titlebar (driver->main_window,
                           gis_assistant_get_titlebar (driver->assistant));
}

static void
rebuild_pages (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[REBUILD_PAGES], 0);
}

GisAssistant *
gis_driver_get_assistant (GisDriver *driver)
{
  return driver->assistant;
}

static GtkTextDirection
get_direction_from_lang_id (const char * lang_id)
{
  /* Manually parse the direction as the locale was only set in this thread and
   * `gtk_get_locale_direction()` will not work. */
  int n_scripts;
  GtkTextDirection direction = GTK_TEXT_DIR_LTR;
  PangoLanguage *lang = pango_language_from_string (lang_id);
  const PangoScript *scripts = pango_language_get_scripts (lang, &n_scripts);

  if (n_scripts > 0)
    {
      hb_script_t script = hb_glib_script_to_script ((GUnicodeScript) scripts[0]);

      direction = (hb_script_get_horizontal_direction (script) == HB_DIRECTION_RTL ?
                   GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);

    }

  return direction;
}

static void
gis_driver_locale_changed (GisDriver *driver)
{
  GtkTextDirection direction;

  direction = get_direction_from_lang_id (driver->lang_id);
  gtk_widget_set_default_direction (direction);

  rebuild_pages (driver);
  gis_assistant_locale_changed (driver->assistant);

  g_signal_emit (G_OBJECT (driver), signals[LOCALE_CHANGED], 0);
}

void
gis_driver_set_user_language (GisDriver *driver, const gchar *lang_id, gboolean update_locale)
{
  g_free (driver->lang_id);
  driver->lang_id = g_strdup (lang_id);

  cc_common_language_set_current_language (lang_id);

  if (update_locale)
    {
      locale_t locale = newlocale (LC_ALL_MASK, lang_id, (locale_t) 0);
      if (locale == (locale_t) 0)
        {
          g_warning ("Failed to create locale %s: %s", lang_id, g_strerror (errno));
          return;
        }

      uselocale (locale);

      if (driver->locale != (locale_t) 0 && driver->locale != LC_GLOBAL_LOCALE)
        freelocale (driver->locale);
      driver->locale = locale;

      gis_driver_locale_changed (driver);
    }
}

const gchar *
gis_driver_get_user_language (GisDriver *driver)
{
  return driver->lang_id;
}

void
gis_driver_set_username (GisDriver *driver, const gchar *username)
{
  g_free (driver->username);
  driver->username = g_strdup (username);

  g_object_notify (G_OBJECT (driver), "username");
}

const gchar *
gis_driver_get_username (GisDriver *driver)
{
  return driver->username;
}

/**
 * gis_driver_set_full_name:
 * @driver: a #GisDriver
 * @full_name: (nullable): full name of the main user, or %NULL if not known
 *
 * Set the #GisDriver:full-name property.
 *
 * Since: 3.36
 */
void
gis_driver_set_full_name (GisDriver   *driver,
                          const gchar *full_name)
{
  g_return_if_fail (GIS_IS_DRIVER (driver));
  g_return_if_fail (full_name == NULL ||
                    g_utf8_validate (full_name, -1, NULL));

  if (g_strcmp0 (driver->full_name, full_name) == 0)
    return;

  g_free (driver->full_name);
  driver->full_name = g_strdup (full_name);

  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_FULL_NAME]);
}

/**
 * gis_driver_get_full_name:
 * @driver: a #GisDriver
 *
 * Get the #GisDriver:full-name property.
 *
 * Returns: (nullable): full name of the main user, or %NULL if not known
 * Since: 3.36
 */
const gchar *
gis_driver_get_full_name (GisDriver *driver)
{
  g_return_val_if_fail (GIS_IS_DRIVER (driver), NULL);

  return driver->full_name;
}

/**
 * gis_driver_set_avatar:
 * @driver: a #GisDriver
 * @avatar: (nullable) (transfer none): avatar of the main user, or %NULL if not known
 *
 * Set the #GisDriver:avatar property.
 *
 * Since: 3.36
 */
void
gis_driver_set_avatar (GisDriver  *driver,
                       GdkTexture *avatar)
{
  g_return_if_fail (GIS_IS_DRIVER (driver));
  g_return_if_fail (avatar == NULL || GDK_IS_TEXTURE (avatar));

  if (g_set_object (&driver->avatar, avatar))
    g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_AVATAR]);
}

/**
 * gis_driver_get_avatar:
 * @driver: a #GisDriver
 *
 * Get the #GisDriver:avatar property.
 *
 * Returns: (nullable) (transfer none): avatar of the main user, or %NULL if not known
 * Since: 3.36
 */
GdkTexture *
gis_driver_get_avatar (GisDriver *driver)
{
  g_return_val_if_fail (GIS_IS_DRIVER (driver), NULL);

  return driver->avatar;
}

/**
 * gis_driver_set_has_default_avatar:
 * @driver: a #GisDriver
 * @has_default_avatar: whether the generated user avatar should be used
 *
 * Set the #GisDriver:has-default-avatar property.
 *
 * Since: 46
 */
void
gis_driver_set_has_default_avatar (GisDriver *driver,
                                   gboolean   has_default_avatar)
{
  if (driver->has_default_avatar == has_default_avatar)
    return;

  driver->has_default_avatar = has_default_avatar;

  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_HAS_DEFAULT_AVATAR]);
}

/**
 * gis_driver_get_has_default_avatar:
 * @driver: a #GisDriver
 *
 * Get the #GisDriver:has-default-avatar property.
 *
 * Returns: whether the generated user avatar should be used
 * Since: 46
 */
gboolean
gis_driver_get_has_default_avatar (GisDriver *driver)
{
  return driver->has_default_avatar;
}

void
gis_driver_set_user_permissions (GisDriver   *driver,
                                 ActUser     *user,
                                 const gchar *password)
{
  g_set_object (&driver->user_account, user);
  g_free (driver->user_password);
  driver->user_password = g_strdup (password);
}

void
gis_driver_get_user_permissions (GisDriver    *driver,
                                 ActUser     **user,
                                 const gchar **password)
{
  if (user != NULL)
    *user = driver->user_account;

  if (password != NULL)
    *password = driver->user_password;
}

/**
 * gis_driver_set_parent_permissions:
 * @driver: a #GisDriver
 * @parent: (transfer none): user account for the parent
 * @password: password for the parent
 *
 * Stores the parent account details for later use when saving the initial setup
 * data.
 *
 * Since: 3.36
 */
void
gis_driver_set_parent_permissions (GisDriver   *driver,
                                   ActUser     *parent,
                                   const gchar *password)
{
  g_set_object (&driver->parent_account, parent);
  g_free (driver->parent_password);
  driver->parent_password = g_strdup (password);
}

/**
 * gis_driver_get_parent_permissions:
 * @driver: a #GisDriver
 * @parent: (out) (transfer none) (optional) (nullable): return location for the
 *    user account for the parent, which may be %NULL
 * @password: (out) (transfer none) (optional) (nullable): return location for
 *    the password for the parent
 *
 * Gets the parent account details saved from an earlier step in the initial
 * setup process. They may be %NULL if not set yet.
 *
 * Since: 3.36
 */
void
gis_driver_get_parent_permissions (GisDriver    *driver,
                                   ActUser     **parent,
                                   const gchar **password)
{
  if (parent != NULL)
    *parent = driver->parent_account;
  if (password != NULL)
    *password = driver->parent_password;
}

void
gis_driver_set_account_mode (GisDriver     *driver,
                             UmAccountMode  mode)
{
  driver->account_mode = mode;
}

UmAccountMode
gis_driver_get_account_mode (GisDriver *driver)
{
  return driver->account_mode;
}

/**
 * gis_driver_set_parental_controls_enabled:
 * @driver: a #GisDriver
 * @parental_controls_enabled: whether parental controls are enabled for the main user
 *
 * Set the #GisDriver:parental-controls-enabled property.
 *
 * Since: 3.36
 */
void
gis_driver_set_parental_controls_enabled (GisDriver *driver,
                                          gboolean   parental_controls_enabled)
{
  if (driver->parental_controls_enabled == parental_controls_enabled)
    return;

  driver->parental_controls_enabled = parental_controls_enabled;
  rebuild_pages (driver);

  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_PARENTAL_CONTROLS_ENABLED]);
}

/**
 * gis_driver_get_parental_controls_enabled:
 * @driver: a #GisDriver
 *
 * Get the #GisDriver:parental-controls-enabled property.
 *
 * Returns: whether parental controls are enabled for the main user
 * Since: 3.36
 */
gboolean
gis_driver_get_parental_controls_enabled (GisDriver *driver)
{
  return driver->parental_controls_enabled;
}

gboolean
gis_driver_get_gdm_objects (GisDriver        *driver,
                            GdmGreeter      **greeter,
                            GdmUserVerifier **user_verifier)
{
  if (driver->greeter == NULL || driver->user_verifier == NULL)
    return FALSE;

  *greeter = driver->greeter;
  *user_verifier = driver->user_verifier;

  return TRUE;
}

void
gis_driver_add_page (GisDriver *driver,
                     GisPage   *page)
{
  gis_assistant_add_page (driver->assistant, page);
}

void
gis_driver_hide_window (GisDriver *driver)
{
  gtk_widget_set_visible (GTK_WIDGET (driver->main_window), FALSE);
}

static gboolean
load_vendor_conf_file_at_path (GisDriver *driver,
                               const char *path)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) vendor_conf_file = g_key_file_new ();

  if (!g_key_file_load_from_file (vendor_conf_file, path, G_KEY_FILE_NONE, &error))
    {
      if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning ("Could not read file %s: %s:", path, error->message);
      return FALSE;
    }

  driver->vendor_conf_file_path = path;
  driver->vendor_conf_file = g_steal_pointer (&vendor_conf_file);
  return TRUE;
}

static void
load_vendor_conf_file (GisDriver *driver)
{
#ifdef VENDOR_CONF_FILE
  load_vendor_conf_file_at_path (driver, VENDOR_CONF_FILE);
#else
  /* If no path was passed at build time, then we have search path:
   *
   *  - First check $(sysconfdir)/gnome-initial-setup/vendor.conf
   *  - Then check $(datadir)/gnome-initial-setup/vendor.conf
   *
   * This allows distributions to provide a default packaged config in a
   * location that might be managed by ostree, and allows OEMs to
   * override using an unmanaged location.
   */
  if (!load_vendor_conf_file_at_path (driver, PKGSYSCONFDIR "/vendor.conf"))
    load_vendor_conf_file_at_path (driver, PKGDATADIR "/vendor.conf");
#endif
}

static void
report_conf_error_if_needed (GisDriver *driver,
                             const gchar *group,
                             const gchar *key,
                             const GError *error)
{
  if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) &&
      !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND))
    g_warning ("Error getting the value for key '%s' of group [%s] in %s: %s",
               group, key, driver->vendor_conf_file_path, error->message);
}

gboolean
gis_driver_conf_get_boolean (GisDriver *driver,
                             const gchar *group,
                             const gchar *key,
                             gboolean default_value)
{
  if (driver->vendor_conf_file) {
    g_autoptr(GError) error = NULL;
    gboolean new_value = g_key_file_get_boolean (driver->vendor_conf_file, group,
                                                 key, &error);
    if (error == NULL)
      return new_value;

    report_conf_error_if_needed (driver, group, key, error);
  }

  return default_value;
}

GStrv
gis_driver_conf_get_string_list (GisDriver *driver,
                                 const gchar *group,
                                 const gchar *key,
                                 gsize *out_length)
{
  if (driver->vendor_conf_file) {
    g_autoptr(GError) error = NULL;
    GStrv new_value = g_key_file_get_string_list (driver->vendor_conf_file, group,
                                                  key, out_length, &error);
    if (error == NULL)
      return new_value;

    report_conf_error_if_needed (driver, group, key, error);
  }

  return NULL;
}

gchar *
gis_driver_conf_get_string (GisDriver *driver,
                            const gchar *group,
                            const gchar *key)
{
  if (driver->vendor_conf_file) {
    g_autoptr(GError) error = NULL;
    gchar *new_value = g_key_file_get_string (driver->vendor_conf_file, group,
                                              key, &error);
    if (error == NULL)
      return new_value;

    report_conf_error_if_needed (driver, group, key, error);
  }

  return NULL;
}

GisDriverMode
gis_driver_get_mode (GisDriver *driver)
{
  return driver->mode;
}

gboolean
gis_driver_is_small_screen (GisDriver *driver)
{
  return driver->small_screen;
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

  switch ((GisDriverProperty) prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, driver->mode);
      break;
    case PROP_USERNAME:
      g_value_set_string (value, driver->username);
      break;
    case PROP_SMALL_SCREEN:
      g_value_set_boolean (value, driver->small_screen);
      break;
    case PROP_PARENTAL_CONTROLS_ENABLED:
      g_value_set_boolean (value, driver->parental_controls_enabled);
      break;
    case PROP_FULL_NAME:
      g_value_set_string (value, driver->full_name);
      break;
    case PROP_AVATAR:
      g_value_set_object (value, driver->avatar);
      break;
    case PROP_HAS_DEFAULT_AVATAR:
      g_value_set_boolean (value, driver->has_default_avatar);
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

  switch ((GisDriverProperty) prop_id)
    {
    case PROP_MODE:
      driver->mode = g_value_get_enum (value);
      break;
    case PROP_USERNAME:
      g_free (driver->username);
      driver->username = g_value_dup_string (value);
      break;
    case PROP_PARENTAL_CONTROLS_ENABLED:
      gis_driver_set_parental_controls_enabled (driver, g_value_get_boolean (value));
      break;
    case PROP_FULL_NAME:
      gis_driver_set_full_name (driver, g_value_get_string (value));
      break;
    case PROP_AVATAR:
      gis_driver_set_avatar (driver, g_value_get_object (value));
      break;
    case PROP_HAS_DEFAULT_AVATAR:
      gis_driver_set_has_default_avatar (driver, g_value_get_boolean (value));
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

  G_APPLICATION_CLASS (gis_driver_parent_class)->activate (app);

  gtk_window_present (GTK_WINDOW (driver->main_window));
}

/* Recompute driver->small_screen based on the monitor where the window is
 * located, if the window is actually realized. If not, recompute it based on
 * the primary monitor of the default display. */
static void
recompute_small_screen (GisDriver *driver)
{
  GdkMonitor *active_monitor;
  gboolean old_value = driver->small_screen;

  if (gtk_widget_get_realized (GTK_WIDGET (driver->main_window)))
    {
      GdkDisplay *default_display = gdk_display_get_default ();
      GdkSurface *surface;

      surface = gtk_native_get_surface (GTK_NATIVE (driver->main_window));
      active_monitor = gdk_display_get_monitor_at_surface (default_display, surface);
      driver->small_screen = monitor_is_small (active_monitor);
    }

  if (driver->small_screen != old_value)
    g_object_notify (G_OBJECT (driver), "small-screen");
}

static void
update_screen_size (GisDriver *driver)
{
  GtkWidget *sw;

  recompute_small_screen (driver);

  if (!gtk_widget_get_realized (GTK_WIDGET (driver->main_window)))
    return;

  sw = gtk_window_get_child (GTK_WINDOW (driver->main_window));

  if (driver->small_screen)
    {
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_window_set_default_size (driver->main_window, 800, 600);
      gtk_window_set_resizable (driver->main_window, TRUE);
      gtk_window_maximize (driver->main_window);
      gtk_window_present (driver->main_window);
    }
  else
    {
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_NEVER);
      gtk_window_set_default_size (driver->main_window, 1024, 768);
      gtk_window_set_resizable (driver->main_window, FALSE);
      gtk_window_unmaximize (driver->main_window);
      gtk_window_present (driver->main_window);
    }
}

static void
on_surface_enter_monitor_cb (GdkSurface *surface,
                             GdkMonitor *monitor,
                             GisDriver  *driver)
{
  update_screen_size (driver);
}

static void
window_realize_cb (GtkWidget *widget, gpointer user_data)
{
  GdkSurface *surface;
  GisDriver *driver;

  driver = GIS_DRIVER (user_data);

  surface = gtk_native_get_surface (GTK_NATIVE (widget));
  g_signal_connect (surface, "enter-monitor", G_CALLBACK (on_surface_enter_monitor_cb), driver);

  update_screen_size (driver);
}

static bool
window_close_request_cb (GtkWidget *widget, gpointer user_data)
{
  return true;
}

static void
connect_to_gdm (GisDriver *driver)
{
  g_autoptr(GError) error = NULL;

  driver->client = gdm_client_new ();

  driver->greeter = gdm_client_get_greeter_sync (driver->client, NULL, &error);
  if (error == NULL)
    driver->user_verifier = gdm_client_get_user_verifier_sync (driver->client, NULL, &error);

  if (error != NULL) {
    g_warning ("Failed to open connection to GDM: %s", error->message);
    g_clear_object (&driver->user_verifier);
    g_clear_object (&driver->greeter);
    g_clear_object (&driver->client);
  }
}

static void
gis_driver_startup (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);

  G_APPLICATION_CLASS (gis_driver_parent_class)->startup (app);

  adw_style_manager_set_color_scheme (adw_style_manager_get_default (),
                                      ADW_COLOR_SCHEME_PREFER_LIGHT);

  if (driver->mode == GIS_DRIVER_MODE_NEW_USER)
    connect_to_gdm (driver);

  driver->main_window = g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                                    "application", app,
                                    "icon-name", "preferences-system",
                                    "deletable", FALSE,
                                    "title", _("Initial Setup"),
                                    NULL);

  g_signal_connect (driver->main_window,
                    "realize",
                    G_CALLBACK (window_realize_cb),
                    (gpointer)app);

  /* Only allow closing the window in existing user mode*/
  if (driver->mode != GIS_DRIVER_MODE_EXISTING_USER) {
    g_signal_connect (driver->main_window,
                      "close-request",
                      G_CALLBACK (window_close_request_cb),
                      NULL);
  }

  driver->assistant = g_object_new (GIS_TYPE_ASSISTANT, NULL);
  gtk_window_set_child (GTK_WINDOW (driver->main_window),
                        GTK_WIDGET (driver->assistant));

  gis_driver_set_user_language (driver, setlocale (LC_MESSAGES, NULL), FALSE);

  prepare_main_window (driver);
  rebuild_pages (driver);
}

static void
gis_driver_init (GisDriver *driver)
{
  load_vendor_conf_file (driver);
  driver->has_default_avatar = FALSE;
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

  signals[REBUILD_PAGES] =
    g_signal_new ("rebuild-pages",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[LOCALE_CHANGED] =
    g_signal_new ("locale-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
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

  /**
   * GisDriver:parental-controls-enabled:
   *
   * Whether parental controls are enabled for the main user. If this is %TRUE,
   * two user accounts will be created when this page is saved: one for the main
   * user (a child) which will be a standard account; and one for the parent
   * which will be an administrative account.
   *
   * Since: 3.36
   */
  obj_props[PROP_PARENTAL_CONTROLS_ENABLED] =
    g_param_spec_boolean ("parental-controls-enabled",
                          "Parental Controls Enabled",
                          "Whether parental controls are enabled for the main user.",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GisDriver:full-name: (nullable)
   *
   * Full name of the main user. May be %NULL if unknown or not set yet.
   *
   * Since: 3.36
   */
  obj_props[PROP_FULL_NAME] =
    g_param_spec_string ("full-name",
                         "Full Name",
                         "Full name of the main user.",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GisDriver:avatar: (nullable)
   *
   * Avatar of the main user. May be %NULL if unknown or not set yet.
   *
   * Since: 3.36
   */
  obj_props[PROP_AVATAR] =
    g_param_spec_object ("avatar",
                         "Avatar",
                         "Avatar of the main user.",
                         GDK_TYPE_TEXTURE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

 /**
   * GisDriver:has-default-avatar:
   *
   * Whether the generated user avatar should be used, If this is %TRUE then,
   * the default generated avatar should be used, instead of the #GdkTexture
   * returned by #GisDriver:avatar property.
   *
   * Since: 46
   */
  obj_props[PROP_HAS_DEFAULT_AVATAR] =
    g_param_spec_boolean ("has-default-avatar",
                          "Has Default Avatar",
                          "Whether the generated user avatar should be used",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, G_N_ELEMENTS (obj_props), obj_props);
}

gboolean
gis_driver_save_data (GisDriver  *driver,
                      GError    **error)
{
  if (gis_get_mock_mode ())
    {
      g_message ("%s: Skipping saving data due to being in mock mode", G_STRFUNC);
      return TRUE;
    }

  return gis_assistant_save_data (driver->assistant, error);
}

GisDriver *
gis_driver_new (GisDriverMode mode)
{
  return g_object_new (GIS_TYPE_DRIVER,
                       "application-id", "org.gnome.InitialSetup",
                       "mode", mode,
                       NULL);
}
