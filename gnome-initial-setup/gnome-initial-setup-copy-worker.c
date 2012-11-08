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

  setpwent();
  while (TRUE) {
    if (getpwent_r (&pw, buf, sizeof (buf), &pwp))
      break;

    if (strcmp (pwp->pw_name, "gnome-initial-setup") == 0)
      return g_strdup (pwp->pw_dir);
  }

  g_error ("Could not find homedir for gnome-initial-setup");
}

static void
move_file_from_tmpfs (GFile *src_base,
                      const gchar *dir,
                      const gchar *path)
{
  GFile *dest_dir = g_file_new_for_path (dir);
  GFile *dest = g_file_get_child (dest_dir, path);
  gchar *basename = g_file_get_basename (dest);
  GFile *src = g_file_get_child (src_base, basename);

  GError *error = NULL;

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
  GError *error = NULL;

  g_type_init ();

  src = g_file_new_for_path (get_gnome_initial_setup_home_dir ());

  if (!g_file_query_exists (src, NULL))
    exit (EXIT_SUCCESS);

#define FILE(d, x) \
  move_file_from_tmpfs (src, g_get_user_##d##_dir (), x)

  FILE (config, "run-welcome-tour");
  FILE (config, "dconf/user");
  FILE (config, "goa-1.0/accounts.conf");
  FILE (data, "keyrings/Default.keyring");

  if (!g_file_delete (src, NULL, &error))
    {
      g_warning ("Unable to delete skeleton dir: %s", error->message);
      exit (EXIT_FAILURE);
    }

  return EXIT_SUCCESS;
}
