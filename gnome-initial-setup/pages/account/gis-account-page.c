/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Account page {{{1 */

#include "config.h"
#include "gis-account-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <act/act-user-manager.h>

#include <gnome-keyring.h>

#include "um-utils.h"
#include "um-photo-dialog.h"
#include "pw-utils.h"

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#define OBJ(type,name) ((type)gtk_builder_get_object(data->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

typedef struct _AccountData AccountData;

struct _AccountData {
  SetupData *setup;
  GtkBuilder *builder;

  ActUser *act_user;
  ActUserManager *act_client;

  gboolean valid_name;
  gboolean valid_username;
  gboolean valid_password;
  const gchar *password_reason;
  ActUserPasswordMode password_mode;
  ActUserAccountType account_type;

  gboolean user_data_unsaved;

  GtkWidget *photo_dialog;
  GdkPixbuf *avatar_pixbuf;
  gchar *avatar_filename;
};

static void
copy_account_data (SetupData *setup, AccountData *data)
{
  ActUser *user = data->act_user;
  /* here is where we copy all the things we just
   * configured, from the current users home dir to the
   * account that was created in the first step
   */
  g_debug ("Copying account data");
  g_settings_sync ();

  gis_copy_account_file (user, ".config/dconf/user");
  gis_copy_account_file (user, ".config/goa-1.0/accounts.conf");
  gis_copy_account_file (user, ".gnome2/keyrings/Default.keyring");
}

static void
update_account_page_status (AccountData *data)
{
  gboolean complete;

  complete = data->valid_name && data->valid_username &&
    (data->valid_password ||
     data->password_mode == ACT_USER_PASSWORD_MODE_NONE);

  gis_assistant_set_page_complete (gis_get_assistant (data->setup), WID("account-page"), complete);
  gtk_widget_set_sensitive (WID("local-account-done-button"), complete);
}

static void
fullname_changed (GtkWidget *w, GParamSpec *pspec, AccountData *data)
{
  GtkWidget *combo;
  GtkWidget *entry;
  GtkTreeModel *model;
  const char *name;

  name = gtk_entry_get_text (GTK_ENTRY (w));

  combo = WID("account-username-combo");
  entry = gtk_bin_get_child (GTK_BIN (combo));
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  gtk_list_store_clear (GTK_LIST_STORE (model));

  data->valid_name = is_valid_name (name);
  data->user_data_unsaved = TRUE;

  if (!data->valid_name) {
    gtk_entry_set_text (GTK_ENTRY (entry), "");
    return;
  }

  generate_username_choices (name, GTK_LIST_STORE (model));

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  update_account_page_status (data);
}

static void
username_changed (GtkComboBoxText *combo, AccountData *data)
{
  const gchar *username;
  gchar *tip;
  GtkWidget *entry;

  username = gtk_combo_box_text_get_active_text (combo);

  data->valid_username = is_valid_username (username, &tip);
  data->user_data_unsaved = TRUE;

  entry = gtk_bin_get_child (GTK_BIN (combo));

  if (tip) {
    set_entry_validation_error (GTK_ENTRY (entry), tip);
    g_free (tip);
  }
  else {
    clear_entry_validation_error (GTK_ENTRY (entry));
  }

  update_account_page_status (data);
}

static void
password_check_changed (GtkWidget *w, GParamSpec *pspec, AccountData *data)
{
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  gboolean need_password;

  need_password = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

  if (need_password) {
    data->password_mode = ACT_USER_PASSWORD_MODE_REGULAR;
    data->valid_password = FALSE;
  }
  else {
    data->password_mode = ACT_USER_PASSWORD_MODE_NONE;
    data->valid_password = TRUE;
  }

  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");

  gtk_entry_set_text (GTK_ENTRY (password_entry), "");
  gtk_entry_set_text (GTK_ENTRY (confirm_entry), "");
  gtk_widget_set_sensitive (password_entry, need_password);
  gtk_widget_set_sensitive (confirm_entry, need_password);

  data->user_data_unsaved = TRUE;
  update_account_page_status (data);
}

static void
admin_check_changed (GtkWidget *w, GParamSpec *pspec, AccountData *data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
    data->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;
  }
  else {
    data->account_type = ACT_USER_ACCOUNT_TYPE_STANDARD;
  }

  data->user_data_unsaved = TRUE;
  update_account_page_status (data);
}

#define MIN_PASSWORD_LEN 6

static void
update_password_entries (AccountData *data)
{
  const gchar *password;
  const gchar *verify;
  const gchar *username;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  GtkWidget *username_combo;
  gdouble strength;
  const gchar *hint;
  const gchar *long_hint = NULL;

  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");
  username_combo = WID("account-username-combo");

  password = gtk_entry_get_text (GTK_ENTRY (password_entry));
  verify = gtk_entry_get_text (GTK_ENTRY (confirm_entry));
  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (username_combo));

  strength = pw_strength (password, NULL, username, &hint, &long_hint);

  if (strength == 0.0) {
    data->valid_password = FALSE;
    data->password_reason = long_hint ? long_hint : hint;
  }
  else if (strcmp (password, verify) != 0) {
    data->valid_password = FALSE;
    data->password_reason = _("Passwords do not match");
  }
  else {
    data->valid_password = TRUE;
  }
}

static void
password_changed (GtkWidget *w, GParamSpec *pspec, AccountData *data)
{
  update_password_entries (data);

  data->user_data_unsaved = TRUE;
  update_account_page_status (data);
}

static void
confirm_changed (GtkWidget *w, GParamSpec *pspec, AccountData *data)
{
  clear_entry_validation_error (GTK_ENTRY (w));
  update_password_entries (data);

  data->user_data_unsaved = TRUE;
  update_account_page_status (data);
}

static gboolean
confirm_entry_focus_out (GtkWidget     *entry,
                         GdkEventFocus *event,
                         AccountData     *data)
{
  const gchar *password;
  const gchar *verify;
  GtkEntry *password_entry;
  GtkEntry *confirm_entry;

  password_entry = OBJ(GtkEntry*, "account-password-entry");
  confirm_entry = OBJ(GtkEntry*, "account-confirm-entry");
  password = gtk_entry_get_text (password_entry);
  verify = gtk_entry_get_text (confirm_entry);

  if (strlen (password) > 0 && strlen (verify) > 0) {
    if (!data->valid_password) {
      set_entry_validation_error (confirm_entry,
                                  data->password_reason);
    }
    else {
      clear_entry_validation_error (confirm_entry);
    }
  }

  return FALSE;
}

static void
set_user_avatar (AccountData *data)
{
  GFile *file = NULL;
  GFileIOStream *io_stream = NULL;
  GOutputStream *stream = NULL;
  GError *error = NULL;

  if (data->avatar_filename != NULL) {
    act_user_set_icon_file (data->act_user, data->avatar_filename);
    return;
  }

  if (data->avatar_pixbuf == NULL) {
    return;
  }

  file = g_file_new_tmp ("usericonXXXXXX", &io_stream, &error);
  if (error != NULL)
    goto out;

  stream = g_io_stream_get_output_stream (G_IO_STREAM (io_stream));
  if (!gdk_pixbuf_save_to_stream (data->avatar_pixbuf, stream, "png", NULL, &error, NULL))
    goto out;

  act_user_set_icon_file (data->act_user, g_file_get_path (file)); 

 out:
  if (error != NULL) {
    g_warning ("failed to save image: %s", error->message);
    g_error_free (error);
  }
  g_clear_object (&stream);
  g_clear_object (&io_stream);
  g_clear_object (&file);
}

static void
create_user (AccountData *data)
{
  const gchar *username;
  const gchar *fullname;
  GError *error;

  username = gtk_combo_box_text_get_active_text (OBJ(GtkComboBoxText*, "account-username-combo"));
  fullname = gtk_entry_get_text (OBJ(GtkEntry*, "account-fullname-entry"));

  error = NULL;

  data->act_user = act_user_manager_create_user (data->act_client, username, fullname, data->account_type, &error);
  if (error != NULL) {
    g_warning ("Failed to create user: %s", error->message);
    g_error_free (error);
  }

  set_user_avatar (data);
}

static void save_account_data (AccountData *data);

static gulong when_loaded;

static void
save_when_loaded (ActUser *user, GParamSpec *pspec, AccountData *data)
{
  g_signal_handler_disconnect (user, when_loaded);
  when_loaded = 0;

  save_account_data (data);
}

static void
clear_account_page (AccountData *data)
{
  GtkWidget *fullname_entry;
  GtkWidget *username_combo;
  GtkWidget *password_check;
  GtkWidget *admin_check;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  gboolean need_password;

  fullname_entry = WID("account-fullname-entry");
  username_combo = WID("account-username-combo");
  password_check = WID("account-password-check");
  admin_check = WID("account-admin-check");
  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");

  data->valid_name = FALSE;
  data->valid_username = FALSE;
  data->valid_password = TRUE;
  data->password_mode = ACT_USER_PASSWORD_MODE_NONE;
  data->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;
  data->user_data_unsaved = FALSE;

  need_password = data->password_mode != ACT_USER_PASSWORD_MODE_NONE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (password_check), need_password);
  gtk_widget_set_sensitive (password_entry, need_password);
  gtk_widget_set_sensitive (confirm_entry, need_password);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (admin_check), data->account_type == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR);

  gtk_entry_set_text (GTK_ENTRY (fullname_entry), "");
  gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (username_combo))));
  gtk_entry_set_text (GTK_ENTRY (password_entry), "");
  gtk_entry_set_text (GTK_ENTRY (confirm_entry), "");
}

