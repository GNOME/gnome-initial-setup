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
#include "account-resources.h"
#include "gis-account-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <act/act-user-manager.h>

#include "um-realm-manager.h"
#include "um-utils.h"
#include "pw-utils.h"

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

G_DEFINE_TYPE (GisAccountPage, gis_account_page, GIS_TYPE_PAGE);

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_ACCOUNT_PAGE, GisAccountPagePrivate))

typedef enum {
  UM_LOCAL,
  UM_ENTERPRISE,
  NUM_MODES,
} UmAccountMode;

struct _GisAccountPagePrivate
{
  ActUser *act_user;
  ActUserManager *act_client;

  UmAccountMode mode;

  gboolean valid_name;
  gboolean valid_username;
  gboolean valid_confirm;
  const gchar *password_reason;
  guint reason_timeout;
  ActUserAccountType account_type;

  gboolean user_data_unsaved;

  gboolean has_enterprise;
  guint realmd_watch;
  UmRealmManager *realm_manager;
  gboolean domain_chosen;

  GtkWidget *action;
};

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE (page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
show_error_dialog (GisAccountPage *page,
                   const gchar *message,
                   GError *error)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
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
clear_account_page (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  GtkWidget *fullname_entry;
  GtkWidget *username_combo;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;

  fullname_entry = WID("account-fullname-entry");
  username_combo = WID("account-username-combo");
  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");

  priv->valid_name = FALSE;
  priv->valid_username = FALSE;
  priv->valid_confirm = FALSE;

  /* FIXME: change this for a large deployment scenario; maybe through a GSetting? */
  priv->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;

  priv->user_data_unsaved = FALSE;

  gtk_entry_set_text (GTK_ENTRY (fullname_entry), "");
  gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (username_combo))));
  gtk_entry_set_text (GTK_ENTRY (password_entry), "");
  gtk_entry_set_text (GTK_ENTRY (confirm_entry), "");
}

static gboolean
local_validate (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;

  return priv->valid_name &&
         priv->valid_username &&
         priv->valid_confirm;
}

static gboolean
enterprise_validate (GisAccountPage *page)
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
page_validate (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  switch (priv->mode) {
  case UM_LOCAL:
    return local_validate (page);
  case UM_ENTERPRISE:
    return enterprise_validate (page);
  default:
    g_assert_not_reached ();
  }
}

static void
update_account_page_status (GisAccountPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
set_mode (GisAccountPage *page,
          UmAccountMode   mode)
{
  GisAccountPagePrivate *priv = page->priv;
  GtkWidget *nb;

  if (priv->mode == mode)
    return;

  priv->mode = mode;

  nb = WID("account-notebook");
  gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), (mode == UM_LOCAL) ? 0 : 1);

  update_account_page_status (page);
}

static void
set_has_enterprise (GisAccountPage *page,
                    gboolean        has_enterprise)
{
  GisAccountPagePrivate *priv = page->priv;

  if (priv->has_enterprise == has_enterprise)
    return;

  priv->has_enterprise = has_enterprise;

  if (!has_enterprise)
    set_mode (page, UM_LOCAL);
}

static void
update_valid_confirm (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  const gchar *password, *verify;

  password = gtk_entry_get_text (GTK_ENTRY (WID("account-password-entry")));
  verify = gtk_entry_get_text (GTK_ENTRY (WID("account-confirm-entry")));

  priv->valid_confirm = strcmp (password, verify) == 0;
}

static void
fullname_changed (GtkWidget      *w,
                  GParamSpec     *pspec,
                  GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  GtkWidget *combo;
  GtkWidget *entry;
  GtkTreeModel *model;
  const char *name;

  name = gtk_entry_get_text (GTK_ENTRY (w));

  combo = WID("account-username-combo");
  entry = gtk_bin_get_child (GTK_BIN (combo));
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  gtk_list_store_clear (GTK_LIST_STORE (model));

  priv->valid_name = is_valid_name (name);
  priv->user_data_unsaved = TRUE;

  if (!priv->valid_name) {
    gtk_entry_set_text (GTK_ENTRY (entry), "");
    return;
  }

  generate_username_choices (name, GTK_LIST_STORE (model));

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  update_account_page_status (page);
}

