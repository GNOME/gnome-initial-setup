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

#include <adwaita.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "pages/welcome/gis-welcome-page.h"
#include "pages/language/gis-language-page.h"
#include "pages/keyboard/gis-keyboard-page.h"
#include "pages/network/gis-network-page.h"
#include "pages/timezone/gis-timezone-page.h"
#include "pages/privacy/gis-privacy-page.h"
#include "pages/software/gis-software-page.h"
#include "pages/account/gis-account-pages.h"
#include "pages/parental-controls/gis-parental-controls-page.h"
#include "pages/password/gis-password-page.h"
#include "pages/summary/gis-summary-page.h"
#include "pages/install/gis-install-page.h"

#define VENDOR_PAGES_GROUP "pages"
#define VENDOR_SKIP_KEY "skip"
#define VENDOR_NEW_USER_ONLY_KEY "new_user_only"
#define VENDOR_EXISTING_USER_ONLY_KEY "existing_user_only"
#define VENDOR_LIVE_USER_ONLY_KEY "live_user_only"

#define STATE_FILE GIS_WORKING_DIR "/state"

static gboolean force_existing_user_mode;
static gboolean force_live_user_mode;

static GPtrArray *skipped_pages;

typedef GisPage *(*PreparePage) (GisDriver *driver);

typedef struct {
  const gchar *page_id;
  PreparePage prepare_page_func;
  GisDriverMode modes;
} PageData;

#define PAGE(name, modes) { #name, gis_prepare_ ## name ## _page, modes }

static PageData page_table[] = {
  PAGE (welcome, GIS_DRIVER_MODE_NEW_USER | GIS_DRIVER_MODE_EXISTING_USER),
  PAGE (language, GIS_DRIVER_MODE_ALL),
  PAGE (keyboard, GIS_DRIVER_MODE_ALL),
  PAGE (network,  GIS_DRIVER_MODE_ALL),
  PAGE (privacy,  GIS_DRIVER_MODE_NEW_USER | GIS_DRIVER_MODE_EXISTING_USER),
  PAGE (timezone, GIS_DRIVER_MODE_ALL),
  PAGE (software, GIS_DRIVER_MODE_NEW_USER | GIS_DRIVER_MODE_EXISTING_USER),
  /* In live user mode, the account page isn't displayed, it just quietly creates the live user */
  PAGE (account,  GIS_DRIVER_MODE_NEW_USER | GIS_DRIVER_MODE_LIVE_USER),
  PAGE (password, GIS_DRIVER_MODE_NEW_USER),
#ifdef HAVE_PARENTAL_CONTROLS
  PAGE (parental_controls, GIS_DRIVER_MODE_NEW_USER | GIS_DRIVER_MODE_EXISTING_USER),
  PAGE (parent_password, GIS_DRIVER_MODE_NEW_USER | GIS_DRIVER_MODE_EXISTING_USER),
#endif
  PAGE (summary, GIS_DRIVER_MODE_NEW_USER),
  PAGE (install, GIS_DRIVER_MODE_LIVE_USER),
  { NULL },
};

#undef PAGE

static gboolean
should_skip_page (const gchar  *page_id,
                  gchar       **skip_pages)
{
  guint i = 0;

  /* special case welcome. We only want to show it if language
   * is skipped
   */
  if (strcmp (page_id, "welcome") == 0)
    return !should_skip_page ("language", skip_pages);

  /* check through our skip pages list for pages we don't want */
  if (skip_pages) {
    while (skip_pages[i]) {
      if (g_strcmp0 (skip_pages[i], page_id) == 0)
        return TRUE;
      i++;
    }
  }

  return FALSE;
}

