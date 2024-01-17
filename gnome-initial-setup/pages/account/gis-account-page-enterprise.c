/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Red Hat
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "gis-account-page-enterprise.h"
#include "gnome-initial-setup.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <act/act-user-manager.h>

#include "um-realm-manager.h"
#include "um-utils.h"

#include "gis-page-header.h"

static void        join_show_prompt    (GisAccountPageEnterprise *page,
                                        GError *error);

static void        on_join_login       (GObject *source,
                                        GAsyncResult *result,
                                        gpointer user_data);

static void        on_realm_joined     (GObject *source,
                                        GAsyncResult *result,
                                        gpointer user_data);

struct _GisAccountPageEnterprise
{
  AdwBin     parent;

  GtkWidget *header;
  GtkWidget *login;
  GtkWidget *password;
  GtkWidget *domain;
  GtkWidget *domain_entry;
  GtkTreeModel *realms_model;

  GtkWidget *join_dialog;
  GtkWidget *join_name;
  GtkWidget *join_password;
  GtkWidget *join_domain;
  GtkWidget *join_computer;

  ActUserManager *act_client;
  ActUser *act_user;

  guint realmd_watch;
  UmRealmManager *realm_manager;
  gboolean domain_chosen;

  /* Valid during apply */
  UmRealmObject *realm;
  GCancellable *cancellable;
  gboolean join_prompted;

  GisPageApplyCallback apply_complete_callback;
  gpointer apply_complete_data;
};

G_DEFINE_TYPE (GisAccountPageEnterprise, gis_account_page_enterprise, ADW_TYPE_BIN);

enum {
  VALIDATION_CHANGED,
  USER_CACHED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
clear_password_validation_error (GtkWidget *entry)
{
  gtk_widget_remove_css_class (entry, "error");
  gtk_widget_set_tooltip_text (entry, NULL);
}

static void
set_password_validation_error (GtkWidget   *entry,
                               const gchar *text)
{
  gtk_widget_add_css_class (entry, "error");
  gtk_widget_set_tooltip_text (entry, text);
}

static void
validation_changed (GisAccountPageEnterprise *page)
{
  g_signal_emit (page, signals[VALIDATION_CHANGED], 0);
}

static void
apply_complete (GisAccountPageEnterprise *page,
                gboolean                  valid)
{
  page->apply_complete_callback (NULL, valid, page->apply_complete_data);
}

static void
show_error_dialog (GisAccountPageEnterprise *page,
                   const gchar *message,
                   GError *error)
{
  GtkWidget *dialog;

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (page))),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", message);

  if (error != NULL) {
    g_dbus_error_strip_remote_error (error);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", error->message);
  }

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_window_destroy),
                    NULL);
  gtk_window_present (GTK_WINDOW (dialog));
}

gboolean
gis_account_page_enterprise_validate (GisAccountPageEnterprise *page)
{
  const gchar *name;
  gboolean valid_name;
  gboolean valid_domain;
  GtkTreeIter iter;

  name = gtk_editable_get_text (GTK_EDITABLE (page->login));
  valid_name = is_valid_name (name);

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (page->domain), &iter)) {
    gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (page->domain)),
                        &iter, 0, &name, -1);
  } else {
    name = gtk_editable_get_text (GTK_EDITABLE (page->domain_entry));
  }

  valid_domain = is_valid_name (name);
  return valid_name && valid_domain;
}

static void
on_cache_user (GObject *source,
               GAsyncResult *result,
               gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GError *error = NULL;

  page->act_user = act_user_manager_cache_user_finish (ACT_USER_MANAGER (source),
                                                       result,
                                                       &error);
  if (error != NULL) {
    show_error_dialog (page, _("Failed to cache account"), error);
    g_message ("Couldn't cache account: %s", error->message);
    g_error_free (error);
    apply_complete (page, FALSE);

    return;
  }

  act_user_set_account_type (page->act_user,
                             ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR);
  g_signal_emit (page,
                 signals[USER_CACHED],
                 0,
                 page->act_user,
                 gtk_editable_get_text (GTK_EDITABLE (page->password)));
  apply_complete (page, TRUE);
}