static void
save_account_data (AccountData *data)
{
  const gchar *realname;
  const gchar *username;
  const gchar *password;

  if (!data->user_data_unsaved) {
    return;
  }

  /* this can happen when going back */
  if (!data->valid_name ||
      !data->valid_username ||
      !data->valid_password) {
    return;
  }

  if (data->act_user == NULL) {
    create_user (data);
  }

  if (data->act_user == NULL) {
    g_warning ("User creation failed");
    clear_account_page (data);
    return;
  }

  if (!act_user_is_loaded (data->act_user)) {
    if (when_loaded == 0)
      when_loaded = g_signal_connect (data->act_user, "notify::is-loaded",
                                      G_CALLBACK (save_when_loaded), data);
    return;
  }

  realname = gtk_entry_get_text (OBJ (GtkEntry*, "account-fullname-entry"));
  username = gtk_combo_box_text_get_active_text (OBJ (GtkComboBoxText*, "account-username-combo"));
  password = gtk_entry_get_text (OBJ (GtkEntry*, "account-password-entry"));

  act_user_set_real_name (data->act_user, realname);
  act_user_set_user_name (data->act_user, username);
  act_user_set_account_type (data->act_user, data->account_type);
  if (data->password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
    act_user_set_password (data->act_user, password, NULL);
  }
  else {
    act_user_set_password_mode (data->act_user, data->password_mode);
  }

  gnome_keyring_create_sync ("Default", password ? password : "");
  gnome_keyring_set_default_keyring_sync ("Default");

  data->user_data_unsaved = FALSE;
}