static void
username_changed (GtkComboBoxText *combo,
                  GisAccountPage  *page)
{
  GisAccountPagePrivate *priv = page->priv;
  const gchar *username;
  gchar *tip;
  GtkWidget *entry;

  username = gtk_combo_box_text_get_active_text (combo);

  priv->valid_username = is_valid_username (username, &tip);
  priv->user_data_unsaved = TRUE;

  entry = gtk_bin_get_child (GTK_BIN (combo));

  if (tip) {
    set_entry_validation_error (GTK_ENTRY (entry), tip);
    g_free (tip);
  }
  else {
    clear_entry_validation_error (GTK_ENTRY (entry));
    /* We hit this the first time when there has been no change to password but
     * the two empty passwords are valid for no password.
     */
    update_valid_confirm (page);
  }

  update_account_page_status (page);
}

static gboolean
reason_timeout_cb (gpointer data)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (data);
  GisAccountPagePrivate *priv = page->priv;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  const gchar *password;
  const gchar *verify;

  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");
  password = gtk_entry_get_text (GTK_ENTRY (password_entry));
  verify = gtk_entry_get_text (GTK_ENTRY (confirm_entry));

  if (strlen (password) == 0)
    set_entry_validation_error (GTK_ENTRY (password_entry), _("No password"));
  else
    set_entry_validation_error (GTK_ENTRY (password_entry), priv->password_reason);

  if (strlen (verify) > 0 && !priv->valid_confirm)
    set_entry_validation_error (GTK_ENTRY (confirm_entry), _("Passwords do not match"));

  priv->reason_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
refresh_reason_timeout (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;

  if (priv->reason_timeout != 0)
    g_source_remove (priv->reason_timeout);

  priv->reason_timeout = g_timeout_add (600, reason_timeout_cb, page);
}

static void
update_password_entries (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  const gchar *password;
  const gchar *verify;
  const gchar *username;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  GtkWidget *username_combo;
  GtkWidget *password_strength;
  GtkWidget *strength_label;
  gdouble strength;
  gint strength_level;
  const gchar *hint;
  const gchar *long_hint = NULL;
  gchar *strength_hint;

  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");
  username_combo = WID("account-username-combo");
  password_strength = WID("account-password-strength");
  strength_label = WID("account-password-strength-label");

  password = gtk_entry_get_text (GTK_ENTRY (password_entry));
  verify = gtk_entry_get_text (GTK_ENTRY (confirm_entry));
  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (username_combo));

  strength = pw_strength (password, NULL, username, &hint, &long_hint, &strength_level);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (password_strength), strength_level);

  if (strlen (password) == 0)
    strength_hint = g_strdup ("");
  else
    strength_hint = g_strdup_printf (_("Strength: %s"), hint);
  gtk_label_set_label (GTK_LABEL (strength_label), strength_hint);
  g_free (strength_hint);

  if (strength == 0.0) {
    priv->password_reason = long_hint ? long_hint : hint;
  }

  update_valid_confirm (page);

  if (priv->valid_confirm)
    clear_entry_validation_error (GTK_ENTRY (password_entry));

  gtk_widget_set_sensitive (confirm_entry, TRUE);

  refresh_reason_timeout (page);
}

static void
password_changed (GtkWidget      *w,
                  GParamSpec     *pspec,
                  GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  clear_entry_validation_error (GTK_ENTRY (w));
  update_password_entries (page);
  priv->user_data_unsaved = TRUE;
  update_account_page_status (page);
}

static gboolean
password_entry_focus_out (GtkWidget      *widget,
                          GdkEventFocus  *event,
                          GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  GtkEntry *entry = GTK_ENTRY (widget);

  if (page->priv->reason_timeout != 0)
    g_source_remove (page->priv->reason_timeout);

  return FALSE;
}

static gboolean
confirm_entry_focus_out (GtkWidget      *widget,
                         GdkEventFocus  *event,
                         GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  GtkEntry *entry = GTK_ENTRY (widget);
  const gchar *verify;

  verify = gtk_entry_get_text (entry);

  if (strlen (verify) > 0 && !priv->valid_confirm)
    set_entry_validation_error (entry, _("Passwords do not match"));
  else
    clear_entry_validation_error (entry);

  return FALSE;
}

static void
create_user (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  const gchar *username;
  const gchar *fullname;
  const gchar *language;
  GError *error;

  username = gtk_combo_box_text_get_active_text (OBJ(GtkComboBoxText*, "account-username-combo"));
  fullname = gtk_entry_get_text (OBJ(GtkEntry*, "account-fullname-entry"));

  error = NULL;

  priv->act_user = act_user_manager_create_user (priv->act_client, username, fullname, priv->account_type, &error);
  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (priv->act_user, language);

  if (error != NULL) {
    g_warning ("Failed to create user: %s", error->message);
    g_error_free (error);
  }
  /* TODO:We don't support coming back to this page to modify the values after
   * the user has been created. For now just disable it
   */
  gtk_widget_set_sensitive (GTK_WIDGET (page), FALSE);
  gtk_widget_set_sensitive (priv->action, FALSE);
}

