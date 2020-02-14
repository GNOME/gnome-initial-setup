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

#include "gis-page.h"
#include "gis-account-page-local.h"
#include "gnome-initial-setup.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <string.h>
#include <act/act-user-manager.h>
#include "um-utils.h"
#include "um-photo-dialog.h"

#include "gis-page-header.h"

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>

#include <rest/oauth-proxy.h>
#include <json-glib/json-glib.h>

#define VALIDATION_TIMEOUT 600

struct _GisAccountPageLocalPrivate
{
  GtkWidget *avatar_button;
  GtkWidget *avatar_image;
  GtkWidget *header;
  GtkWidget *fullname_entry;
  GtkWidget *username_combo;
  GtkWidget *enable_parental_controls_box;
  GtkWidget *enable_parental_controls_check_button;
  gboolean   has_custom_username;
  GtkWidget *username_explanation;
  UmPhotoDialog *photo_dialog;

  gint timeout_id;

  GdkPixbuf *avatar_pixbuf;
  gchar *avatar_filename;

  ActUserManager *act_client;

  GoaClient *goa_client;

  gboolean valid_name;
  gboolean valid_username;
  ActUserAccountType account_type;
};
typedef struct _GisAccountPageLocalPrivate GisAccountPageLocalPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisAccountPageLocal, gis_account_page_local, GTK_TYPE_BIN);

enum {
  VALIDATION_CHANGED,
  MAIN_USER_CREATED,
  PARENT_USER_CREATED,
  CONFIRM,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
validation_changed (GisAccountPageLocal *page)
{
  g_signal_emit (page, signals[VALIDATION_CHANGED], 0);
}

static gboolean
get_profile_sync (const gchar        *access_token,
                  gchar             **out_name,
                  gchar             **out_picture,
                  GCancellable       *cancellable,
                  GError            **error)
{
  GError *identity_error;
  RestProxy *proxy;
  RestProxyCall *call;
  JsonParser *parser;
  JsonObject *json_object;
  gboolean ret;

  ret = FALSE;

  identity_error = NULL;
  proxy = NULL;
  call = NULL;
  parser = NULL;

  /* TODO: cancellable */

  proxy = rest_proxy_new ("https://www.googleapis.com/oauth2/v2/userinfo", FALSE);
  call = rest_proxy_new_call (proxy);
  rest_proxy_call_set_method (call, "GET");
  rest_proxy_call_add_param (call, "access_token", access_token);

  if (!rest_proxy_call_sync (call, error))
    goto out;

  if (rest_proxy_call_get_status_code (call) != 200)
    {
      g_set_error (error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   "Expected status 200 when requesting your identity, instead got status %d (%s)",
                   rest_proxy_call_get_status_code (call),
                   rest_proxy_call_get_status_message (call));
      goto out;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   rest_proxy_call_get_payload (call),
                                   rest_proxy_call_get_payload_length (call),
                                   &identity_error))
    {
      g_warning ("json_parser_load_from_data() failed: %s (%s, %d)",
                   identity_error->message,
                   g_quark_to_string (identity_error->domain),
                   identity_error->code);
      g_set_error (error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   "Could not parse response");
      goto out;
    }

  ret = TRUE;

  json_object = json_node_get_object (json_parser_get_root (parser));
  if (out_name != NULL)
    *out_name = g_strdup (json_object_get_string_member (json_object, "name"));

  if (out_picture != NULL)
    *out_picture = g_strdup (json_object_get_string_member (json_object, "picture"));

 out:
  g_clear_error (&identity_error);
  if (call != NULL)
    g_object_unref (call);
  if (proxy != NULL)
    g_object_unref (proxy);

  return ret;
}