static void
show_local_account_dialog (GtkWidget *button,
                           AccountData *data)
{
  GtkWidget *dialog;

  dialog = WID("local-account-dialog");

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
hide_local_account_dialog (GtkButton *button, gpointer user_data)
{
  AccountData *data = user_data;
  GtkWidget *dialog;

  dialog = WID("local-account-dialog");

  gtk_widget_hide (dialog);
  clear_account_page (data);
}

static void
create_local_account (GtkButton *button, gpointer user_data)
{
  AccountData *data = user_data;
  gtk_widget_hide (WID("local-account-dialog"));
}

static void
avatar_callback (GdkPixbuf   *pixbuf,
                 const gchar *filename,
                 gpointer     user_data)
{
  AccountData *data = user_data;
  GtkWidget *image;
  GdkPixbuf *tmp;

  g_clear_object (&data->avatar_pixbuf);
  g_free (data->avatar_filename);
  data->avatar_filename = NULL;

  image = WID("local-account-avatar-image");

  if (pixbuf) {
    data->avatar_pixbuf = g_object_ref (pixbuf);
    tmp = gdk_pixbuf_scale_simple (pixbuf, 64, 64, GDK_INTERP_BILINEAR);
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), tmp);
    g_object_unref (tmp);
  }
  else if (filename) {
    data->avatar_filename = g_strdup (filename);
    tmp = gdk_pixbuf_new_from_file_at_size (filename, 64, 64, NULL);
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), tmp);
    g_object_unref (tmp);
  }
  else {
    gtk_image_set_from_icon_name (GTK_IMAGE (image), "avatar-default",
                                  GTK_ICON_SIZE_DIALOG);
  }
}

