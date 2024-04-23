/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>
#include <locale.h>
#include <sys/stat.h>

typedef struct {
  GFile *testdir;
  GFile *srcdir;
  GFile *destdir;
} CopyWorkerFixture;

static void
remove_recursive (GFile *dir)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  if (dir == NULL)
    return;

  enumerator = g_file_enumerate_children (dir,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);
  g_assert_no_error (error);
  g_assert_nonnull (enumerator);

  while (1) {
    GFileInfo *info;
    GFile *child;
    GFileType child_type;

    ret = g_file_enumerator_iterate (enumerator, &info, &child, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    if (info == NULL)
      break;
    g_assert_nonnull (child);

    child_type = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE);
    if (child_type == G_FILE_TYPE_DIRECTORY) {
      remove_recursive (child);
    } else {
      ret = g_file_delete (child, NULL, &error);
      g_assert_no_error (error);
      g_assert_true (ret);
    }
  }

  ret = g_file_delete (dir, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
}

/* Create a temporary test directory. /var/tmp is preferred since it has
 * a better chance of supporting xattrs.
 */
static GFile *
create_testdir (void)
{
  const char *tmpl = "test-copy-worker-XXXXXX";
  g_autofree char *testdir = NULL;
  g_autoptr(GError) error = NULL;

  testdir = g_strdup_printf ("/var/tmp/%s", tmpl);
  if (g_mkdtemp (testdir) == NULL) {
    g_test_message ("Could not create test directory %s: %s",
                    testdir,
                    g_strerror (errno));

    /* Fallback to regular tmpdir. */
    g_clear_pointer (&testdir, g_free);
    testdir = g_dir_make_tmp (tmpl, &error);
    g_assert_no_error (error);
    g_assert_nonnull (testdir);
  }

  g_test_message ("Created test directory %s", testdir);
  return g_file_new_for_path (testdir);
}

static void
copy_worker_fixture_setup (CopyWorkerFixture *fixture,
                           gconstpointer      user_data)
{
  gboolean ret;
  g_autoptr(GError) error = NULL;

  fixture->testdir = create_testdir ();
  fixture->srcdir = g_file_get_child (fixture->testdir, "src");
  ret = g_file_make_directory (fixture->srcdir, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  fixture->destdir = g_file_get_child (fixture->testdir, "dest");
  ret = g_file_make_directory (fixture->destdir, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
}

static void
copy_worker_fixture_teardown (CopyWorkerFixture *fixture,
                              gconstpointer      user_data)
{
  if (fixture->testdir != NULL) {
    g_test_message ("Removing %s", g_file_peek_path (fixture->testdir));
    remove_recursive (fixture->testdir);
  }
  g_clear_object (&fixture->destdir);
  g_clear_object (&fixture->srcdir);
  g_clear_object (&fixture->testdir);
}

static void
create_file (GFile      *file,
             const char *contents)
{
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFileOutputStream) stream = NULL;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  parent = g_file_get_parent (file);
  ret = g_file_make_directory_with_parents (parent, NULL, &error);
  if (ret) {
    g_assert_no_error (error);
  } else {
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS);
    g_clear_error (&error);
  }

  stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  if (contents != NULL) {
    ret = g_output_stream_write_all (G_OUTPUT_STREAM (stream),
                                     contents,
                                     strlen (contents) + 1,
                                     NULL,
                                     NULL,
                                     &error);
    g_assert_no_error (error);
    g_assert_true (ret);
  }

  ret = g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
}