static void
prepopulate_account_page (GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  gchar *name = NULL;
  gchar *picture = NULL;
  GdkPixbuf *pixbuf = NULL;

  if (priv->goa_client) {
    GList *accounts, *l;
    accounts = goa_client_get_accounts (priv->goa_client);
    for (l = accounts; l != NULL; l = l->next) {
      GoaOAuth2Based *oa2;
      oa2 = goa_object_get_oauth2_based (GOA_OBJECT (l->data));
      if (oa2) {
        gchar *token = NULL;
        GError *error = NULL;
        if (!goa_oauth2_based_call_get_access_token_sync (oa2, &token, NULL, NULL, &error))
          {
            g_warning ("Couldn't get oauth2 token: %s", error->message);
            g_error_free (error);
          }
        else if (!get_profile_sync (token, &name, &picture, NULL, &error))
          {
            g_warning ("Couldn't get profile information: %s", error->message);
            g_error_free (error);
          }
        /* FIXME: collect information from more than one account
         * and present at least the pictures in the avatar chooser
         */
        break;
      }
    }
    g_list_free_full (accounts, (GDestroyNotify) g_object_unref);
  }

  if (name) {
    g_object_set (priv->header, "subtitle", _("Please check the name and username. You can choose a picture too."), NULL);
    gtk_entry_set_text (GTK_ENTRY (priv->fullname_entry), name);
  }

  if (picture) {
    GFile *file;
    GFileInputStream *stream;
    GError *error = NULL;
    file = g_file_new_for_uri (picture);
    stream = g_file_read (file, NULL, &error);
    if (!stream)
      {
        g_warning ("Failed to read picture %s: %s", picture, error->message);
        g_error_free (error);
      }
    else
      {
        pixbuf = gdk_pixbuf_new_from_stream_at_scale (G_INPUT_STREAM (stream), -1, 96, TRUE, NULL, NULL);
        g_object_unref (stream);
      }
    g_object_unref (file);
  }

  if (pixbuf) {
    GdkPixbuf *rounded = round_image (pixbuf);

    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->avatar_image), rounded);
    g_object_unref (rounded);
    priv->avatar_pixbuf = pixbuf;
  }

  g_free (name);
  g_free (picture);
}

static void
accounts_changed (GoaClient *client, GoaObject *object, gpointer data)
{
  GisAccountPageLocal *page = data;

  prepopulate_account_page (page);
}

static gboolean
validate (GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  GtkWidget *entry;
  const gchar *name, *username;
  gboolean parental_controls_enabled;
  gchar *tip;

  if (priv->timeout_id != 0) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  entry = gtk_bin_get_child (GTK_BIN (priv->username_combo));

  name = gtk_entry_get_text (GTK_ENTRY (priv->fullname_entry));
  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->username_combo));
#ifdef HAVE_PARENTAL_CONTROLS
  parental_controls_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->enable_parental_controls_check_button));
#else
  parental_controls_enabled = FALSE;
#endif

  priv->valid_name = is_valid_name (name);
  if (priv->valid_name)
    set_entry_validation_checkmark (GTK_ENTRY (priv->fullname_entry));

  priv->valid_username = is_valid_username (username, parental_controls_enabled, &tip);
  if (priv->valid_username)
    set_entry_validation_checkmark (GTK_ENTRY (entry));

  gtk_label_set_text (GTK_LABEL (priv->username_explanation), tip);
  g_free (tip);

  um_photo_dialog_generate_avatar (priv->photo_dialog, name);

  validation_changed (page);

  return FALSE;
}

static gboolean
on_focusout (GisAccountPageLocal *page)
{
  validate (page);

  return FALSE;
}

static void
fullname_changed (GtkWidget      *w,
                  GParamSpec     *pspec,
                  GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  GtkWidget *entry;
  GtkTreeModel *model;
  const char *name;

  name = gtk_entry_get_text (GTK_ENTRY (w));

  entry = gtk_bin_get_child (GTK_BIN (priv->username_combo));
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->username_combo));

  gtk_list_store_clear (GTK_LIST_STORE (model));

  if ((name == NULL || strlen (name) == 0) && !priv->has_custom_username) {
    gtk_entry_set_text (GTK_ENTRY (entry), "");
  }
  else if (name != NULL && strlen (name) != 0) {
    generate_username_choices (name, GTK_LIST_STORE (model));
    if (!priv->has_custom_username)
      gtk_combo_box_set_active (GTK_COMBO_BOX (priv->username_combo), 0);
  }

  clear_entry_validation_error (GTK_ENTRY (w));

  priv->valid_name = FALSE;

  /* username_changed() is called consequently due to changes */
}