static void save_account_data (GisAccountPage *page);

static gulong when_loaded;

static void
save_when_loaded (ActUser        *user,
                  GParamSpec     *pspec,
                  GisAccountPage *page)
{
  g_signal_handler_disconnect (user, when_loaded);
  when_loaded = 0;

  save_account_data (page);
}

static void
local_create_user (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  const gchar *realname;
  const gchar *username;
  const gchar *password;

  if (!priv->user_data_unsaved) {
    return;
  }

  /* this can happen when going back */
  if (!priv->valid_name ||
      !priv->valid_username) {
    return;
  }

  if (priv->act_user == NULL) {
    create_user (page);
  }

  if (priv->act_user == NULL) {
    g_warning ("User creation failed");
    clear_account_page (page);
    return;
  }

  if (!act_user_is_loaded (priv->act_user)) {
    if (when_loaded == 0)
      when_loaded = g_signal_connect (priv->act_user, "notify::is-loaded",
                                      G_CALLBACK (save_when_loaded), page);
    return;
  }

  realname = gtk_entry_get_text (OBJ (GtkEntry*, "account-fullname-entry"));
  username = gtk_combo_box_text_get_active_text (OBJ (GtkComboBoxText*, "account-username-combo"));
  password = gtk_entry_get_text (OBJ (GtkEntry*, "account-password-entry"));

  act_user_set_real_name (priv->act_user, realname);
  act_user_set_user_name (priv->act_user, username);
  act_user_set_account_type (priv->act_user, priv->account_type);
  if (strlen (password) == 0)
    act_user_set_password_mode (priv->act_user, ACT_USER_PASSWORD_MODE_NONE);
  else
    act_user_set_password (priv->act_user, password, "");

  gis_driver_set_user_permissions (GIS_PAGE (page)->driver,
                                   priv->act_user,
                                   password);

  priv->user_data_unsaved = FALSE;
}

static void
on_permit_user_login (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
  GisAccountPage *page = user_data;
  GisAccountPagePrivate *priv = page->priv;

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

    priv->act_user = act_user_manager_cache_user (priv->act_client, login, NULL);

    g_free (login);
  } else {
    show_error_dialog (page, _("Failed to register account"), error);
    g_message ("Couldn't permit logins on account: %s", error->message);
    g_error_free (error);
  }
}

static void
enterprise_permit_user_login (GisAccountPage *page, UmRealmObject *realm)
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
                                            page);

  g_object_unref (common);
  g_free (login);
}

static void
on_realm_joined (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
  GisAccountPage *page = user_data;
  UmRealmObject *realm = UM_REALM_OBJECT (source);
  GError *error = NULL;

  um_realm_join_finish (realm, result, &error);

  /* Yay, joined the domain, register the user locally */
  if (error == NULL) {
    g_debug ("Joining realm completed successfully");
    enterprise_permit_user_login (page, realm);

    /* Credential failure while joining domain, prompt for admin creds */
  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN) ||
             g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
    g_debug ("Joining realm failed due to credentials");

    /* XXX */
    /* join_show_prompt (self, error); */

    /* Other failure */
  } else {
    show_error_dialog (page, _("Failed to join domain"), error);
    g_message ("Failed to join the domain: %s", error->message);
  }

  g_clear_error (&error);
}

static void
on_realm_login (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
  GisAccountPage *page = user_data;
  UmRealmObject *realm = UM_REALM_OBJECT (source);
  GError *error = NULL;
  GBytes *creds = NULL;

  um_realm_login_finish (result, &creds, &error);

  /*
   * User login is valid, but cannot authenticate right now (eg: user needs
   * to change password at next login etc.)
   */
  if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_CANNOT_AUTH)) {
    g_clear_error (&error);
    creds = NULL;
  }

  if (error == NULL) {

    /* Already joined to the domain, just register this user */
    if (um_realm_is_configured (realm)) {
      g_debug ("Already joined to this realm");
      enterprise_permit_user_login (page, realm);

      /* Join the domain, try using the user's creds */
    } else if (creds == NULL ||
               !um_realm_join_as_user (realm,
                                       gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-login")),
                                       gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-password")),
                                       creds, NULL,
                                       on_realm_joined,
                                       page)) {

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
    show_error_dialog (page, _("Failed to log into domain"), error);
    g_message ("Couldn't log in as user: %s", error->message);
  }

  g_clear_error (&error);
}

