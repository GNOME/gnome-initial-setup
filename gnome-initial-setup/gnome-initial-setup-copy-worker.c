/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copies settings installed from gnome-initial-setup and
 * sticks them in the user's profile */

#include <gio/gio.h>

#define SKELETON_PATH "gnome-initial-setup/skeleton"

static char *
get_skeleton_dir (void)
{
  return g_build_filename (g_get_user_runtime_dir (), SKELETON_PATH, NULL);
}

static gboolean
move_file_from_tmpfs (GFile *src_base,
                      const gchar *dir,
                      const gchar *path)
{
  GFile *dest_dir = g_file_new_for_path (dir);
  GFile *dest = g_file_get_child (dest_dir, path);
  gchar *basename = g_file_get_basename (dest);
  GFile *src = g_file_get_child (src_base, basename);

  GError *error = NULL;
  gboolean ret = TRUE;

  if (!g_file_move (src, dest, G_FILE_COPY_NONE,
                    NULL, NULL, NULL, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
      g_warning ("Unable to move %s to %s: %s",
                 g_file_get_path (src),
                 g_file_get_path (dest),
                 error->message);
      ret = FALSE;
    }
    g_error_free (error);
  }

  g_object_unref (dest_dir);
  g_object_unref (dest);
  g_object_unref (src);
  g_free (basename);

  return ret;
}

int
main (int    argc,
      char **argv)
{
  GFile *src;
  GError *error = NULL;
  int ret = 0;

  g_type_init ();

  src = g_file_new_for_path (get_skeleton_dir ());

  if (!g_file_query_exists (src, NULL))
    goto out;

  ret = 1;

#define FILE(d, x) \
  move_file_from_tmpfs (src, g_get_user_##d##_dir (), x)

  FILE (config, "run-welcome-tour");
  FILE (config, "dconf/user");
  FILE (config, "goa-1.0/accounts.conf");
  FILE (data, "keyrings/Default.keyring");

  if (!g_file_delete (src, NULL, &error))
    {
      g_warning ("Unable to delete skeleton dir: %s", error->message);
      goto out;
    }

  ret = 0;

 out:
  g_object_unref (src);
  return ret;
}