static void
username_changed (GtkComboBoxText     *combo,
                  GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  GtkWidget *entry;
  const gchar *username;

  entry = gtk_bin_get_child (GTK_BIN (combo));
  username = gtk_entry_get_text (GTK_ENTRY (entry));
  if (*username == '\0')
    priv->has_custom_username = FALSE;
  else if (gtk_widget_has_focus (entry) ||
           gtk_combo_box_get_active (GTK_COMBO_BOX (priv->username_combo)) > 0)
    priv->has_custom_username = TRUE;

  clear_entry_validation_error (GTK_ENTRY (entry));

  priv->valid_username = FALSE;
  validation_changed (page);

  if (priv->timeout_id != 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate, page);
}

static void
avatar_callback (GdkPixbuf   *pixbuf,
                 const gchar *filename,
                 gpointer     user_data)
{
  GisAccountPageLocal *page = user_data;
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  g_autoptr(GdkPixbuf) tmp = NULL;
  g_autoptr(GdkPixbuf) rounded = NULL;

  g_clear_object (&priv->avatar_pixbuf);
  g_clear_pointer (&priv->avatar_filename, g_free);

  if (pixbuf) {
    priv->avatar_pixbuf = g_object_ref (pixbuf);
    rounded = round_image (pixbuf);
  }
  else if (filename) {
    priv->avatar_filename = g_strdup (filename);
    tmp = gdk_pixbuf_new_from_file_at_size (filename, 96, 96, NULL);

    if (tmp != NULL)
      rounded = round_image (tmp);
  }

  if (rounded != NULL) {
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->avatar_image), rounded);
  }
  else {
    /* Fallback. */
    gtk_image_set_pixel_size (GTK_IMAGE (priv->avatar_image), 96);
    gtk_image_set_from_icon_name (GTK_IMAGE (priv->avatar_image), "avatar-default-symbolic", 1);
  }
}

static void
confirm (GisAccountPageLocal *page)
{
  if (gis_account_page_local_validate (page))
    g_signal_emit (page, signals[CONFIRM], 0);
}

static void
enable_parental_controls_check_button_toggled_cb (GtkToggleButton *toggle_button,
                                                  gpointer         user_data)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (user_data);
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  gboolean parental_controls_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->enable_parental_controls_check_button));

  /* This sets the account type of the main user. When we save_data(), we create
   * two users if parental controls are enabled: the first user is always an
   * admin, and the second user is the main user using this @account_type. */
  priv->account_type = parental_controls_enabled ? ACT_USER_ACCOUNT_TYPE_STANDARD : ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;

  validate (page);
}

static void
gis_account_page_local_constructed (GObject *object)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (object);
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  GtkCssProvider *provider;

  G_OBJECT_CLASS (gis_account_page_local_parent_class)->constructed (object);

  priv->act_client = act_user_manager_get_default ();

  g_signal_connect (priv->fullname_entry, "notify::text",
                    G_CALLBACK (fullname_changed), page);
  g_signal_connect_swapped (priv->fullname_entry, "focus-out-event",
                            G_CALLBACK (on_focusout), page);
  g_signal_connect_swapped (priv->fullname_entry, "activate",
                            G_CALLBACK (validate), page);
  g_signal_connect (priv->username_combo, "changed",
                    G_CALLBACK (username_changed), page);
  g_signal_connect_swapped (priv->username_combo, "focus-out-event",
                            G_CALLBACK (on_focusout), page);
  g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->username_combo)),
                            "activate", G_CALLBACK (confirm), page);
  g_signal_connect_swapped (priv->fullname_entry, "activate",
                            G_CALLBACK (confirm), page);
  g_signal_connect (priv->enable_parental_controls_check_button, "toggled",
                    G_CALLBACK (enable_parental_controls_check_button_toggled_cb), page);

  /* Disable parental controls if support is not compiled in. */
#ifndef HAVE_PARENTAL_CONTROLS
  gtk_widget_hide (priv->enable_parental_controls_box);