static void
on_permit_user_login (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
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
    login = um_realm_calculate_login (common, gtk_editable_get_text (GTK_EDITABLE (page->login)));
    g_return_if_fail (login != NULL);

    g_debug ("Caching remote user: %s", login);

    act_user_manager_cache_user_async (page->act_client,
                                       login,
                                       page->cancellable,
                                       on_cache_user,
                                       page);

    g_free (login);
  } else {
    show_error_dialog (page, _("Failed to register account"), error);
    g_message ("Couldn't permit logins on account: %s", error->message);
    g_error_free (error);
    apply_complete (page, FALSE);
  }
}

static void
enterprise_permit_user_login (GisAccountPageEnterprise *page, UmRealmObject *realm)
{
  UmRealmCommon *common;
  gchar *login;
  const gchar *add[2];
  const gchar *remove[1];
  GVariant *options;

  common = um_realm_object_get_common (realm);

  login = um_realm_calculate_login (common, gtk_editable_get_text (GTK_EDITABLE (page->login)));
  g_return_if_fail (login != NULL);

  add[0] = login;
  add[1] = NULL;
  remove[0] = NULL;

  g_debug ("Permitting login for: %s", login);
  options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

  um_realm_common_call_change_login_policy (common, "",
                                            add, remove, options,
                                            page->cancellable,
                                            on_permit_user_login,
                                            page);

  g_object_unref (common);
  g_free (login);
}

static void
on_set_static_hostname (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GError *error = NULL;
  GVariant *retval;

  retval = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
  if (error != NULL) {
    join_show_prompt (page, error);
    g_error_free (error);
    return;
  }

  g_variant_unref (retval);

  /* Prompted for some admin credentials, try to use them to log in */
  um_realm_login (page->realm,
                  gtk_editable_get_text (GTK_EDITABLE (page->join_name)),
                  gtk_editable_get_text (GTK_EDITABLE (page->join_password)),
                  page->cancellable, on_join_login, page);
}

static void
on_join_response (GtkDialog *dialog,
                  gint response,
                  gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GDBusConnection *connection;
  GError *error = NULL;
  gchar hostname[128];
  const gchar *name;

  gtk_widget_set_visible (GTK_WIDGET (dialog), FALSE);
  if (response != GTK_RESPONSE_OK) {
    apply_complete (page, FALSE);
    return;
  }

  name = gtk_editable_get_text (GTK_EDITABLE (page->join_computer));
  if (gethostname (hostname, sizeof (hostname)) == 0 &&
      !g_str_equal (name, hostname)) {
    g_debug ("Setting StaticHostname to '%s'", name);

    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, page->cancellable, &error);
    if (error != NULL) {
      apply_complete (page, FALSE);
      g_warning ("Could not get DBus connection: %s", error->message);
      g_error_free (error);
      return;
    }

    g_dbus_connection_call (connection, "org.freedesktop.hostname1",
                            "/org/freedesktop/hostname1", "org.freedesktop.hostname1",
                            "SetStaticHostname",
                            g_variant_new ("(sb)", name, TRUE),
                            G_VARIANT_TYPE ("()"),
                            G_DBUS_CALL_FLAGS_NONE,
                            G_MAXINT, NULL, on_set_static_hostname, page);

  } else {
    name = gtk_editable_get_text (GTK_EDITABLE (page->join_name));
    g_debug ("Logging in as admin user: %s", name);

    /* Prompted for some admin credentials, try to use them to log in */
    um_realm_login (page->realm, name,
                    gtk_editable_get_text (GTK_EDITABLE (page->join_password)),
                    NULL, on_join_login, page);
  }
}

