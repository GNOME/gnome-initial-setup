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

#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#include "pages/language/gis-language-page.h"
#include "pages/region/gis-region-page.h"
#include "pages/keyboard/gis-keyboard-page.h"
#include "pages/eulas/gis-eula-pages.h"
#include "pages/network/gis-network-page.h"
#include "pages/timezone/gis-timezone-page.h"
#include "pages/privacy/gis-privacy-page.h"
#include "pages/software/gis-software-page.h"
#include "pages/goa/gis-goa-page.h"
#include "pages/account/gis-account-pages.h"
#include "pages/password/gis-password-page.h"
#include "pages/summary/gis-summary-page.h"

#define VENDOR_PAGES_GROUP "pages"
#define VENDOR_SKIP_KEY "skip"
#define VENDOR_NEW_USER_ONLY_KEY "new_user_only"
#define VENDOR_EXISTING_USER_ONLY_KEY "existing_user_only"

static gboolean force_existing_user_mode;

static GPtrArray *skipped_pages;

typedef GisPage *(*PreparePage) (GisDriver *driver);

typedef struct {
  const gchar *page_id;
  PreparePage prepare_page_func;
  gboolean new_user_only;
} PageData;

#define PAGE(name, new_user_only) { #name, gis_prepare_ ## name ## _page, new_user_only }

static PageData page_table[] = {
  PAGE (language, FALSE),
#ifdef ENABLE_REGION_PAGE
  PAGE (region,   FALSE),
#endif /* ENABLE_REGION_PAGE */
  PAGE (keyboard, FALSE),
  PAGE (eula,     FALSE),
  PAGE (network,  FALSE),
  PAGE (privacy,  FALSE),
  PAGE (timezone, TRUE),
  PAGE (software, TRUE),
  PAGE (goa,      FALSE),
  PAGE (account,  TRUE),
  PAGE (password, TRUE),
  PAGE (summary,  FALSE),
  { NULL },
};

#undef PAGE

static gboolean
should_skip_page (const gchar  *page_id,
                  gchar       **skip_pages)
{
  guint i = 0;

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
strv_append (gchar **a,
             gchar **b)
{
  guint n = g_strv_length (a);
  guint m = g_strv_length (b);

  a = g_renew (gchar *, a, n + m + 1);
  for (guint i = 0; i < m; i++)
    a[n + i] = g_strdup (b[i]);
  a[n + m] = NULL;

  return a;
}

static gchar **
pages_to_skip_from_file (gboolean is_new_user)
{
  GKeyFile *skip_pages_file;
  gchar **skip_pages = NULL;
  gchar **additional_skip_pages = NULL;
  GError *error = NULL;

  /* VENDOR_CONF_FILE points to a keyfile containing vendor customization
   * options. This code will look for options under the "pages" group, and
   * supports the following keys:
   *   - skip (optional): list of pages to be skipped always
   *   - new_user_only (optional): list of pages to be skipped in existing user mode
   *   - existing_user_only (optional): list of pages to be skipped in new user mode
   *
   * This is how this file might look on a vendor image:
   *
   *   [pages]
   *   skip=timezone
   *   existing_user_only=language;keyboard
   */
  skip_pages_file = g_key_file_new ();
  if (!g_key_file_load_from_file (skip_pages_file, VENDOR_CONF_FILE,
                                  G_KEY_FILE_NONE, &error)) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      g_warning ("Could not read file %s: %s", VENDOR_CONF_FILE, error->message);

    g_error_free (error);
    goto out;
  }

  skip_pages = g_key_file_get_string_list (skip_pages_file,
                                           VENDOR_PAGES_GROUP,
                                           VENDOR_SKIP_KEY, NULL, NULL);
  additional_skip_pages = g_key_file_get_string_list (skip_pages_file,
                                                      VENDOR_PAGES_GROUP,
                                                      is_new_user ? VENDOR_EXISTING_USER_ONLY_KEY : VENDOR_NEW_USER_ONLY_KEY,
                                                      NULL, NULL);

  if (!skip_pages && additional_skip_pages) {
    skip_pages = additional_skip_pages;
  } else if (skip_pages && additional_skip_pages) {
    skip_pages = strv_append (skip_pages, additional_skip_pages);
    g_strfreev (additional_skip_pages);
  }

 out:
  g_key_file_free (skip_pages_file);

  return skip_pages;
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
    gtk_widget_destroy (GTK_WIDGET (l->data));
  }
}