static gchar **
pages_to_skip_from_file (GisDriver *driver)
{
  GisDriverMode driver_mode;
  GisDriverMode other_modes;
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  g_auto (GStrv) skip_pages = NULL;
  g_autofree char *mode_group = NULL;
  g_autoptr (GFlagsClass) driver_mode_flags_class = NULL;
  const GFlagsValue *driver_mode_flags = NULL;
  g_autoptr (GError) error = NULL;

  /* This code will read the keyfile containing vendor customization options and
   * look for options under the "pages" group, and supports the following keys:
   *   - skip (optional): list of pages to be skipped always
   *   - new_user_only (optional): list of pages to be skipped for modes other than new_user
   *   - existing_user_only (optional): list of pages to be skipped for modes other than existing_user
   *
   * In addition it will look for options under the "{mode} pages" group where {mode} is the
   * current driver mode for the following keys:
   *   - skip (optional): list of pages to be skipped for the current mode
   *
   * This is how this file might look on a vendor image:
   *
   *   [pages]
   *   skip=timezone
   *
   *   [new_user pages]
   *   skip=language;keyboard
   *
   * Older files might look like so:
   *
   *   [pages]
   *   skip=timezone
   *   existing_user_only=language;keyboard
   */

  skip_pages = gis_driver_conf_get_string_list (driver, VENDOR_PAGES_GROUP,
                                                VENDOR_SKIP_KEY, NULL);
  if (skip_pages != NULL)
    {
      g_strv_builder_addv (builder, (const char **) skip_pages);
      g_clear_pointer (&skip_pages, g_strfreev);
    }

  driver_mode_flags_class = g_type_class_ref (GIS_TYPE_DRIVER_MODE);

  driver_mode = gis_driver_get_mode (driver);
  driver_mode_flags = g_flags_get_first_value (driver_mode_flags_class, driver_mode);

  mode_group = g_strdup_printf ("%s pages", driver_mode_flags->value_nick);
  skip_pages = gis_driver_conf_get_string_list (driver, mode_group,
                                                VENDOR_SKIP_KEY, NULL);
  if (skip_pages != NULL)
    {
      g_strv_builder_addv (builder, (const char **) skip_pages);
      g_clear_pointer (&skip_pages, g_strfreev);
    }

  other_modes = GIS_DRIVER_MODE_ALL & ~driver_mode;
  while (other_modes) {
    const GFlagsValue *other_mode_flags = g_flags_get_first_value (driver_mode_flags_class, other_modes);

    if (other_mode_flags != NULL) {
      g_autofree char *vendor_key = g_strdup_printf ("%s_only", other_mode_flags->value_nick);

      skip_pages = gis_driver_conf_get_string_list (driver, VENDOR_PAGES_GROUP,
                                                    vendor_key, NULL);

      if (skip_pages != NULL)
        {
          g_strv_builder_addv (builder, (const char **) skip_pages);
          g_clear_pointer (&skip_pages, g_strfreev);
        }

      other_modes &= ~other_mode_flags->value;
    }
  }

  /* Also, if this is a system mode, we check if the user already answered questions earlier in
   * a different system mode, and skip those pages too.
   */
  if (driver_mode & GIS_DRIVER_MODE_NEW_USER) {
    g_autoptr(GKeyFile) state = NULL;
    gboolean state_loaded;
    g_autoptr(GKeyFile) anaconda = NULL;
    gboolean anaconda_loaded;

    state = g_key_file_new ();
    state_loaded = g_key_file_load_from_file (state, STATE_FILE, G_KEY_FILE_NONE, &error);

    if (state_loaded) {
      skip_pages = g_key_file_get_string_list (state, VENDOR_PAGES_GROUP, VENDOR_SKIP_KEY, NULL, NULL);

      if (skip_pages != NULL) {
          g_strv_builder_addv (builder, (const char **) skip_pages);
          g_clear_pointer (&skip_pages, g_strfreev);
      }
    }

    anaconda = g_key_file_new ();
    anaconda_loaded = g_key_file_load_from_file (anaconda, "/etc/sysconfig/anaconda", G_KEY_FILE_NONE, NULL);

    if (anaconda_loaded) {
      struct {
        const char *spoke_name;
        const char *page_name;
      } spoke_page_map[] = {
        { "WelcomeLanguageSpoke", "language" },
        { "DatetimeSpoke", "timezone" },
        { "KeyboardSpoke", "keyboard" },
        { NULL, NULL }
      };
      size_t i;

      for (i = 0; spoke_page_map[i].spoke_name != NULL; i++) {
        if (g_key_file_get_boolean (anaconda, spoke_page_map[i].spoke_name, "visited", NULL))
          g_strv_builder_add (builder, spoke_page_map[i].page_name);
      }
    }
  }

  return g_strv_builder_end (builder);
}