static void
join_show_prompt (GisAccountPageEnterprise *page,
                  GError *error)
{
  UmRealmKerberosMembership *membership;
  UmRealmKerberos *kerberos;
  gchar hostname[128];
  const gchar *name;

  gtk_editable_set_text (GTK_EDITABLE (page->join_password), "");
  gtk_widget_grab_focus (GTK_WIDGET (page->join_password));

  kerberos = um_realm_object_get_kerberos (page->realm);
  membership = um_realm_object_get_kerberos_membership (page->realm);

  gtk_label_set_text (GTK_LABEL (page->join_domain),
                      um_realm_kerberos_get_domain_name (kerberos));

  if (gethostname (hostname, sizeof (hostname)) == 0)
    gtk_editable_set_text (GTK_EDITABLE (page->join_computer), hostname);

  clear_entry_validation_error (GTK_ENTRY (page->join_name));
  clear_password_validation_error (page->join_password);

  if (!page->join_prompted) {
    name = um_realm_kerberos_membership_get_suggested_administrator (membership);
    if (name && !g_str_equal (name, "")) {
      g_debug ("Suggesting admin user: %s", name);
      gtk_editable_set_text (GTK_EDITABLE (page->join_name), name);
    } else {
      gtk_widget_grab_focus (GTK_WIDGET (page->join_name));
    }

  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_HOSTNAME)) {
    g_debug ("Bad host name: %s", error->message);
    set_entry_validation_error (GTK_ENTRY (page->join_computer), error->message);

  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
    g_debug ("Bad admin password: %s", error->message);
    set_password_validation_error (page->join_password, error->message);

  } else {
    g_debug ("Admin login failure: %s", error->message);
    g_dbus_error_strip_remote_error (error);
    set_entry_validation_error (GTK_ENTRY (page->join_name), error->message);
  }

  g_debug ("Showing admin password dialog");
  gtk_window_set_transient_for (GTK_WINDOW (page->join_dialog),
                                GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (page))));
  gtk_window_set_modal (GTK_WINDOW (page->join_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (page->join_dialog));

  page->join_prompted = TRUE;
  g_object_unref (kerberos);
  g_object_unref (membership);

  /* And now we wait for on_join_response() */
}

static void
on_join_login (GObject *source,
               GAsyncResult *result,
               gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GError *error = NULL;
  GBytes *creds;

  um_realm_login_finish (page->realm, result, &creds, &error);

  /* Logged in as admin successfully, use creds to join domain */
  if (error == NULL) {
    if (!um_realm_join_as_admin (page->realm,
                                 gtk_editable_get_text (GTK_EDITABLE (page->join_name)),
                                 gtk_editable_get_text (GTK_EDITABLE (page->join_password)),
                                 creds, NULL, on_realm_joined, page)) {
      show_error_dialog (page, _("No supported way to authenticate with this domain"), NULL);
      g_message ("Authenticating as admin is not supported by the realm");
    }

    g_bytes_unref (creds);

  /* Couldn't login as admin, show prompt again */
  } else {
    join_show_prompt (page, error);
    g_message ("Couldn't log in as admin to join domain: %s", error->message);
    g_error_free (error);
  }
}

static void
on_realm_joined (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GError *error = NULL;

  um_realm_join_finish (page->realm, result, &error);

  /* Yay, joined the domain, register the user locally */
  if (error == NULL) {
    g_debug ("Joining realm completed successfully");
    enterprise_permit_user_login (page, page->realm);

    /* Credential failure while joining domain, prompt for admin creds */
  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN) ||
             g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD) ||
             g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_HOSTNAME)) {
    g_debug ("Joining realm failed due to credentials or host name");

    join_show_prompt (page, error);

    /* Other failure */
  } else {
    show_error_dialog (page, _("Failed to join domain"), error);
    g_message ("Failed to join the domain: %s", error->message);
    apply_complete (page, FALSE);
  }

  g_clear_error (&error);
}

