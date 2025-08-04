/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copies settings installed from gnome-initial-setup and
 * sticks them in the user's profile */

#include "config.h"

/* For futimens and nanosecond struct stat fields. */
#if !defined _POSIX_C_SOURCE || _POSIX_C_SOURCE < 200809L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <pwd.h>
#include <string.h>
#include <locale.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_XATTR
#include <sys/xattr.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

static gboolean
file_is_ours (const char *path)
{
  GStatBuf buf;

  if (g_stat (path, &buf) != 0) {
    if (errno != ENOENT)
      g_warning ("Could not get information on \"%s\": %s",
                 path,
                 g_strerror (errno));
    return FALSE;
  }

  return buf.st_uid == geteuid ();
}

static void
copy_file_mode (GStatBuf   *src_stat,
                const char *dest,
                int         dest_fd)
{
  if (fchmod (dest_fd, src_stat->st_mode) == -1) {
    g_warning ("Could not set file permissions for %s: %s", dest, g_strerror (errno));
    return;
  }
}

/* Copy the source file modification time. Like g_file_copy, access time
 * is not changed.
 */
static void
copy_file_modtime (GStatBuf   *src_stat,
                   const char *dest,
                   int         dest_fd)
{
  struct timespec times[2];

  times[0].tv_sec = 0;
  times[0].tv_nsec = UTIME_OMIT;
  times[1] = src_stat->st_mtim;
  if (futimens (dest_fd, times) == -1) {
    g_warning ("Could not set file timestamps for %s: %s", dest, g_strerror (errno));
    return;
  }
}

static void
copy_file_xattrs (const char *src,
                  int         src_fd,
                  const char *dest,
                  int         dest_fd)
{
#ifdef HAVE_XATTR
  g_autofree char *attrs = NULL;
  ssize_t attrs_size;
  ssize_t buf_size;
  char *key;

  buf_size = 0;
  while (1) {
    attrs_size = flistxattr (src_fd, attrs, buf_size);
    if (attrs_size <= buf_size)
      break;

    buf_size = attrs_size;
    attrs = g_realloc (attrs, buf_size);
  }

  if (attrs_size == -1) {
    g_warning ("Could not get extended attrs for %s: %s", src, g_strerror (errno));
    return;
  } else if (attrs_size == 0) {
    g_debug ("%s has no extended attributes", src);
    return;
  }

  key = attrs;
  while (attrs_size > 0) {
    g_autofree char *value = NULL;
    ssize_t value_size;
    size_t key_len;

    /* Skip SELinux context so the policy default is used. */
    if (g_strcmp0 (key, "security.selinux") == 0)
      goto next_attr;

    buf_size = 0;
    while (1) {
      value_size = fgetxattr (src_fd, key, value, buf_size);
      if (value_size <= buf_size)
        break;

      buf_size = value_size;
      value = g_realloc (value, buf_size);
    }

    if (value_size == -1) {
      g_warning ("Could not get extended attr %s for %s: %s", key, src, g_strerror (errno));
      goto next_attr;
    }

    if (fsetxattr (dest_fd, key, value, value_size, 0) == -1) {
      g_warning ("Could not set extended attr %s for %s: %s", key, src, g_strerror (errno));
      goto next_attr;
    }

  next_attr:
    key_len = strlen (key);
    key += key_len + 1;
    attrs_size -= key_len + 1;
  }
#endif /* HAVE_XATTR */
}