#endif

  priv->valid_name = FALSE;
  priv->valid_username = FALSE;

  /* FIXME: change this for a large deployment scenario; maybe through a GSetting? */
  priv->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;

  g_object_set (priv->header, "subtitle", _("We need a few details to complete setup."), NULL);
  gtk_entry_set_text (GTK_ENTRY (priv->fullname_entry), "");
  gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->username_combo))));
  priv->has_custom_username = FALSE;

  gtk_image_set_pixel_size (GTK_IMAGE (priv->avatar_image), 96);
  gtk_image_set_from_icon_name (GTK_IMAGE (priv->avatar_image), "avatar-default-symbolic", 1);

  priv->goa_client = goa_client_new_sync (NULL, NULL);
  if (priv->goa_client) {
    g_signal_connect (priv->goa_client, "account-added",
                      G_CALLBACK (accounts_changed), page);
    g_signal_connect (priv->goa_client, "account-removed",
                      G_CALLBACK (accounts_changed), page);
    prepopulate_account_page (page);
  }

  priv->photo_dialog = um_photo_dialog_new (priv->avatar_button,
                                            avatar_callback,
                                            page);
  um_photo_dialog_generate_avatar (priv->photo_dialog, "");

  validate (page);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/initial-setup/gis-account-page-style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

static void
gis_account_page_local_dispose (GObject *object)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (object);
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);

  g_clear_object (&priv->goa_client);
  g_clear_object (&priv->avatar_pixbuf);
  g_clear_pointer (&priv->avatar_filename, g_free);

  if (priv->timeout_id != 0) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  G_OBJECT_CLASS (gis_account_page_local_parent_class)->dispose (object);
}

static void
set_user_avatar (GisAccountPageLocal *page,
                 ActUser             *user)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  GFile *file = NULL;
  GFileIOStream *io_stream = NULL;
  GOutputStream *stream = NULL;
  GError *error = NULL;

  if (priv->avatar_filename != NULL) {
    act_user_set_icon_file (user, priv->avatar_filename);
    return;
  }

  if (priv->avatar_pixbuf == NULL) {
    return;
  }

  file = g_file_new_tmp ("usericonXXXXXX", &io_stream, &error);
  if (error != NULL)
    goto out;

  stream = g_io_stream_get_output_stream (G_IO_STREAM (io_stream));
  if (!gdk_pixbuf_save_to_stream (priv->avatar_pixbuf, stream, "png", NULL, &error, NULL))
    goto out;

  act_user_set_icon_file (user, g_file_get_path (file));

 out:
  if (error != NULL) {
    g_warning ("failed to save image: %s", error->message);
    g_error_free (error);
  }
  g_clear_object (&io_stream);
  g_clear_object (&file);
}

static void
local_create_user (GisAccountPageLocal *local,
                   GisPage             *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (local);
  const gchar *username;
  const gchar *fullname;
  g_autoptr(GError) local_error = NULL;
  gboolean parental_controls_enabled;
  g_autoptr(ActUser) main_user = NULL;
  g_autoptr(ActUser) parent_user = NULL;

  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->username_combo));
  fullname = gtk_entry_get_text (GTK_ENTRY (priv->fullname_entry));
  parental_controls_enabled = gis_driver_get_parental_controls_enabled (page->driver);

  /* Always create the admin user first, in case of failure part-way through
   * this function, which would leave us with no admin user at all. */
  if (parental_controls_enabled) {
    g_autoptr(GDBusConnection) connection = NULL;
    const gchar *parent_username = "administrator";
    const gchar *parent_fullname = _("Administrator");

    parent_user = act_user_manager_create_user (priv->act_client, parent_username, parent_fullname, ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR, &local_error);
    if (local_error != NULL) {
      g_warning ("Failed to create parent user: %s", local_error->message);
      return;
    }

    /* Mark it as the parent user account.
     * FIXME: This should be async. */
    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &local_error);
    if (connection != NULL) {
      g_dbus_connection_call_sync (connection,
                                   "org.freedesktop.Accounts",
                                   act_user_get_object_path (parent_user),
                                   "org.freedesktop.DBus.Properties",
                                   "Set",
                                   g_variant_new ("(ssv)",
                                                  "com.endlessm.ParentalControls.AccountInfo",
                                                  "IsParent",
                                                  g_variant_new_boolean (TRUE)),
                                   NULL,  /* reply type */
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,  /* default timeout */
                                   NULL,  /* cancellable */
                                   &local_error);
    }
    if (local_error != NULL) {
      /* Make this non-fatal, since the correct accounts-service interface
       * might not be installed, depending on which version of malcontent is installed. */
      g_warning ("Failed to mark user as parent: %s", local_error->message);
      g_clear_error (&local_error);
    }

    g_signal_emit (local, signals[PARENT_USER_CREATED], 0, parent_user, "");
  }

  /* Now create the main user. */
  main_user = act_user_manager_create_user (priv->act_client, username, fullname, priv->account_type, &local_error);
  if (local_error != NULL) {
    g_warning ("Failed to create user: %s", local_error->message);
    return;
  }

  set_user_avatar (local, main_user);

  g_signal_emit (local, signals[MAIN_USER_CREATED], 0, main_user, "");
}