static void
on_realm_login (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GError *error = NULL;
  GBytes *creds = NULL;

  um_realm_login_finish (page->realm, result, &creds, &error);

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
    if (um_realm_is_configured (page->realm)) {
      g_debug ("Already joined to this realm");
      enterprise_permit_user_login (page, page->realm);

      /* Join the domain, try using the user's creds */
    } else if (creds == NULL ||
               !um_realm_join_as_user (page->realm,
                                       gtk_editable_get_text (GTK_EDITABLE (page->login)),
                                       gtk_editable_get_text (GTK_EDITABLE (page->password)),
                                       creds, page->cancellable,
                                       on_realm_joined,
                                       page)) {

      /* If we can't do user auth, try to authenticate as admin */
      g_debug ("Cannot join with user credentials");

      join_show_prompt (page, error);
    }

    g_bytes_unref (creds);

    /* A problem with the user's login name or password */
  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN)) {
    g_debug ("Problem with the user's login: %s", error->message);
    set_entry_validation_error (GTK_ENTRY (page->login), error->message);
    gtk_widget_grab_focus (page->login);
    apply_complete (page, FALSE);

  } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
    g_debug ("Problem with the user's password: %s", error->message);
    set_password_validation_error (page->password, error->message);
    gtk_widget_grab_focus (page->password);
    apply_complete (page, FALSE);

    /* Other login failure */
  } else {
    show_error_dialog (page, _("Failed to log into domain"), error);
    g_message ("Couldn't log in as user: %s", error->message);
    apply_complete (page, FALSE);
  }

  g_clear_error (&error);
}

static void
enterprise_check_login (GisAccountPageEnterprise *page)
{

  g_assert (page->realm);

  um_realm_login (page->realm,
                  gtk_editable_get_text (GTK_EDITABLE (page->login)),
                  gtk_editable_get_text (GTK_EDITABLE (page->password)),
                  page->cancellable,
                  on_realm_login,
                  page);
}

static void
on_realm_discover_input (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GError *error = NULL;
  GList *realms;

  realms = um_realm_manager_discover_finish (page->realm_manager,
                                             result, &error);

  /* Found a realm, log user into domain */
  if (error == NULL) {
    g_assert (realms != NULL);
    page->realm = g_object_ref (realms->data);
    enterprise_check_login (page);
    g_list_free_full (realms, g_object_unref);

  } else {
    /* The domain is likely invalid */
    g_dbus_error_strip_remote_error (error);
    g_message ("Couldn't discover domain: %s", error->message);
    gtk_widget_grab_focus (page->domain_entry);
    set_entry_validation_error (GTK_ENTRY (page->domain_entry), error->message);
    apply_complete (page, FALSE);
    g_error_free (error);
  }
}

static void
enterprise_add_user (GisAccountPageEnterprise *page)
{
  GtkTreeIter iter;

  page->join_prompted = FALSE;
  g_clear_object (&page->realm);

  /* Already know about this realm, try to login as user */
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (page->domain), &iter)) {
    gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (page->domain)),
                        &iter, 1, &page->realm, -1);
    enterprise_check_login (page);

    /* Something the user typed, we need to discover realm */
  } else {
    um_realm_manager_discover (page->realm_manager,
                               gtk_editable_get_text (GTK_EDITABLE (page->domain_entry)),
                               page->cancellable,
                               on_realm_discover_input,
                               page);
  }
}

gboolean
gis_account_page_enterprise_apply (GisAccountPageEnterprise *page,
                                   GCancellable             *cancellable,
                                   GisPageApplyCallback      callback,
                                   gpointer                  data)
{
  GisPage *account_page = GIS_PAGE (data);

  /* Parental controls are not enterprise ready. It’s possible for them to have
   * been enabled if the user enabled them, applied the account-local page, and
   * then went back and decided to go all enterprise instead. */
  gis_driver_set_parental_controls_enabled (account_page->driver, FALSE);

  page->apply_complete_callback = callback;
  page->apply_complete_data = data;
  page->cancellable = g_object_ref (cancellable);
  enterprise_add_user (page);
  return TRUE;
}

