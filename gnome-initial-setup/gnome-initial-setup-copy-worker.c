/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copies settings installed from gnome-initial-setup and
 * sticks them in the user's profile */

#include "config.h"

#include <pwd.h>
#include <string.h>
#include <locale.h>
#include <gio/gio.h>
#include <stdlib.h>

static char *
get_gnome_initial_setup_home_dir (void)
{
  struct passwd pw, *pwp;
  char buf[4096];

  getpwnam_r ("gnome-initial-setup", &pw, buf, sizeof (buf), &pwp);
  if (pwp != NULL)
    return g_strdup (pwp->pw_dir);
  else
    return NULL;
}

static gboolean
file_is_ours (GFile *file)
{
  GFileInfo *info;
  uid_t uid;

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_UNIX_UID,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            NULL,
                            NULL);
  if (!info)
    return FALSE;

  uid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID);
  g_object_unref (info);

  return uid == geteuid ();
}

static void
copy_file_from_homedir (GFile       *src_base,
                        GFile       *dest_base,
                        const gchar *path)
{
  GFile *dest = g_file_get_child (dest_base, path);
  GFile *dest_parent = g_file_get_parent (dest);
  GFile *src = g_file_get_child (src_base, path);

  GError *error = NULL;

  g_file_make_directory_with_parents (dest_parent, NULL, NULL);

  g_debug ("Copying %s to %s", g_file_get_path (src), g_file_get_path (dest));
  if (!g_file_copy (src, dest, G_FILE_COPY_NONE,
                    NULL, NULL, NULL, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
      g_warning ("Unable to copy %s to %s: %s",
                 g_file_get_path (src),
                 g_file_get_path (dest),
                 error->message);
    }
  }
}

int
main (int    argc,
      char **argv)
{
  g_autofree char *src_path = NULL;
  g_autofree char *dest_path = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  GFile *src;
  GFile *dest;

  GOptionEntry entries[] = {
    { "src", 0, 0, G_OPTION_ARG_FILENAME, &src_path,
      "Source path (default: home directory)", NULL },
    { "dest", 0, 0, G_OPTION_ARG_FILENAME, &dest_path,
      "Destination path (default: gnome-initial-setup home directory)", NULL },
    { NULL }
  };

  setlocale (LC_ALL, "");

  context = g_option_context_new ("— GNOME initial setup copy worker");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("Error parsing arguments: %s\n", error->message);
    exit (EXIT_FAILURE);
  }

  if (src_path == NULL) {
    src_path = get_gnome_initial_setup_home_dir ();
    if (src_path == NULL) {
      g_debug ("Could not determine gnome-initial-setup homedir");
      exit (EXIT_SUCCESS);
    }
  }

  if (dest_path == NULL)
    dest_path = g_strdup (g_get_home_dir ());

  src = g_file_new_for_path (src_path);

  if (!g_file_query_exists (src, NULL)) {
    g_debug ("Initial setup homedir %s does not exist", src_path);
    exit (EXIT_SUCCESS);
  }

  if (!file_is_ours (src)) {
    g_warning ("Initial setup homedir %s is not owned by UID %u",
               src_path,
               geteuid ());
    exit (EXIT_SUCCESS);
  }

  dest = g_file_new_for_path (dest_path);

#define FILE(path) \
  copy_file_from_homedir (src, dest, path);

  FILE (".config/gnome-initial-setup-done");
  FILE (".config/dconf/user");
  FILE (".config/monitors.xml");
  FILE (".local/share/keyrings/login.keyring");

  return EXIT_SUCCESS;
}