static void
enterprise_check_login (GisAccountPage *page, UmRealmObject *realm)
{
  g_assert (realm);

  um_realm_login (realm,
                  gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-login")),
                  gtk_entry_get_text (OBJ (GtkEntry *, "enterprise-password")),
                  NULL,
                  on_realm_login,
                  page);
}

static void
on_realm_discover_input (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
  GisAccountPage *page = user_data;
  GisAccountPagePrivate *priv = page->priv;
  GError *error = NULL;
  GList *realms;

  realms = um_realm_manager_discover_finish (priv->realm_manager,
                                             result, &error);

  /* Found a realm, log user into domain */
  if (error == NULL) {
    UmRealmObject *realm;
    g_assert (realms != NULL);
    realm = g_object_ref (realms->data);
    enterprise_check_login (page, realm);
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
enterprise_add_user (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  UmRealmObject *realm;
  GtkTreeIter iter;
  GtkComboBox *domain = OBJ(GtkComboBox*, "enterprise-domain");

  /* Already know about this realm, try to login as user */
  if (gtk_combo_box_get_active_iter (domain, &iter)) {
    gtk_tree_model_get (gtk_combo_box_get_model (domain),
                        &iter, 1, &realm, -1);
    enterprise_check_login (page, realm);

    /* Something the user typed, we need to discover realm */
  } else {
    um_realm_manager_discover (priv->realm_manager,
                               gtk_entry_get_text (OBJ (GtkEntry*, "enterprise-domain-entry")),
                               NULL,
                               on_realm_discover_input,
                               page);
  }
}

static void
save_account_data (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = page->priv;
  switch (priv->mode) {
  case UM_LOCAL:
    local_create_user (page);
    break;
  case UM_ENTERPRISE:
    enterprise_add_user (page);
    break;
  default:
    g_assert_not_reached ();
  }
}

static void
enterprise_add_realm (GisAccountPage *page,
                      UmRealmObject  *realm)
{
  GisAccountPagePrivate *priv = page->priv;
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

  if (!priv->domain_chosen && um_realm_is_configured (realm))
    gtk_combo_box_set_active_iter (domain, &iter);

  g_object_unref (common);
}

static void
on_manager_realm_added (UmRealmManager  *manager,
                        UmRealmObject   *realm,
                        gpointer         user_data)
{
  GisAccountPage *page = user_data;
  enterprise_add_realm (page, realm);
}

static void
on_realm_manager_created (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
  GisAccountPage *page = user_data;
  GisAccountPagePrivate *priv = page->priv;
  GError *error = NULL;
  GList *realms, *l;

  g_clear_object (&priv->realm_manager);
  priv->realm_manager = um_realm_manager_new_finish (result, &error);

  if (error != NULL) {
    g_warning ("Couldn't contact realmd service: %s", error->message);
    g_error_free (error);
    return;
  }

  /* Lookup all the realm objects */
  realms = um_realm_manager_get_realms (priv->realm_manager);
  for (l = realms; l != NULL; l = g_list_next (l))
    enterprise_add_realm (page, l->data);

  g_list_free (realms);
  g_signal_connect (priv->realm_manager, "realm-added",
                    G_CALLBACK (on_manager_realm_added), page);

  /* When no realms try to discover a sensible default, triggers realm-added signal */
  um_realm_manager_discover (priv->realm_manager, "", NULL, NULL, NULL);
  set_has_enterprise (page, TRUE);
}

static void
on_realmd_appeared (GDBusConnection *connection,
                    const gchar *name,
                    const gchar *name_owner,
                    gpointer user_data)
{
  GisAccountPage *page = user_data;
  um_realm_manager_new (NULL, on_realm_manager_created, page);
}

static void
on_realmd_disappeared (GDBusConnection *unused1,
                       const gchar *unused2,
                       gpointer user_data)
{
  GisAccountPage *page = user_data;
  GisAccountPagePrivate *priv = page->priv;

  if (priv->realm_manager != NULL) {
    g_signal_handlers_disconnect_by_func (priv->realm_manager,
                                          on_manager_realm_added,
                                          page);
    g_clear_object (&priv->realm_manager);
  }

  set_has_enterprise (page, FALSE);
}

static void
on_domain_changed (GtkComboBox *widget,
                   gpointer user_data)
{
  GisAccountPage *page = user_data;
  page->priv->domain_chosen = TRUE;
  update_account_page_status (page);
  clear_entry_validation_error (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))));
}