static void
destroy_pages_after (GisAssistant *assistant,
                     GisPage      *page)
{
  GList *pages, *l, *next;

  pages = gis_assistant_get_all_pages (assistant);

  for (l = pages; l != NULL; l = l->next)
    if (l->data == page)
      break;

  l = l->next;
  for (; l != NULL; l = next) {
    next = l->next;
    gis_assistant_remove_page (assistant, l->data);
  }
}

static void
destroy_page (gpointer data)
{
  GtkWidget *assistant;
  GisPage *page;

  page = data;
  assistant = gtk_widget_get_ancestor (GTK_WIDGET (page), GIS_TYPE_ASSISTANT);

  if (assistant)
    gis_assistant_remove_page (GIS_ASSISTANT (assistant), page);
}

static void
rebuild_pages_cb (GisDriver *driver)
{
  PageData *page_data;
  GisPage *page;
  GisAssistant *assistant;
  GisPage *current_page;
  gchar **skip_pages;
  GisDriverMode driver_mode;
  gboolean skipped;

  assistant = gis_driver_get_assistant (driver);
  current_page = gis_assistant_get_current_page (assistant);
  page_data = page_table;

  g_ptr_array_free (skipped_pages, TRUE);
  skipped_pages = g_ptr_array_new_with_free_func (destroy_page);

  if (current_page != NULL) {
    destroy_pages_after (assistant, current_page);

    for (page_data = page_table; page_data->page_id != NULL; ++page_data)
      if (g_str_equal (page_data->page_id, GIS_PAGE_GET_CLASS (current_page)->page_id))
        break;

    ++page_data;
  }

  driver_mode = gis_driver_get_mode (driver);
  skip_pages = pages_to_skip_from_file (driver);

  for (; page_data->page_id != NULL; ++page_data) {
    skipped = FALSE;

    if (((page_data->modes & driver_mode) == 0) ||
        (should_skip_page (page_data->page_id, skip_pages)))
      skipped = TRUE;

    page = page_data->prepare_page_func (driver);
    if (!page)
      continue;

    if (skipped) {
      gis_page_skip (page);
      g_ptr_array_add (skipped_pages, page);
    } else {
      gis_driver_add_page (driver, page);
    }
  }

  g_strfreev (skip_pages);
}

static GisDriverMode
get_mode (void)
{
  if (force_existing_user_mode)
    return GIS_DRIVER_MODE_EXISTING_USER;
  else if (force_live_user_mode)
    return GIS_DRIVER_MODE_LIVE_USER;
  else
    return GIS_DRIVER_MODE_NEW_USER;
}

static gboolean
initial_setup_disabled_by_anaconda (void)
{
  const gchar *file_name = SYSCONFDIR "/sysconfig/anaconda";
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (key_file, file_name, G_KEY_FILE_NONE, &error)) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
        !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_NOT_FOUND)) {
      g_warning ("Could not read %s: %s", file_name, error->message);
    }
    return FALSE;
  }

  return g_key_file_get_boolean (key_file, "General", "post_install_tools_disabled", NULL);
}

