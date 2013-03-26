/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copies settings installed from gnome-initial-setup and
 * sticks them in the user's profile */

#include <pwd.h>
#include <string.h>
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

static void
move_file_from_tmpfs (GFile       *src_base,
                      GFile       *dest_base,
                      const gchar *path)
{
  GFile *dest = g_file_get_child (dest_base, path);
  GFile *dest_parent = g_file_get_parent (dest);
  GFile *src = g_file_get_child (src_base, path);

  GError *error = NULL;

  g_file_make_directory_with_parents (dest_parent, NULL, NULL);

  if (!g_file_move (src, dest, G_FILE_COPY_NONE,
                    NULL, NULL, NULL, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
      g_warning ("Unable to move %s to %s: %s",
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
  GFile *src;
  GFile *dest;
  GError *error = NULL;
  char *initial_setup_homedir;
  gchar *gis_done_file_path;

  g_type_init ();

  initial_setup_homedir = get_gnome_initial_setup_home_dir ();
  if (initial_setup_homedir == NULL)
    exit (EXIT_SUCCESS);

  src = g_file_new_for_path (initial_setup_homedir);

  if (!g_file_query_exists (src, NULL))
    exit (EXIT_SUCCESS);

  dest = g_file_new_for_path (g_get_home_dir ());

#define FILE(path) \
  move_file_from_tmpfs (src, dest, path)

  FILE (".config/run-welcome-tour");
  FILE (".config/dconf/user");
  FILE (".config/goa-1.0/accounts.conf");
  FILE (".local/share/keyrings/login.keyring");

  gis_done_file_path = g_build_filename (g_get_user_config_dir (),
                                         "gnome-initial-setup-done",
                                         NULL);

  if (!g_file_set_contents (gis_done_file_path, "yes", -1, &error))
    g_warning ("Unable to create %s: %s", gis_done_file_path, error->message);

  if (!g_file_delete (src, NULL, &error))
    {
      g_warning ("Unable to delete skeleton dir: %s", error->message);
      exit (EXIT_FAILURE);
    }

  return EXIT_SUCCESS;
}