static void
on_entry_changed (GtkEditable *editable,
                  gpointer user_data)
{
  GisAccountPage *page = user_data;
  update_account_page_status (page);
  clear_entry_validation_error (GTK_ENTRY (editable));
}

static void
toggle_mode (GtkToggleButton *button,
             gpointer         user_data)
{
  set_mode (GIS_ACCOUNT_PAGE (user_data),
            gtk_toggle_button_get_active (button) ? UM_ENTERPRISE : UM_LOCAL);
}

static void
next_page_cb (GisAssistant *assistant,
              GisPage      *which_page,
              GisPage      *this_page)
{
  if (which_page == this_page)
    save_account_data (GIS_ACCOUNT_PAGE (this_page));
}

static void
gis_account_page_constructed (GObject *object)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (object);
  GisAccountPagePrivate *priv = page->priv;
  GisAssistant *assistant = gis_driver_get_assistant (GIS_PAGE (page)->driver);

  GtkWidget *fullname_entry;
  GtkWidget *username_combo;
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;

  G_OBJECT_CLASS (gis_account_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("account-page"));

  priv->realmd_watch = g_bus_watch_name (G_BUS_TYPE_SYSTEM, "org.freedesktop.realmd",
                                         G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                         on_realmd_appeared, on_realmd_disappeared,
                                         page, NULL);

  fullname_entry = WID("account-fullname-entry");
  username_combo = WID("account-username-combo");
  password_entry = WID("account-password-entry");
  confirm_entry = WID("account-confirm-entry");

  g_signal_connect (fullname_entry, "notify::text",
                    G_CALLBACK (fullname_changed), page);
  g_signal_connect (username_combo, "changed",
                    G_CALLBACK (username_changed), page);
  g_signal_connect (password_entry, "notify::text",
                    G_CALLBACK (password_changed), page);
  g_signal_connect (confirm_entry, "notify::text",
                    G_CALLBACK (password_changed), page);
  g_signal_connect_after (password_entry, "focus-out-event",
                          G_CALLBACK (password_entry_focus_out), page);
  g_signal_connect_after (confirm_entry, "focus-out-event",
                          G_CALLBACK (confirm_entry_focus_out), page);

  g_signal_connect (WID("enterprise-domain"), "changed",
                    G_CALLBACK (on_domain_changed), page);
  g_signal_connect (WID("enterprise-login"), "changed",
                    G_CALLBACK (on_entry_changed), page);

  priv->act_client = act_user_manager_get_default ();

  g_signal_connect (assistant, "next-page", G_CALLBACK (next_page_cb), page);

  clear_account_page (page);
  update_account_page_status (page);

  priv->has_enterprise = FALSE;

  priv->action = gtk_toggle_button_new_with_mnemonic ("_Use Enterprise Login");
  g_signal_connect (priv->action, "toggled", G_CALLBACK (toggle_mode), page);
  gtk_widget_show (priv->action);
  g_object_ref_sink (priv->action);

  /* force a refresh by setting to an invalid value */
  priv->mode = NUM_MODES;
  set_mode (page, UM_LOCAL);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_account_page_dispose (GObject *object)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (object);
  GisAccountPagePrivate *priv = page->priv;

  if (priv->realm_manager && priv->realmd_watch)
    g_bus_unwatch_name (priv->realmd_watch);
  g_clear_object (&priv->realm_manager);
  g_clear_object (&priv->action);

  G_OBJECT_CLASS (gis_account_page_parent_class)->dispose (object);
}

static GtkWidget *
gis_account_page_get_action_widget (GisPage *page)
{
  return GIS_ACCOUNT_PAGE (page)->priv->action;
}

static void
gis_account_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Login"));
}

static void
gis_account_page_class_init (GisAccountPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_account_page_locale_changed;
  page_class->get_action_widget = gis_account_page_get_action_widget;
  object_class->constructed = gis_account_page_constructed;
  object_class->dispose = gis_account_page_dispose;

  g_type_class_add_private (object_class, sizeof(GisAccountPagePrivate));
}

static void
gis_account_page_init (GisAccountPage *page)
{
  g_resources_register (account_get_resource ());
  page->priv = GET_PRIVATE (page);
}

void
gis_prepare_account_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_ACCOUNT_PAGE,
                                     "driver", driver,
                                     NULL));
}