static gchar *
realm_get_name (UmRealmObject *realm)
{
  UmRealmCommon *common;
  gchar *name;

  common = um_realm_object_get_common (realm);
  name = g_strdup (um_realm_common_get_name (common));
  g_object_unref (common);

  return name;
}

static gboolean
model_contains_realm (GtkTreeModel *model,
                      const gchar *realm_name)
{
  gboolean contains = FALSE;
  GtkTreeIter iter;
  gboolean match;
  gchar *name;
  gboolean ret;

  ret = gtk_tree_model_get_iter_first (model, &iter);
  while (ret) {
    gtk_tree_model_get (model, &iter, 0, &name, -1);
    match = (g_strcmp0 (name, realm_name) == 0);
    g_free (name);
    if (match) {
      g_debug ("ignoring duplicate realm: %s", realm_name);
      contains = TRUE;
      break;
    }
    ret = gtk_tree_model_iter_next (model, &iter);
  }

  return contains;
}

static void
enterprise_add_realm (GisAccountPageEnterprise *page,
                      UmRealmObject  *realm)
{
  GtkTreeIter iter;
  gchar *name;

  name = realm_get_name (realm);

  /*
   * Don't add a second realm if we already have one with this name.
   * Sometimes realmd returns two realms for the same name, if it has
   * different ways to use that realm. The first one that realmd
   * returns is the one it prefers.
   */

  if (!model_contains_realm (GTK_TREE_MODEL (page->realms_model), name)) {
    gtk_list_store_append (GTK_LIST_STORE (page->realms_model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (page->realms_model), &iter,
                        0, name,
                        1, realm,
                        -1);

    if (!page->domain_chosen && um_realm_is_configured (realm))
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (page->domain), &iter);

    g_debug ("added realm to drop down: %s %s", name,
             g_dbus_object_get_object_path (G_DBUS_OBJECT (realm)));
  }

  g_free (name);
}

static void
on_manager_realm_added (UmRealmManager  *manager,
                        UmRealmObject   *realm,
                        gpointer         user_data)
{
  GisAccountPageEnterprise *page = user_data;
  enterprise_add_realm (page, realm);
}

static void
on_realm_manager_created (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  GError *error = NULL;
  GList *realms, *l;

  g_clear_object (&page->realm_manager);
  page->realm_manager = um_realm_manager_new_finish (result, &error);

  if (error != NULL) {
    g_warning ("Couldn't contact realmd service: %s", error->message);
    g_error_free (error);
    return;
  }

  /* Lookup all the realm objects */
  realms = um_realm_manager_get_realms (page->realm_manager);
  for (l = realms; l != NULL; l = g_list_next (l))
    enterprise_add_realm (page, l->data);

  g_list_free (realms);
  g_signal_connect (page->realm_manager, "realm-added",
                    G_CALLBACK (on_manager_realm_added), page);

  /* When no realms try to discover a sensible default, triggers realm-added signal */
  um_realm_manager_discover (page->realm_manager, "", NULL, NULL, NULL);
  gtk_widget_set_visible (GTK_WIDGET (page), TRUE);
}

static void
on_realmd_appeared (GDBusConnection *connection,
                    const gchar *name,
                    const gchar *name_owner,
                    gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  um_realm_manager_new (NULL, on_realm_manager_created, page);
}

static void
on_realmd_disappeared (GDBusConnection *unused1,
                       const gchar *unused2,
                       gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;

  if (page->realm_manager != NULL) {
    g_signal_handlers_disconnect_by_func (page->realm_manager,
                                          on_manager_realm_added,
                                          page);
    g_clear_object (&page->realm_manager);
  }

  gtk_widget_set_visible (GTK_WIDGET (page), FALSE);
}

static void
on_domain_changed (GtkComboBox *widget,
                   gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;

  page->domain_chosen = TRUE;
  validation_changed (page);
  clear_entry_validation_error (GTK_ENTRY (gtk_combo_box_get_child (widget)));
}

