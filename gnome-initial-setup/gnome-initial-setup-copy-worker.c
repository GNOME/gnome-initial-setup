/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copies settings installed from gnome-initial-setup and
 * sticks them in the user's profile */

#include <gio/gio.h>

#define SKELETON_DIR "/dev/shm/gnome-initial-setup/skeleton"

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

  if (!g_file_move (src, dest, G_FILE_COPY_NONE,
                    NULL, NULL, NULL, &error)) {
    g_warning ("Unable to move %s to %s: %s",
               g_file_get_path (src),
               g_file_get_path (dest),
               error->message);
    goto out;
  }

 out:
  g_object_unref (dest_dir);
  g_object_unref (dest);
  g_object_unref (src);
  g_free (basename);

  if (error != NULL) {
    g_error_free (error);
    return FALSE;
  } else {
    return TRUE;
  }
}

int
main (int    argc,
      char **argv)
{
  GFile *src;
  int ret = 0;

  g_type_init ();

  src = g_file_new_for_path (SKELETON_DIR);

  if (g_file_query_exists (src, NULL))
    goto out;

  ret = 1;

#define MOVE(d, x)                                                      \
  if (!move_file_from_tmpfs (src, g_get_user_##d##_dir (), x))          \
    goto out;

  MOVE (config, "dconf/user");
  MOVE (config, "goa-1.0/accounts.conf");
  MOVE (data, "keyrings/Default.keyring");

  ret = 0;

 out:
  g_object_unref (src);
  return ret;
}