int
main (int argc, char *argv[])
{
  GisDriver *driver;
  int status;
  GOptionContext *context;
  GisDriverMode mode;

  GOptionEntry entries[] = {
    { "existing-user", 0, 0, G_OPTION_ARG_NONE, &force_existing_user_mode,
      _("Force existing user mode"), NULL },
    { "live-user", 0, 0, G_OPTION_ARG_NONE, &force_live_user_mode,
      _("Force live user mode"), NULL },
    { NULL }
  };

  g_unsetenv ("GIO_USE_VFS");

  /* By default, libadwaita reads settings from the Settings portal, which causes
   * the portal to be started, which causes gnome-keyring to be started. This
   * interferes with our attempt below to manually start gnome-keyring and set
   * the login keyring password to a well-known value, which we overwrite with
   * the user's password once they choose one.
   */
  g_setenv ("ADW_DISABLE_PORTAL", "1", TRUE);

  context = g_option_context_new (_("— GNOME initial setup"));
  g_option_context_add_main_entries (context, entries, NULL);

  g_option_context_parse (context, &argc, &argv, NULL);

  if (gis_kernel_command_line_has_argument ((const char *[]) { "rd.live.image", "endless.live_boot", NULL }))
    force_live_user_mode = TRUE;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_message ("Starting gnome-initial-setup");
  if (gis_get_mock_mode ())
    g_message ("Mock mode: changes will not be saved to disk");
  else
    g_message ("Production mode: changes will be saved to disk");

  skipped_pages = g_ptr_array_new_with_free_func (destroy_page);
  mode = get_mode ();

  /* When we are running as the gnome-initial-setup user we
   * dont have a normal user session and need to initialize
   * the keyring manually so that we can pass the credentials
   * along to the new user in the handoff.
   */
  if ((mode & GIS_DRIVER_MODE_SYSTEM) && !gis_get_mock_mode ())
    gis_ensure_login_keyring ();

  driver = gis_driver_new (mode);
  adw_style_manager_set_color_scheme (adw_style_manager_get_default (),
                                      ADW_COLOR_SCHEME_PREFER_LIGHT);

  /* On first login, GNOME Shell offers to run a tour. If we also run Initial
   * Setup, the two immovable, centred windows will sit atop one another.
   * Until we have the ability to run Initial Setup in the "kiosk" mode, like
   * it does in new-user mode, disable Initial Setup for existing users.
   *
   * https://gitlab.gnome.org/GNOME/gnome-initial-setup/-/issues/120#note_1019004
   * https://gitlab.gnome.org/GNOME/gnome-initial-setup/-/issues/12
   */
  if (mode == GIS_DRIVER_MODE_EXISTING_USER) {
    g_message ("Skipping gnome-initial-setup for existing user");
    gis_ensure_stamp_files (driver);
    exit (EXIT_SUCCESS);
  }

  /* We only do this in existing-user mode, because if gdm launches us
   * in new-user mode and we just exit, gdm's special g-i-s session
   * never terminates. */
  if (initial_setup_disabled_by_anaconda () &&
      mode == GIS_DRIVER_MODE_EXISTING_USER) {
    gis_ensure_stamp_files (driver);
    exit (EXIT_SUCCESS);
  }

  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);

  g_ptr_array_free (skipped_pages, TRUE);

  g_object_unref (driver);
  g_option_context_free (context);
  return status;
}

static void
write_state (GisDriver *driver)
{
  g_autoptr(GKeyFile) state = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_auto(GStrv) visited_pages = NULL;
  GisAssistant *assistant;
  GList *pages, *node;

  assistant = gis_driver_get_assistant (driver);

  if (assistant == NULL)
    return;

  state = g_key_file_new ();

  builder = g_strv_builder_new ();

  pages = gis_assistant_get_all_pages (assistant);
  for (node = pages; node != NULL; node = node->next) {
    GisPage *page = node->data;
    g_strv_builder_add (builder, GIS_PAGE_GET_CLASS (page)->page_id);
  }

  visited_pages = g_strv_builder_end (builder);

  g_key_file_set_string_list (state,
                              VENDOR_PAGES_GROUP,
                              VENDOR_SKIP_KEY,
                              (const char * const *)
                              visited_pages,
                              g_strv_length (visited_pages));

  if (!g_key_file_save_to_file (state, STATE_FILE, &error)) {
    g_warning ("Unable to save state to %s: %s", STATE_FILE, error->message);
    return;
  }
}

void
gis_ensure_stamp_files (GisDriver *driver)
{
  g_autofree gchar *done_file = NULL;
  g_autoptr(GError) error = NULL;

  done_file = g_build_filename (g_get_user_config_dir (), "gnome-initial-setup-done", NULL);
  if (!g_file_set_contents (done_file, "yes", -1, &error)) {
      g_warning ("Unable to create %s: %s", done_file, error->message);
      g_clear_error (&error);
  }

  write_state (driver);
}

/**
 * gis_get_mock_mode:
 *
 * Gets whether gnome-initial-setup has been built for development, and hence
 * shouldn’t permanently change any system configuration.
 *
 * By default, mock mode is enabled when running in a build environment. This
 * heuristic may be changed in future.
 *
 * Returns: %TRUE if in mock mode, %FALSE otherwise
 */
gboolean
gis_get_mock_mode (void)
{
  return (g_getenv ("UNDER_JHBUILD") != NULL);
}