static void
on_entry_changed (GtkEditable *editable,
                  gpointer user_data)
{
  GisAccountPageEnterprise *page = user_data;
  validation_changed (page);
  clear_entry_validation_error (GTK_ENTRY (editable));
}

static void
on_password_changed (GtkEditable *editable,
                     gpointer user_data)
{
  clear_password_validation_error (GTK_WIDGET (editable));
}

static void
gis_account_page_enterprise_realize (GtkWidget *widget)
{
  GisAccountPageEnterprise *page = GIS_ACCOUNT_PAGE_ENTERPRISE (widget);
  GtkWidget *gis_page;

  gis_page = gtk_widget_get_ancestor (widget, GIS_TYPE_PAGE);
  g_object_bind_property (gis_page, "small-screen",
                          page->header, "show-icon",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  GTK_WIDGET_CLASS (gis_account_page_enterprise_parent_class)->realize (widget);
}

static void
gis_account_page_enterprise_constructed (GObject *object)
{
  GisAccountPageEnterprise *page = GIS_ACCOUNT_PAGE_ENTERPRISE (object);

  G_OBJECT_CLASS (gis_account_page_enterprise_parent_class)->constructed (object);

  page->act_client = act_user_manager_get_default ();

  page->realmd_watch = g_bus_watch_name (G_BUS_TYPE_SYSTEM, "org.freedesktop.realmd",
                                         G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                         on_realmd_appeared, on_realmd_disappeared,
                                         page, NULL);

  g_signal_connect (page->join_dialog, "response",
                    G_CALLBACK (on_join_response), page);
  g_signal_connect (page->domain, "changed",
                    G_CALLBACK (on_domain_changed), page);
  g_signal_connect (page->login, "changed",
                    G_CALLBACK (on_entry_changed), page);
  g_signal_connect (page->password, "changed",
                    G_CALLBACK (on_password_changed), page);
  g_signal_connect (page->join_password, "changed",
                    G_CALLBACK (on_password_changed), page);
}

static void
gis_account_page_enterprise_dispose (GObject *object)
{
  GisAccountPageEnterprise *page = GIS_ACCOUNT_PAGE_ENTERPRISE (object);

  if (page->realmd_watch)
    g_bus_unwatch_name (page->realmd_watch);

  page->realmd_watch = 0;

  g_cancellable_cancel (page->cancellable);

  g_clear_object (&page->realm_manager);
  g_clear_object (&page->realm);
  g_clear_object (&page->cancellable);

  G_OBJECT_CLASS (gis_account_page_enterprise_parent_class)->dispose (object);
}

static void
gis_account_page_enterprise_class_init (GisAccountPageEnterpriseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gis_account_page_enterprise_constructed;
  object_class->dispose = gis_account_page_enterprise_dispose;

  widget_class->realize = gis_account_page_enterprise_realize;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page-enterprise.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, login);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, password);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, domain);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, realms_model);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, header);

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, join_dialog);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, join_name);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, join_password);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, join_domain);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEnterprise, join_computer);

  signals[VALIDATION_CHANGED] = g_signal_new ("validation-changed", GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE,
                                              G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                              G_TYPE_NONE, 0);
  signals[USER_CACHED] = g_signal_new ("user-cached", GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE,
                                        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                        G_TYPE_NONE, 2, ACT_TYPE_USER, G_TYPE_STRING);
}

static void
gis_account_page_enterprise_init (GisAccountPageEnterprise *page)
{
  g_type_ensure (GIS_TYPE_PAGE_HEADER);
  gtk_widget_init_template (GTK_WIDGET (page));

  page->domain_entry = gtk_combo_box_get_child (GTK_COMBO_BOX (page->domain));
}

void
gis_account_page_enterprise_shown (GisAccountPageEnterprise *page)
{
  gtk_widget_grab_focus (page->domain_entry);
}
