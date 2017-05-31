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

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>

#include <rest/oauth-proxy.h>
#include <json-glib/json-glib.h>

#define VALIDATION_TIMEOUT 600

struct _GisAccountPageLocalPrivate
{
  GtkWidget *avatar_button;
  GtkWidget *avatar_image;
  GtkWidget *subtitle;
  GtkWidget *fullname_entry;
  GtkWidget *username_combo;
  gboolean   has_custom_username;
  GtkWidget *username_explanation;
  UmPhotoDialog *photo_dialog;

  gint timeout_id;

  GdkPixbuf *avatar_pixbuf;
  gchar *avatar_filename;

  ActUser *act_user;
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
  USER_CREATED,
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
    gtk_label_set_text (GTK_LABEL (priv->subtitle), _("Please check the name and username. You can choose a picture too."));
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
        g_warning ("Failed to read picture: %s", error->message);
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
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->avatar_image), pixbuf);
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
  gchar *tip;

  if (priv->timeout_id != 0) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  entry = gtk_bin_get_child (GTK_BIN (priv->username_combo));

  name = gtk_entry_get_text (GTK_ENTRY (priv->fullname_entry));
  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->username_combo));

  priv->valid_name = is_valid_name (name);
  if (priv->valid_name)
    set_entry_validation_checkmark (GTK_ENTRY (priv->fullname_entry));

  priv->valid_username = is_valid_username (username, &tip);
  if (priv->valid_username)
    set_entry_validation_checkmark (GTK_ENTRY (entry));

  gtk_label_set_text (GTK_LABEL (priv->username_explanation), tip);
  g_free (tip);

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
  GdkPixbuf *tmp;

  g_clear_object (&priv->avatar_pixbuf);
  g_free (priv->avatar_filename);
  priv->avatar_filename = NULL;

  if (pixbuf) {
    priv->avatar_pixbuf = g_object_ref (pixbuf);
    tmp = gdk_pixbuf_scale_simple (pixbuf, 96, 96, GDK_INTERP_BILINEAR);
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->avatar_image), tmp);
    g_object_unref (tmp);
  }
  else if (filename) {
    priv->avatar_filename = g_strdup (filename);
    tmp = gdk_pixbuf_new_from_file_at_size (filename, 96, 96, NULL);
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->avatar_image), tmp);
    g_object_unref (tmp);
  }
  else {
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
gis_account_page_local_constructed (GObject *object)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (object);
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);

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

  priv->valid_name = FALSE;
  priv->valid_username = FALSE;

  /* FIXME: change this for a large deployment scenario; maybe through a GSetting? */
  priv->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;

  gtk_label_set_text (GTK_LABEL (priv->subtitle), _("We need a few details to complete setup."));
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

  validate (page);
}

static void
gis_account_page_local_dispose (GObject *object)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (object);
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);

  g_clear_object (&priv->goa_client);
  g_clear_object (&priv->avatar_pixbuf);
  g_clear_pointer (&priv->avatar_filename, g_free);
  g_clear_pointer (&priv->photo_dialog, um_photo_dialog_free);

  if (priv->timeout_id != 0) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  G_OBJECT_CLASS (gis_account_page_local_parent_class)->dispose (object);
}

static void
set_user_avatar (GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  GFile *file = NULL;
  GFileIOStream *io_stream = NULL;
  GOutputStream *stream = NULL;
  GError *error = NULL;

  if (priv->avatar_filename != NULL) {
    act_user_set_icon_file (priv->act_user, priv->avatar_filename);
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

  act_user_set_icon_file (priv->act_user, g_file_get_path (file));

 out:
  if (error != NULL) {
    g_warning ("failed to save image: %s", error->message);
    g_error_free (error);
  }
  g_clear_object (&io_stream);
  g_clear_object (&file);
}

static void
local_create_user (GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);
  const gchar *username;
  const gchar *fullname;
  GError *error = NULL;

  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->username_combo));
  fullname = gtk_entry_get_text (GTK_ENTRY (priv->fullname_entry));

  priv->act_user = act_user_manager_create_user (priv->act_client, username, fullname, priv->account_type, &error);
  if (error != NULL) {
    g_warning ("Failed to create user: %s", error->message);
    g_error_free (error);
    return;
  }

  act_user_set_user_name (priv->act_user, username);
  act_user_set_account_type (priv->act_user, priv->account_type);

  set_user_avatar (page);

  g_signal_emit (page, signals[USER_CREATED], 0, priv->act_user, "");
}

static void
gis_account_page_local_class_init (GisAccountPageLocalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page-local.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, avatar_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, avatar_image);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, subtitle);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, fullname_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, username_combo);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, username_explanation);

  object_class->constructed = gis_account_page_local_constructed;
  object_class->dispose = gis_account_page_local_dispose;

  signals[VALIDATION_CHANGED] = g_signal_new ("validation-changed", GIS_TYPE_ACCOUNT_PAGE_LOCAL,
                                              G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                              G_TYPE_NONE, 0);

  signals[USER_CREATED] = g_signal_new ("user-created", GIS_TYPE_ACCOUNT_PAGE_LOCAL,
                                        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                        G_TYPE_NONE, 2, ACT_TYPE_USER, G_TYPE_STRING);

  signals[CONFIRM] = g_signal_new ("confirm", GIS_TYPE_ACCOUNT_PAGE_LOCAL,
                                   G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                   G_TYPE_NONE, 0);
}

static void
gis_account_page_local_init (GisAccountPageLocal *page)
{
  gtk_widget_init_template (GTK_WIDGET (page));
}

gboolean
gis_account_page_local_validate (GisAccountPageLocal *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (page);

  return priv->valid_name && priv->valid_username;
}

void
gis_account_page_local_create_user (GisAccountPageLocal *page)
{
  local_create_user (page);
}

gboolean
gis_account_page_local_apply (GisAccountPageLocal *local, GisPage *page)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (local);
  const gchar *username;

  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->username_combo));
  gis_driver_set_username (GIS_PAGE (page)->driver, username);

  return FALSE;
}

void
gis_account_page_local_shown (GisAccountPageLocal *local)
{
  GisAccountPageLocalPrivate *priv = gis_account_page_local_get_instance_private (local);
  gtk_widget_grab_focus (priv->fullname_entry); 
}