static void
spawn_copy_worker (const char  *src,
                   const char  *dest)
{
  const char *worker_path;
  char *worker_argv[4];
  g_autofree char *worker_src_arg = NULL;
  g_autofree char *worker_dest_arg = NULL;
  g_auto(GStrv) worker_envp = NULL;
  g_autofree char *worker_stdout = NULL;
  g_autofree char *worker_stderr = NULL;
  int worker_status = 0;
  gboolean ret;
  g_autoptr(GError) error = NULL;

  worker_path = g_getenv ("COPY_WORKER_PATH");
  g_assert_nonnull (worker_path);
  g_assert_true (*worker_path != '\0');
  worker_status = 0;
  worker_src_arg = g_strdup_printf ("--src=%s", src);
  worker_dest_arg = g_strdup_printf ("--dest=%s", dest);
  worker_argv[0] = g_strdup (worker_path);
  worker_argv[1] = worker_src_arg;
  worker_argv[2] = worker_dest_arg;
  worker_argv[3] = NULL;
  worker_envp = g_get_environ();
  worker_envp = g_environ_setenv (worker_envp, "G_MESSAGES_DEBUG", "all", TRUE);
  g_test_message ("Executing %s %s %s", worker_argv[0], worker_argv[1], worker_argv[2]);
  ret = g_spawn_sync (NULL,
                      worker_argv,
                      worker_envp,
                      G_SPAWN_DEFAULT,
                      NULL,
                      NULL,
                      &worker_stdout,
                      &worker_stderr,
                      &worker_status,
                      &error);
  if (worker_stdout != NULL)
    g_test_message ("Worker stdout:\n%s", worker_stdout);
  if (worker_stderr != NULL)
    g_test_message ("Worker stderr:\n%s", worker_stderr);
  g_assert_no_error (error);
  g_assert_true (ret);
  ret = g_spawn_check_exit_status (worker_status, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
}

static void
test_modtime (CopyWorkerFixture *fixture,
              gconstpointer      user_data)
{
  g_autoptr(GFile) src_file = NULL;
  g_autoptr(GFileInfo) src_file_info = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  guint64 src_file_time;
  g_autoptr(GFile) dest_file = NULL;
  g_autoptr(GFileInfo) dest_file_info = NULL;
  guint64 dest_file_time;

  g_clear_object (&src_file);
  src_file = g_file_get_child (fixture->srcdir,
                               ".config/gnome-initial-setup-done");
  create_file (src_file, NULL);

  g_clear_object (&src_file_info);
  src_file_info = g_file_query_info (src_file,
                                     G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL,
                                     &error);
  g_assert_no_error (error);
  g_assert_nonnull (src_file_info);

  /* Set the modification time back 1 second. */
  src_file_time = g_file_info_get_attribute_uint64 (src_file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  src_file_time -= 1;
  g_file_info_set_attribute_uint64 (src_file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED, src_file_time);
  ret = g_file_set_attributes_from_info (src_file,
                                         src_file_info,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL,
                                         &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  spawn_copy_worker (g_file_peek_path (fixture->srcdir),
                     g_file_peek_path (fixture->destdir));

  g_clear_object (&dest_file);
  g_clear_object (&dest_file_info);
  dest_file = g_file_get_child (fixture->destdir,
                                ".config/gnome-initial-setup-done");
  dest_file_info = g_file_query_info (dest_file,
                                      G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL,
                                      &error);
  g_assert_no_error (error);
  g_assert_nonnull (dest_file_info);

  dest_file_time = g_file_info_get_attribute_uint64 (dest_file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  g_assert_cmpuint (dest_file_time, ==, src_file_time);
}

static void
test_permissions (CopyWorkerFixture *fixture,
                  gconstpointer      user_data)
{
  g_autoptr(GFile) src_file = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInfo) src_file_info = NULL;
  guint32 src_mode;
  g_autoptr(GFile) dest_file = NULL;
  g_autoptr(GFileInfo) dest_file_info = NULL;
  guint32 dest_mode;

  g_clear_object (&src_file);
  src_file = g_file_get_child (fixture->srcdir,
                               ".config/gnome-initial-setup-done");
  create_file (src_file, NULL);

  g_clear_object (&src_file_info);
  src_file_info = g_file_query_info (src_file,
                                     G_FILE_ATTRIBUTE_UNIX_MODE,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL,
                                     &error);
  g_assert_no_error (error);
  g_assert_nonnull (src_file_info);

  if (!g_file_info_has_attribute (src_file_info, G_FILE_ATTRIBUTE_UNIX_MODE))
    return g_test_skip ("Not a UNIX system");

  /* Set the sticky bit on the source file. */
  src_mode = g_file_info_get_attribute_uint32 (src_file_info, G_FILE_ATTRIBUTE_UNIX_MODE);
  src_mode |= S_ISVTX;
  g_file_info_set_attribute_uint32 (src_file_info, G_FILE_ATTRIBUTE_UNIX_MODE, src_mode);
  ret = g_file_set_attributes_from_info (src_file,
                                         src_file_info,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL,
                                         &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  spawn_copy_worker (g_file_peek_path (fixture->srcdir),
                     g_file_peek_path (fixture->destdir));

  g_clear_object (&dest_file);
  g_clear_object (&dest_file_info);
  dest_file = g_file_get_child (fixture->destdir,
                                ".config/gnome-initial-setup-done");
  dest_file_info = g_file_query_info (dest_file,
                                      G_FILE_ATTRIBUTE_UNIX_MODE,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL,
                                      &error);
  g_assert_no_error (error);
  g_assert_nonnull (dest_file_info);

  dest_mode = g_file_info_get_attribute_uint32 (dest_file_info, G_FILE_ATTRIBUTE_UNIX_MODE);
  g_assert_cmpuint (dest_mode, ==, src_mode);
}

static void
test_xattrs (CopyWorkerFixture *fixture,
             gconstpointer      user_data)
{
  g_autoptr(GFile) src_file = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInfo) src_file_info = NULL;
  const char *src_value = "bar";
  g_autoptr(GFile) dest_file = NULL;
  g_autoptr(GFileInfo) dest_file_info = NULL;
  const char *dest_value;

  g_clear_object (&src_file);
  src_file = g_file_get_child (fixture->srcdir,
                               ".config/gnome-initial-setup-done");
  create_file (src_file, NULL);

  g_clear_object (&src_file_info);
  src_file_info = g_file_query_info (src_file,
                                     "xattr::foo",
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL,
                                     &error);
  g_assert_no_error (error);
  g_assert_nonnull (src_file_info);

  /* Set an xattr. */
  g_file_info_set_attribute_string (src_file_info, "xattr::foo", src_value);
  ret = g_file_set_attributes_from_info (src_file,
                                         src_file_info,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL,
                                         &error);
  if (ret) {
    g_assert_no_error (error);
  } else {
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
    return g_test_skip ("xattrs not supported on this file system");
  }

  spawn_copy_worker (g_file_peek_path (fixture->srcdir),
                     g_file_peek_path (fixture->destdir));

  g_clear_object (&dest_file);
  g_clear_object (&dest_file_info);
  dest_file = g_file_get_child (fixture->destdir,
                                ".config/gnome-initial-setup-done");
  dest_file_info = g_file_query_info (dest_file,
                                     "xattr::foo",
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL,
                                      &error);
  g_assert_no_error (error);
  g_assert_nonnull (dest_file_info);

  dest_value = g_file_info_get_attribute_string (dest_file_info, "xattr::foo");
  g_assert_cmpstr (dest_value, ==, src_value);
}

static void
test_contents (CopyWorkerFixture *fixture,
               gconstpointer      user_data)
{
  g_autoptr(GFile) src_file = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  const char src_contents[] = "foo\nbar\baz\n";
  gsize src_size = sizeof (src_contents);
  g_autoptr(GFile) dest_file = NULL;
  g_autofree char *dest_contents = NULL;
  gsize dest_size = 0;

  g_clear_object (&src_file);
  src_file = g_file_get_child (fixture->srcdir,
                               ".config/gnome-initial-setup-done");
  create_file (src_file, src_contents);

  spawn_copy_worker (g_file_peek_path (fixture->srcdir),
                     g_file_peek_path (fixture->destdir));

  g_clear_object (&dest_file);
  dest_file = g_file_get_child (fixture->destdir,
                                ".config/gnome-initial-setup-done");
  ret = g_file_load_contents (dest_file,
                              NULL,
                              &dest_contents,
                              &dest_size,
                              NULL,
                              &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_assert_cmpstr (dest_contents, ==, src_contents);
  g_assert_cmpuint (dest_size, ==, src_size);
}

static void
test_files (CopyWorkerFixture *fixture,
            gconstpointer      user_data)
{
  g_autoptr(GFile) src_file = NULL;
  const char copied_path[] = ".config/gnome-initial-setup-done";
  const char skipped_path[] = "someotherthing";
  g_autoptr(GFile) dest_file = NULL;

  g_clear_object (&src_file);
  src_file = g_file_get_child (fixture->srcdir, copied_path);
  create_file (src_file, NULL);

  g_clear_object (&src_file);
  src_file = g_file_get_child (fixture->srcdir, skipped_path);
  create_file (src_file, NULL);

  spawn_copy_worker (g_file_peek_path (fixture->srcdir),
                     g_file_peek_path (fixture->destdir));

  g_clear_object (&dest_file);
  dest_file = g_file_get_child (fixture->destdir, copied_path);
  g_assert_true (g_file_query_exists (dest_file, NULL));

  g_clear_object (&dest_file);
  dest_file = g_file_get_child (fixture->destdir, skipped_path);
  g_assert_false (g_file_query_exists (dest_file, NULL));
}

static void
test_directory (CopyWorkerFixture *fixture,
                gconstpointer      user_data)
{
  g_autoptr(GFile) src_file = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) dest_file = NULL;

  g_clear_object (&src_file);
  src_file = g_file_get_child (fixture->srcdir,
                               ".config/gnome-initial-setup-done");
  ret = g_file_make_directory_with_parents (src_file, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  spawn_copy_worker (g_file_peek_path (fixture->srcdir),
                     g_file_peek_path (fixture->destdir));

  /* Directories are skipped. */
  g_clear_object (&dest_file);
  dest_file = g_file_get_child (fixture->destdir,
                               ".config/gnome-initial-setup-done");
  g_assert_false (g_file_query_exists (dest_file, NULL));
}

#define test_add(name, func) \
  g_test_add (name, \
              CopyWorkerFixture, \
              NULL, \
              copy_worker_fixture_setup, \
              func, \
              copy_worker_fixture_teardown)

int
main (int    argc,
      char **argv)
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  test_add ("/modtime", test_modtime);
  test_add ("/permissions", test_permissions);
  test_add ("/xattrs", test_xattrs);
  test_add ("/contents", test_contents);
  test_add ("/files", test_files);
  test_add ("/directory", test_directory);

  return g_test_run ();
}