void
gis_prepare_account_page (SetupData *setup)
{
  GtkWidget *fullname_entry;
  GtkWidget *username_combo;
  GtkWidget *password_check;
  GtkWidget *admin_check;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  GtkWidget *local_account_cancel_button;
  GtkWidget *local_account_done_button;
  GtkWidget *local_account_avatar_button;
  AccountData *data = g_slice_new (AccountData);
  GisAssistant *assistant = gis_get_assistant (setup);
  data->builder = gis_builder ("gis-account-page");
  data->setup = setup;

  gtk_widget_show (WID("account-page"));

  g_signal_connect (WID("account-new-local"), "clicked",
                    G_CALLBACK (show_local_account_dialog), data);

  gtk_window_set_transient_for (OBJ(GtkWindow*,"local-account-dialog"),
                                gis_get_main_window(setup));

  fullname_entry = WID("account-fullname-entry");
  username_combo = WID("account-username-combo");
  password_check = WID("account-password-check");
  admin_check = WID("account-admin-check");
  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");
  local_account_cancel_button = WID("local-account-cancel-button");
  local_account_done_button = WID("local-account-done-button");
  local_account_avatar_button = WID("local-account-avatar-button");
  data->photo_dialog = (GtkWidget *)um_photo_dialog_new (local_account_avatar_button,
                                                         avatar_callback,
                                                         data);

  g_signal_connect (fullname_entry, "notify::text",
                    G_CALLBACK (fullname_changed), data);
  g_signal_connect (username_combo, "changed",
                    G_CALLBACK (username_changed), data);
  g_signal_connect (password_check, "notify::active",
                    G_CALLBACK (password_check_changed), data);
  g_signal_connect (admin_check, "notify::active",
                    G_CALLBACK (admin_check_changed), data);
  g_signal_connect (password_entry, "notify::text",
                    G_CALLBACK (password_changed), data);
  g_signal_connect (confirm_entry, "notify::text",
                    G_CALLBACK (confirm_changed), data);
  g_signal_connect_after (confirm_entry, "focus-out-event",
                          G_CALLBACK (confirm_entry_focus_out), data);
  g_signal_connect (local_account_cancel_button, "clicked",
                    G_CALLBACK (hide_local_account_dialog), data);
  g_signal_connect (local_account_done_button, "clicked",
                    G_CALLBACK (create_local_account), data);

  data->act_client = act_user_manager_get_default ();

  g_object_set_data (OBJ (GObject *, "account-page"), "gis-page-title", _("Login"));
  gis_assistant_add_page (assistant, WID ("account-page"));
  gis_add_summary_callback (setup, (GFunc)copy_account_data, data);

  clear_account_page (data);
  update_account_page_status (data);
}