static void
rebuild_pages_cb (GisDriver *driver)
{
  PageData *page_data;
  GisPage *page;
  GisAssistant *assistant;
  GisPage *current_page;
  gchar **skip_pages;
  gboolean is_new_user, skipped;

  assistant = gis_driver_get_assistant (driver);
  current_page = gis_assistant_get_current_page (assistant);
  page_data = page_table;

  g_ptr_array_free (skipped_pages, TRUE);
  skipped_pages = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_widget_destroy);

  if (current_page != NULL) {
    destroy_pages_after (assistant, current_page);

    for (page_data = page_table; page_data->page_id != NULL; ++page_data)
      if (g_str_equal (page_data->page_id, GIS_PAGE_GET_CLASS (current_page)->page_id))
        break;

    ++page_data;
  }

  is_new_user = (gis_driver_get_mode (driver) == GIS_DRIVER_MODE_NEW_USER);
  skip_pages = pages_to_skip_from_file (is_new_user);

  for (; page_data->page_id != NULL; ++page_data) {
    skipped = FALSE;

    if ((page_data->new_user_only && !is_new_user) ||
        (should_skip_page (page_data->page_id, skip_pages)))
      skipped = TRUE;

    page = page_data->prepare_page_func (driver);
    if (!page)
      continue;

    if (skipped && gis_page_skip (page))
      g_ptr_array_add (skipped_pages, page);
    else
      gis_driver_add_page (driver, page);
  }

  g_strfreev (skip_pages);
}

static GisDriverMode
get_mode (void)
{
  if (force_existing_user_mode)
    return GIS_DRIVER_MODE_EXISTING_USER;
  else
    return GIS_DRIVER_MODE_NEW_USER;
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
    { NULL }
  };

  g_unsetenv ("GIO_USE_VFS");

  context = g_option_context_new (_("— GNOME initial setup"));
  g_option_context_add_main_entries (context, entries, NULL);

  g_option_context_parse (context, &argc, &argv, NULL);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

#ifdef HAVE_CHEESE
  cheese_gtk_init (NULL, NULL);
#endif

  gtk_init (&argc, &argv);

  skipped_pages = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_widget_destroy);
  mode = get_mode ();

  /* When we are running as the gnome-initial-setup user we
   * dont have a normal user session and need to initialize
   * the keyring manually so that we can pass the credentials
   * along to the new user in the handoff.
   */
  if (mode == GIS_DRIVER_MODE_NEW_USER)
    gis_ensure_login_keyring ();

  driver = gis_driver_new (mode);
  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);

  g_ptr_array_free (skipped_pages, TRUE);

  g_object_unref (driver);
  g_option_context_free (context);
  return status;
}

void
gis_ensure_stamp_files (void)
{
  gchar *file;
  GError *error = NULL;

  file = g_build_filename (g_get_user_config_dir (), "run-welcome-tour", NULL);
  if (!g_file_set_contents (file, "yes", -1, &error)) {
      g_warning ("Unable to create %s: %s", file, error->message);
      g_clear_error (&error);
  }
  g_free (file);

  file = g_build_filename (g_get_user_config_dir (), "gnome-initial-setup-done", NULL);
  if (!g_file_set_contents (file, "yes", -1, &error)) {
      g_warning ("Unable to create %s: %s", file, error->message);
      g_clear_error (&error);
  }
  g_free (file);
}