static void
copy_file (const char *src,
           const char *dest,
           GStatBuf   *src_stat)
{
  int src_fd = -1;
  int dest_fd = -1;
  mode_t dest_mode;
  guint8 read_buf[8192];
  const guint8 *write_buf;
  ssize_t bytes_read;
  ssize_t bytes_to_write;
  ssize_t bytes_written;

  src_fd = g_open (src, O_RDONLY | O_BINARY, 0);
  if (src_fd == -1) {
    g_warning ("Could not open %s: %s", src, g_strerror (errno));
    goto out;
  }

  dest_mode = src_stat->st_mode & ~S_IFMT;
  dest_fd = g_open (dest, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, dest_mode);
  if (dest_fd == -1) {
    g_warning ("Could not open %s: %s", dest, g_strerror (errno));
    goto out;
  }

  while (1) {
    bytes_read = read (src_fd, read_buf, sizeof (read_buf));
    if (bytes_read == 0) {
      break;
    } else if (bytes_read == -1) {
      if (errno == EINTR)
        continue;
      g_warning ("Unable to read %s: %s", src, g_strerror (errno));
      goto out;
    }

    write_buf = read_buf;
    bytes_to_write = bytes_read;
    while (bytes_to_write > 0) {
      bytes_written = write (dest_fd, write_buf, bytes_to_write);
      if (bytes_written == -1) {
        if (errno == EINTR)
          continue;
        g_warning ("Unable to write %s: %s", dest, g_strerror (errno));
        goto out;
      }

      write_buf += bytes_written;
      bytes_to_write -= bytes_written;
    }
  }

  copy_file_mode (src_stat, dest, dest_fd);
  copy_file_modtime (src_stat, dest, dest_fd);
  copy_file_xattrs (src, src_fd, dest, dest_fd);

 out:
  if (src_fd >= 0)
    g_close (src_fd, NULL);
  if (dest_fd >= 0)
    g_close (dest_fd, NULL);
}

static void
copy_file_from_homedir (const gchar *src_base,
                        const gchar *dest_base,
                        const gchar *path)
{
  g_autofree gchar *dest = g_build_filename (dest_base, path, NULL);
  g_autofree gchar *dest_parent = g_path_get_dirname (dest);
  g_autofree gchar *src = g_build_filename (src_base, path, NULL);
  GStatBuf src_stat;

  g_debug ("Copying %s to %s", src, dest);

  if (g_lstat (src, &src_stat) == -1) {
    g_debug ("Cannot copy %s: %s", src, g_strerror (errno));
    return;
  }

  /* Only regular files are supported */
  if (!S_ISREG (src_stat.st_mode)) {
    if (S_ISDIR (src_stat.st_mode)) {
      g_warning ("Skipping directory %s", src);
      return;
    } else {
      g_warning ("Skipping non-file %s", src);
      return;
    }
  }

  if (g_mkdir_with_parents (dest_parent, 0777) != 0) {
    g_warning ("Unable to create directory %s: %s", dest_parent, g_strerror (errno));
    return;
  }

  copy_file (src, dest, &src_stat);
}

int
main (int    argc,
      char **argv)
{
  g_autofree char *src = NULL;
  g_autofree char *dest = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;

  GOptionEntry entries[] = {
    { "src", 0, 0, G_OPTION_ARG_FILENAME, &src,
      "Source path (default: " GDM_EXPORT_DIR ")", NULL },
    { "dest", 0, 0, G_OPTION_ARG_FILENAME, &dest,
      "Destination path (default: home directory)", NULL },
    { NULL }
  };

  setlocale (LC_ALL, "");

  context = g_option_context_new ("— GNOME initial setup copy worker");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("Error parsing arguments: %s\n", error->message);
    exit (EXIT_FAILURE);
  }

  if (src == NULL)
    src = g_strdup (GDM_EXPORT_DIR);

  if (dest == NULL)
    dest = g_strdup (g_get_home_dir ());

  if (g_access (src, F_OK) != 0) {
    g_debug ("Initial setup homedir %s does not exist", src);
    exit (EXIT_SUCCESS);
  }

  if (!file_is_ours (src)) {
    g_warning ("Initial setup homedir %s is not owned by UID %u",
               src,
               geteuid ());
    exit (EXIT_SUCCESS);
  }

#define FILE(path) \
  copy_file_from_homedir (src, dest, path);

  FILE (".config/gnome-initial-setup-done");
  FILE (".config/dconf/user");
  FILE (".config/monitors.xml");

  return EXIT_SUCCESS;
}
