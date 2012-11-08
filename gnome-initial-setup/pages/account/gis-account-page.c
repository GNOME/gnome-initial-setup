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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Account page {{{1 */

#define PAGE_ID "account"

#include "config.h"
#include "gis-account-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <act/act-user-manager.h>

#include "um-realm-manager.h"
#include "um-utils.h"
#include "um-photo-dialog.h"
#include "pw-utils.h"

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#define OBJ(type,name) ((type)gtk_builder_get_object(data->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

typedef struct _AccountData AccountData;

typedef enum {
  UM_LOCAL,
  UM_ENTERPRISE,
  NUM_MODES,
} UmAccountMode;

struct _AccountData {
  GisDriver *driver;
  GtkWidget *widget;
  GtkBuilder *builder;

  ActUser *act_user;
  ActUserManager *act_client;

  UmAccountMode mode;

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

  gboolean has_enterprise;
  guint realmd_watch;
  UmRealmManager *realm_manager;
  gboolean domain_chosen;
};

static void
show_error_dialog (AccountData *data,
                   const gchar *message,
                   GError *error)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (data->widget)),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", message);

  if (error != NULL) {
    g_dbus_error_strip_remote_error (error);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", error->message);
  }

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
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

static gboolean
local_validate (AccountData *data)
{
  return data->valid_name && data->valid_username &&
    (data->valid_password ||
     data->password_mode == ACT_USER_PASSWORD_MODE_NONE);
}

static gboolean
enterprise_validate (AccountData *data)
{
  const gchar *name;
  gboolean valid_name;
  gboolean valid_domain;
  GtkTreeIter iter;
  GtkComboBox *domain = OBJ(GtkComboBox*, "enterprise-domain");

  name = gtk_entry_get_text (OBJ(GtkEntry*, "enterprise-login"));
  valid_name = is_valid_name (name);

  if (gtk_combo_box_get_active_iter (domain, &iter)) {
    gtk_tree_model_get (gtk_combo_box_get_model (domain),
                        &iter, 0, &name, -1);
  } else {
    name = gtk_entry_get_text (OBJ(GtkEntry*, "enterprise-domain-entry"));
  }

  valid_domain = is_valid_name (name);
  return valid_name && valid_domain;
}

static gboolean
page_validate (AccountData *data)
{
  switch (data->mode) {
  case UM_LOCAL:
    return local_validate (data);
  case UM_ENTERPRISE:
    return enterprise_validate (data);
  default:
    g_assert_not_reached ();
  }
}

static void
update_account_page_status (AccountData *data)
{
  gis_assistant_set_page_complete (gis_driver_get_assistant (data->driver),
                                   data->widget, page_validate (data));
}

static void
set_mode (AccountData   *data,
          UmAccountMode  mode)
{
  if (data->mode == mode)
    return;

  data->mode = mode;
  gtk_widget_set_visible (WID ("local-area"), (mode == UM_LOCAL));
  gtk_widget_set_visible (WID ("enterprise-area"), (mode == UM_ENTERPRISE));
  gtk_toggle_button_set_active (OBJ (GtkToggleButton *, "local-button"), (mode == UM_LOCAL));
  gtk_toggle_button_set_active (OBJ (GtkToggleButton *, "enterprise-button"), (mode == UM_ENTERPRISE));

  update_account_page_status (data);
}

static void
set_has_enterprise (AccountData *data,
                    gboolean     has_enterprise)
{
  if (data->has_enterprise == has_enterprise)
    return;

  data->has_enterprise = has_enterprise;
  gtk_widget_set_visible (WID ("account-mode"), has_enterprise);

  if (!has_enterprise)
    set_mode (data, UM_LOCAL);
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
local_create_user (AccountData *data)
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

  gis_driver_set_user_permissions (data->driver, data->act_user, password);

  data->user_data_unsaved = FALSE;
}

static void
on_permit_user_login (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
  AccountData *data = user_data;
  UmRealmCommon *common;
  GError *error = NULL;
  gchar *login;

  common = UM_REALM_COMMON (source);
  um_realm_common_call_change_login_policy_finish (common, result, &error);
  if (error == NULL) {

    /*
     * Now tell the account service about this user. The account service
     * should also lookup information about this via the realm and make
     * sure all that is functional.
     */
    login = um_realm_calculate_login (common, gtk_entry_get_text (OBJ (GtkEntry*, "enterprise-login")));
    g_return_if_fail (login != NULL);

    g_debug ("Caching remote user: %s", login);

    data->act_user = act_user_manager_cache_user (data->act_client, login, NULL);

    g_free (login);
  } else {
    show_error_dialog (data, _("Failed to register account"), error);
    g_message ("Couldn't permit logins on account: %s", error->message);
    g_error_free (error);
  }
}

static void
enterprise_permit_user_login (AccountData *data, UmRealmObject *realm)
{
  UmRealmCommon *common;
  gchar *login;
  const gchar *add[2];
  const gchar *remove[1];
  GVariant *options;

  common = um_realm_object_get_common (realm);

  login = um_realm_calculate_login (common, gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-login")));
  g_return_if_fail (login != NULL);

  add[0] = login;
  add[1] = NULL;
  remove[0] = NULL;

  g_debug ("Permitting login for: %s", login);
  options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

  um_realm_common_call_change_login_policy (common, "",
                                            add, remove, options,
                                            NULL,
                                            on_permit_user_login,
                                            data);

  g_object_unref (common);
  g_free (login);
}

static void
on_realm_joined (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
  AccountData *data = user_data;
  UmRealmObject *realm = UM_REALM_OBJECT (source);
  GError *error = NULL;

  um_realm_join_finish (realm, result, &error);

  /* Yay, joined the domain, register the user locally */
  if (error == NULL) {
    g_debug ("Joining realm completed successfully");
    enterprise_permit_user_login (data, realm);

    /* Credential failure while joining domain, prompt for admin creds */
  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN) ||
             g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
    g_debug ("Joining realm failed due to credentials");

    /* XXX */
    /* join_show_prompt (self, error); */

    /* Other failure */
  } else {
    show_error_dialog (data, _("Failed to join domain"), error);
    g_message ("Failed to join the domain: %s", error->message);
  }

  g_clear_error (&error);
}

static void
on_realm_login (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
  AccountData *data = user_data;
  UmRealmObject *realm = UM_REALM_OBJECT (source);
  GError *error = NULL;
  GBytes *creds;

  um_realm_login_finish (result, &creds, &error);
  if (error == NULL) {

    /* Already joined to the domain, just register this user */
    if (um_realm_is_configured (realm)) {
      g_debug ("Already joined to this realm");
      enterprise_permit_user_login (data, realm);

      /* Join the domain, try using the user's creds */
    } else if (!um_realm_join_as_user (realm,
                                       gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-login")),
                                       gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-password")),
                                       creds, NULL,
                                       on_realm_joined,
                                       data)) {

      /* If we can't do user auth, try to authenticate as admin */
      g_debug ("Cannot join with user credentials");

      /* XXX: creds */
      /* join_show_prompt (self, NULL); */
    }

    g_bytes_unref (creds);

    /* A problem with the user's login name or password */
  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN)) {
    g_debug ("Problem with the user's login: %s", error->message);
    set_entry_validation_error (OBJ (GtkEntry *, "enterprise-login"), error->message);
    gtk_widget_grab_focus (WID ("enterprise-login"));

  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
    g_debug ("Problem with the user's password: %s", error->message);
    set_entry_validation_error (OBJ (GtkEntry *, "enterprise-password"), error->message);
    gtk_widget_grab_focus (WID ("enterprise-password"));

    /* Other login failure */
  } else {
    show_error_dialog (data, _("Failed to log into domain"), error);
    g_message ("Couldn't log in as user: %s", error->message);
  }

  g_clear_error (&error);
}

static void
enterprise_check_login (AccountData *data, UmRealmObject *realm)
{
  g_assert (realm);

  um_realm_login (realm,
                  gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-login")),
                  gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-password")),
                  NULL,
                  on_realm_login,
                  data);
}

static void
on_realm_discover_input (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
  AccountData *data = user_data;
  GError *error = NULL;
  GList *realms;

  realms = um_realm_manager_discover_finish (data->realm_manager,
                                             result, &error);

  /* Found a realm, log user into domain */
  if (error == NULL) {
    UmRealmObject *realm;
    g_assert (realms != NULL);
    realm = g_object_ref (realms->data);
    enterprise_check_login (data, realm);
    g_list_free_full (realms, g_object_unref);

  } else {
    /* The domain is likely invalid */
    g_dbus_error_strip_remote_error (error);
    g_message ("Couldn't discover domain: %s", error->message);
    gtk_widget_grab_focus (WID ("enterprise-domain-entry"));
    set_entry_validation_error (OBJ (GtkEntry*, "enterprise-domain-entry"), error->message);
    g_error_free (error);
  }
}

static void
enterprise_add_user (AccountData *data)
{
  UmRealmObject *realm;
  GtkTreeIter iter;
  GtkComboBox *domain = OBJ(GtkComboBox*, "enterprise-domain");

  /* Already know about this realm, try to login as user */
  if (gtk_combo_box_get_active_iter (domain, &iter)) {
    gtk_tree_model_get (gtk_combo_box_get_model (domain),
                        &iter, 1, &realm, -1);
    enterprise_check_login (data, realm);

    /* Something the user typed, we need to discover realm */
  } else {
    um_realm_manager_discover (data->realm_manager,
                               gtk_entry_get_text (OBJ (GtkEntry*, "enterprise-domain-entry")),
                               NULL,
                               on_realm_discover_input,
                               data);
  }
}

static void
save_account_data (AccountData *data)
{
  switch (data->mode) {
  case UM_LOCAL:
    local_create_user (data);
    break;
  case UM_ENTERPRISE:
    enterprise_add_user (data);
    break;
  default:
    g_assert_not_reached ();
  }
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

static void
enterprise_add_realm (AccountData *data,
                      UmRealmObject *realm)
{
  GtkTreeIter iter;
  UmRealmCommon *common;
  GtkComboBox *domain = OBJ(GtkComboBox*, "enterprise-domain");
  GtkListStore *model = OBJ(GtkListStore*, "enterprise-realms-model");

  g_debug ("Adding new realm to drop down: %s",
           g_dbus_object_get_object_path (G_DBUS_OBJECT (realm)));

  common = um_realm_object_get_common (realm);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter,
                      0, um_realm_common_get_name (common),
                      1, realm,
                      -1);

  if (!data->domain_chosen && um_realm_is_configured (realm))
    gtk_combo_box_set_active_iter (domain, &iter);

  g_object_unref (common);
}

static void
on_manager_realm_added (UmRealmManager  *manager,
                        UmRealmObject   *realm,
                        gpointer         user_data)
{
  AccountData *data = user_data;
  enterprise_add_realm (data, realm);
}

static void
on_realm_manager_created (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
  AccountData *data = user_data;
  GError *error = NULL;
  GList *realms, *l;

  g_clear_object (&data->realm_manager);
  data->realm_manager = um_realm_manager_new_finish (result, &error);

  if (error != NULL) {
    g_warning ("Couldn't contact realmd service: %s", error->message);
    g_error_free (error);
    return;
  }

  /* Lookup all the realm objects */
  realms = um_realm_manager_get_realms (data->realm_manager);
  for (l = realms; l != NULL; l = g_list_next (l))
    enterprise_add_realm (data, l->data);

  g_list_free (realms);
  g_signal_connect (data->realm_manager, "realm-added",
                    G_CALLBACK (on_manager_realm_added), data);

  /* When no realms try to discover a sensible default, triggers realm-added signal */
  um_realm_manager_discover (data->realm_manager, "", NULL, NULL, NULL);
  set_has_enterprise (data, TRUE);
}

static void
on_realmd_appeared (GDBusConnection *connection,
                    const gchar *name,
                    const gchar *name_owner,
                    gpointer user_data)
{
  AccountData *data = user_data;
  um_realm_manager_new (NULL, on_realm_manager_created, data);
}

static void
on_realmd_disappeared (GDBusConnection *unused1,
                       const gchar *unused2,
                       gpointer user_data)
{
  AccountData *data = user_data;

  if (data->realm_manager != NULL) {
    g_signal_handlers_disconnect_by_func (data->realm_manager,
                                          on_manager_realm_added,
                                          data);
    g_clear_object (&data->realm_manager);
  }

  set_has_enterprise (data, FALSE);
}

static void
on_domain_changed (GtkComboBox *widget,
                   gpointer user_data)
{
  AccountData *data = user_data;
  data->domain_chosen = TRUE;
  update_account_page_status (data);
  clear_entry_validation_error (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))));
}