static void
gis_account_page_local_class_init (GisAccountPageLocalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page-local.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, avatar_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, avatar_image);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, header);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, fullname_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, username_combo);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, username_explanation);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, enable_parental_controls_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, enable_parental_controls_check_button);

  object_class->constructed = gis_account_page_local_constructed;
  object_class->dispose = gis_account_page_local_dispose;

  signals[VALIDATION_CHANGED] = g_signal_new ("validation-changed", GIS_TYPE_ACCOUNT_PAGE_LOCAL,
                                              G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                              G_TYPE_NONE, 0);

  signals[MAIN_USER_CREATED] = g_signal_new ("main-user-created", GIS_TYPE_ACCOUNT_PAGE_LOCAL,
                                             G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                             G_TYPE_NONE, 2, ACT_TYPE_USER, G_TYPE_STRING);

  signals[PARENT_USER_CREATED] = g_signal_new ("parent-user-created", GIS_TYPE_ACCOUNT_PAGE_LOCAL,
                                               G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                               G_TYPE_NONE, 2, ACT_TYPE_USER, G_TYPE_STRING);

  signals[CONFIRM] = g_signal_new ("confirm", GIS_TYPE_ACCOUNT_PAGE_LOCAL,
                                   G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                   G_TYPE_NONE, 0);
}

static void
gis_account_page_local_init (GisAccountPageLocal *page)
{
  g_type_ensure (GIS_TYPE_PAGE_HEADER);
  gtk_widget_init_template (GTK_WIDGET (page));
}

gboolean
gis_account_page_local_validate (GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);

  return priv->valid_name && priv->valid_username;
}

void
gis_account_page_local_create_user (GisAccountPageLocal *local,
                                    GisPage             *page)
{
  local_create_user (local, page);
}

gboolean
gis_account_page_local_apply (GisAccountPageLocal *local, GisPage *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (local);
  const gchar *username, *full_name;
  gboolean parental_controls_enabled;

  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->username_combo));
  gis_driver_set_username (GIS_PAGE (page)->driver, username);

  full_name = gtk_entry_get_text (GTK_ENTRY (priv->fullname_entry));
  gis_driver_set_full_name (GIS_PAGE (page)->driver, full_name);

  if (priv->avatar_pixbuf != NULL)
    {
      gis_driver_set_avatar (GIS_PAGE (page)->driver, priv->avatar_pixbuf);
    }
  else if (priv->avatar_filename != NULL)
    {
      g_autoptr(GdkPixbuf) pixbuf = NULL;

      pixbuf = gdk_pixbuf_new_from_file_at_size (priv->avatar_filename, 96, 96, NULL);
      gis_driver_set_avatar (GIS_PAGE (page)->driver, pixbuf);
    }

#ifdef HAVE_PARENTAL_CONTROLS
  parental_controls_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->enable_parental_controls_check_button));
#else
  parental_controls_enabled = FALSE;
#endif
  gis_driver_set_parental_controls_enabled (GIS_PAGE (page)->driver, parental_controls_enabled);

  return FALSE;
}

void
gis_account_page_local_shown (GisAccountPageLocal *local)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (local);
  gtk_widget_grab_focus (priv->fullname_entry);
}