static void
on_entry_changed (GtkEditable *editable,
                  gpointer user_data)
{
  AccountData *data = user_data;
  update_account_page_status (data);
  clear_entry_validation_error (GTK_ENTRY (editable));
}

static void
on_local_toggle (GtkToggleButton *toggle,
                 gpointer         user_data)
{
  AccountData *data = user_data;
  if (gtk_toggle_button_get_active (toggle)) {
    set_mode (data, UM_LOCAL);
  }
}

static void
on_enterprise_toggle (GtkToggleButton *toggle,
                      gpointer         user_data)
{
  AccountData *data = user_data;
  if (gtk_toggle_button_get_active (toggle)) {
    set_mode (data, UM_ENTERPRISE);
  }
}

static void
next_page_cb (GisAssistant *assistant, GtkWidget *page, AccountData *data)
{
  if (page == data->widget)
    save_account_data (data);
}

void
gis_prepare_account_page (GisDriver *driver)
{
  GtkWidget *fullname_entry;
  GtkWidget *username_combo;
  GtkWidget *password_check;
  GtkWidget *admin_check;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  GtkWidget *local_account_avatar_button;
  AccountData *data = g_slice_new0 (AccountData);
  GisAssistant *assistant = gis_driver_get_assistant (driver);
  data->driver = driver;
  data->builder = gis_builder (PAGE_ID);
  data->widget = WID("account-page");

  gtk_widget_show (data->widget);

  data->realmd_watch = g_bus_watch_name (G_BUS_TYPE_SYSTEM, "org.freedesktop.realmd",
                                         G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                         on_realmd_appeared, on_realmd_disappeared,
                                         data, NULL);

  fullname_entry = WID("account-fullname-entry");
  username_combo = WID("account-username-combo");
  password_check = WID("account-password-check");
  admin_check = WID("account-admin-check");
  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");
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

  g_signal_connect (WID("enterprise-domain"), "changed",
                    G_CALLBACK (on_domain_changed), data);
  g_signal_connect (WID("enterprise-login"), "changed",
                    G_CALLBACK (on_entry_changed), data);
  g_signal_connect (WID("local-button"), "toggled",
                    G_CALLBACK (on_local_toggle), data);
  g_signal_connect (WID("enterprise-button"), "toggled",
                    G_CALLBACK (on_enterprise_toggle), data);

  data->act_client = act_user_manager_get_default ();

  gis_assistant_add_page (assistant, data->widget);
  gis_assistant_set_page_title (assistant, data->widget, _("Login"));

  g_signal_connect (assistant, "next-page", G_CALLBACK (next_page_cb), data);

  clear_account_page (data);
  update_account_page_status (data);

  data->has_enterprise = FALSE;

  /* force a refresh by setting to an invalid value */
  data->mode = NUM_MODES;
  set_mode (data, UM_LOCAL);
}
