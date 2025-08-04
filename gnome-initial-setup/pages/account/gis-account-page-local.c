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
#include <glib/gstdio.h>

#include <string.h>
#include <act/act-user-manager.h>
#include "um-utils.h"
#include "um-photo-dialog.h"

#include "gis-page-header.h"

#include <json-glib/json-glib.h>

/* Key and values that are written as metadata to the exported user avatar this
 * way it's possible to know how the image was initially created.
 * If set to generated we can regenerated the avatar when the style changes or
 * when the users full name changes. The other two values don't have a specific use yet */
#define IMAGE_SOURCE_KEY "tEXt::source"
#define IMAGE_SOURCE_VALUE_GENERATED "gnome-generated"
#define IMAGE_SOURCE_VALUE_FACE "gnome-face"
#define IMAGE_SOURCE_VALUE_CUSTOM "gnome-custom"
#define IMAGE_SIZE 512
#define VALIDATION_TIMEOUT 600

struct _GisAccountPageLocal
{
  AdwBin     parent;

  GtkWidget *avatar_button;
  GtkWidget *remove_avatar_button;
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

  ActUserManager *act_client;

  gboolean valid_name;
  gboolean valid_username;
  ActUserAccountType account_type;
};

G_DEFINE_TYPE (GisAccountPageLocal, gis_account_page_local, ADW_TYPE_BIN);

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

static void
update_avatar_text (GisAccountPageLocal *page)
{
  const gchar *name;
  name = gtk_editable_get_text (GTK_EDITABLE (page->fullname_entry));

  if (*name == '\0') {
    GtkWidget *entry = gtk_combo_box_get_child (GTK_COMBO_BOX (page->username_combo));

    name = gtk_editable_get_text (GTK_EDITABLE (entry));
  }

  if (*name == '\0') {
    name = NULL;
  }

  adw_avatar_set_text (ADW_AVATAR (page->avatar_image), name);
}

static gboolean
validate (GisAccountPageLocal *page)
{
  GtkWidget *entry;
  const gchar *name, *username;
  gboolean parental_controls_enabled;
  g_autofree gchar *tip = NULL;

  g_clear_handle_id (&page->timeout_id, g_source_remove);

  entry = gtk_combo_box_get_child (GTK_COMBO_BOX (page->username_combo));

  name = gtk_editable_get_text (GTK_EDITABLE (page->fullname_entry));
  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (page->username_combo));
#ifdef HAVE_PARENTAL_CONTROLS
  parental_controls_enabled = gtk_check_button_get_active (GTK_CHECK_BUTTON (page->enable_parental_controls_check_button));
#else
  parental_controls_enabled = FALSE;
#endif

  page->valid_name = is_valid_name (name);
  if (page->valid_name)
    set_entry_validation_checkmark (GTK_ENTRY (page->fullname_entry));

  page->valid_username = is_valid_username (username, parental_controls_enabled, &tip);
  if (page->valid_username)
    set_entry_validation_checkmark (GTK_ENTRY (entry));

  const gchar *current_label = gtk_label_get_text (GTK_LABEL (page->username_explanation));
  if (!g_str_equal (tip, current_label) && !g_str_equal (current_label, ""))
    gtk_accessible_announce (GTK_ACCESSIBLE (page), tip, GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
  gtk_label_set_text (GTK_LABEL (page->username_explanation), tip);

  validation_changed (page);

  return G_SOURCE_REMOVE;
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
  GtkWidget *entry;
  GtkTreeModel *model;
  const char *name;

  name = gtk_editable_get_text (GTK_EDITABLE (w));

  entry = gtk_combo_box_get_child (GTK_COMBO_BOX (page->username_combo));
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (page->username_combo));

  gtk_list_store_clear (GTK_LIST_STORE (model));

  if ((name == NULL || strlen (name) == 0) && !page->has_custom_username) {
    gtk_editable_set_text (GTK_EDITABLE (entry), "");
  }
  else if (name != NULL && strlen (name) != 0) {
    generate_username_choices (name, GTK_LIST_STORE (model));
    if (!page->has_custom_username)
      gtk_combo_box_set_active (GTK_COMBO_BOX (page->username_combo), 0);
  }

  clear_entry_validation_error (GTK_ENTRY (w));

  page->valid_name = FALSE;

  /* username_changed() is called consequently due to changes */

  update_avatar_text (page);
}

static void
username_changed (GtkComboBoxText     *combo,
                  GisAccountPageLocal *page)
{
  GtkWidget *entry;
  const gchar *username;

  entry = gtk_combo_box_get_child (GTK_COMBO_BOX (combo));
  username = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (*username == '\0')
    page->has_custom_username = FALSE;
  else if (gtk_widget_has_focus (entry) ||
           gtk_widget_get_focus_child (entry) ||
           gtk_combo_box_get_active (GTK_COMBO_BOX (page->username_combo)) > 0)
    page->has_custom_username = TRUE;

  clear_entry_validation_error (GTK_ENTRY (entry));

  page->valid_username = FALSE;
  validation_changed (page);

  if (page->timeout_id != 0)
    g_source_remove (page->timeout_id);
  page->timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate, page);

  update_avatar_text (page);
}

static void
on_remove_avatar_button_clicked (GisAccountPageLocal *page)
{
  adw_avatar_set_custom_image (ADW_AVATAR (page->avatar_image), NULL);
  gtk_widget_set_visible (GTK_WIDGET (page->remove_avatar_button), FALSE);
}

static void
avatar_callback (const gchar *filename,
                 gpointer     user_data)
{
  GisAccountPageLocal *page = user_data;
  g_autoptr(GdkTexture) texture = NULL;

  if (filename) {
    g_autoptr(GError) error = NULL;
    texture = gdk_texture_new_from_filename (filename, &error);

    if (error)
      g_warning ("Failed to load user icon from path %s: %s", filename, error->message);
  }

  adw_avatar_set_custom_image (ADW_AVATAR (page->avatar_image), GDK_PAINTABLE (texture));
  gtk_widget_set_visible (GTK_WIDGET (page->remove_avatar_button), texture != NULL);
}

static void
confirm (GisAccountPageLocal *page)
{
  if (gis_account_page_local_validate (page))
    g_signal_emit (page, signals[CONFIRM], 0);
}

static void
enable_parental_controls_check_button_toggled_cb (GtkCheckButton *check_button,
                                                  gpointer        user_data)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (user_data);
  gboolean parental_controls_enabled = gtk_check_button_get_active (GTK_CHECK_BUTTON (page->enable_parental_controls_check_button));

  /* This sets the account type of the main user. When we save_data(), we create
   * two users if parental controls are enabled: the first user is always an
   * admin, and the second user is the main user using this @account_type. */
  page->account_type = parental_controls_enabled ? ACT_USER_ACCOUNT_TYPE_STANDARD : ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;

  validate (page);
}

static void
track_focus_out (GisAccountPageLocal *page,
                 GtkWidget           *widget)
{
  GtkEventController *focus_controller;

  focus_controller = gtk_event_controller_focus_new ();
  gtk_widget_add_controller (widget, focus_controller);

  g_signal_connect_swapped (focus_controller, "leave", G_CALLBACK (on_focusout), page);
}


static void
gis_account_page_local_constructed (GObject *object)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (object);

  G_OBJECT_CLASS (gis_account_page_local_parent_class)->constructed (object);

  page->act_client = act_user_manager_get_default ();
  page->photo_dialog = um_photo_dialog_new (avatar_callback, page);

  g_signal_connect (page->fullname_entry, "notify::text",
                    G_CALLBACK (fullname_changed), page);
  track_focus_out (page, page->fullname_entry);

  g_signal_connect_swapped (page->fullname_entry, "activate",
                            G_CALLBACK (validate), page);
  g_signal_connect (page->username_combo, "changed",
                    G_CALLBACK (username_changed), page);
  track_focus_out (page, page->username_combo);

  g_signal_connect_swapped (gtk_combo_box_get_child (GTK_COMBO_BOX (page->username_combo)),
                            "activate", G_CALLBACK (confirm), page);
  g_signal_connect_swapped (page->fullname_entry, "activate",
                            G_CALLBACK (confirm), page);
  g_signal_connect (page->enable_parental_controls_check_button, "toggled",
                    G_CALLBACK (enable_parental_controls_check_button_toggled_cb), page);

  /* Disable parental controls if support is not compiled in. */
#ifndef HAVE_PARENTAL_CONTROLS
  gtk_widget_set_visible (page->enable_parental_controls_box, FALSE);
#endif

  page->valid_name = FALSE;
  page->valid_username = FALSE;

  /* FIXME: change this for a large deployment scenario; maybe through a GSetting? */
  page->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;

  g_object_set (page->header, "subtitle", _("We need a few details to complete setup."), NULL);
  gtk_editable_set_text (GTK_EDITABLE (page->fullname_entry), "");
  gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (page->username_combo))));
  page->has_custom_username = FALSE;

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (page->avatar_button),
                               GTK_WIDGET (page->photo_dialog));

  validate (page);
}

static void
gis_account_page_local_dispose (GObject *object)
{
  GisAccountPageLocal *page = GIS_ACCOUNT_PAGE_LOCAL (object);

  g_clear_handle_id (&page->timeout_id, g_source_remove);

  G_OBJECT_CLASS (gis_account_page_local_parent_class)->dispose (object);
}

/* This function was taken from AdwAvatar and modified so that it's possible to
 * export a GdkTexture at a different size than the AdwAvatar is rendered
 * See: https://gitlab.gnome.org/GNOME/libadwaita/-/blob/afd0fab86ff9b4332d165b985a435ea6f822d41b/src/adw-avatar.c#L751
 * License: LGPL-2.1-or-later */
static GdkTexture *
draw_avatar_to_texture (AdwAvatar *avatar, int size)
{
  GdkTexture *result;
  GskRenderNode *node;
  GtkSnapshot *snapshot;
  GdkPaintable *paintable;
  GtkNative *native;
  GskRenderer *renderer;
  int real_size;
  graphene_matrix_t transform;
  gboolean transform_ok;

  real_size = adw_avatar_get_size (avatar);

  /* This works around the issue that when the custom-image or text of the AdwAvatar changes the
   * allocation gets invalidateds and therefore we can't snapshot the widget till the allocation
   * is recalculated */
  gtk_widget_measure (GTK_WIDGET (avatar), GTK_ORIENTATION_HORIZONTAL, real_size, NULL, NULL, NULL, NULL);
  gtk_widget_allocate (GTK_WIDGET (avatar), real_size, real_size, -1, NULL);

  transform_ok = gtk_widget_compute_transform (GTK_WIDGET (avatar),
                                               gtk_widget_get_first_child (GTK_WIDGET (avatar)),
                                               &transform);

  g_assert (transform_ok);

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_transform_matrix (snapshot, &transform);
  GTK_WIDGET_GET_CLASS (avatar)->snapshot (GTK_WIDGET (avatar), snapshot);

  /* Create first a GdkPaintable at the size the avatar was drawn
   * then create a GdkSnapshot of it at the size requested */
  paintable = gtk_snapshot_free_to_paintable (snapshot, &GRAPHENE_SIZE_INIT (real_size, real_size));
  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, size, size);
  g_object_unref (paintable);

  node = gtk_snapshot_free_to_node (snapshot);

  native = gtk_widget_get_native (GTK_WIDGET (avatar));
  renderer = gtk_native_get_renderer (native);

  result = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (-1, 0, size, size));

  gsk_render_node_unref (node);

  return result;
}

static GdkPixbuf *
texture_to_pixbuf (GdkTexture *texture)
{
  g_autoptr(GdkTextureDownloader) downloader = NULL;
  g_autoptr(GBytes) bytes = NULL;
  gsize stride;

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8A8);
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);

  return gdk_pixbuf_new_from_bytes (bytes,
                                    GDK_COLORSPACE_RGB,
                                    true,
                                    8,
                                    gdk_texture_get_width (texture),
                                    gdk_texture_get_height (texture),
                                    stride);
}

static void
set_user_avatar (GisPage *page,
                 ActUser *user)
{
  GdkTexture *texture = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *path = NULL;
  const gchar *image_source;
  int fd;

  texture = gis_driver_get_avatar (page->driver);

  if (gis_driver_get_has_default_avatar (page->driver))
    image_source = IMAGE_SOURCE_VALUE_GENERATED;
  else
    image_source = IMAGE_SOURCE_VALUE_FACE;

  /* IMAGE_SOURCE_VALUE_CUSTOM isn't used here since we don't allow custom files
   * to be set during initial setup */

  fd = g_file_open_tmp ("usericonXXXXXX", &path, &error);

  if (fd == -1) {
    g_warning ("Failed to create temporary user icon: %s", error->message);
    return;
  }

  g_autoptr(GdkPixbuf) pixbuf = texture_to_pixbuf (texture);
  if (!gdk_pixbuf_save (pixbuf, path, "png", &error, IMAGE_SOURCE_KEY, image_source, NULL)) {
    g_warning ("Failed to save temporary user icon: %s", error->message);
  }

  close (fd);

  act_user_set_icon_file (user, path);

  g_remove (path);
}

static gboolean
local_create_user (GisAccountPageLocal  *local,
                   GisPage              *page,
                   GError              **error)
{
  const gchar *username;
  const gchar *fullname;
  gboolean parental_controls_enabled;
  g_autoptr(ActUser) main_user = NULL;
  g_autoptr(ActUser) parent_user = NULL;

  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (local->username_combo));
  fullname = gtk_editable_get_text (GTK_EDITABLE (local->fullname_entry));
  parental_controls_enabled = gis_driver_get_parental_controls_enabled (page->driver);

  /* Always create the admin user first, in case of failure part-way through
   * this function, which would leave us with no admin user at all. */
  if (parental_controls_enabled) {
    g_autoptr(GError) local_error = NULL;
    g_autoptr(GDBusConnection) connection = NULL;
    const gchar *parent_username = "administrator";
    const gchar *parent_fullname = _("Administrator");

    parent_user = act_user_manager_create_user (local->act_client, parent_username, parent_fullname, ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR, error);
    if (parent_user == NULL)
      {
        g_prefix_error (error,
                        _("Failed to create user '%s': "),
                        parent_username);
        return FALSE;
      }

    /* Make the admin account usable in case g-i-s crashes. If all goes
     * according to plan a password will be set on it in gis-password-page.c */
    act_user_set_password_mode (parent_user, ACT_USER_PASSWORD_MODE_SET_AT_LOGIN);

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
  main_user = act_user_manager_create_user (local->act_client, username, fullname, local->account_type, error);
  if (main_user == NULL)
    {
      g_prefix_error (error,
                      _("Failed to create user '%s': "),
                      username);
      /* FIXME: Could we delete the @parent_user at this point to reset the state
       * and allow g-i-s to be run again after a reboot? */
      return FALSE;
    }

  set_user_avatar (page, main_user);

  g_signal_emit (local, signals[MAIN_USER_CREATED], 0, main_user, "");

  return TRUE;
}

static void
gis_account_page_local_class_init (GisAccountPageLocalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page-local.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, avatar_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, remove_avatar_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, avatar_image);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, header);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, fullname_entry);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, username_combo);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, username_explanation);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, enable_parental_controls_box);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageLocal, enable_parental_controls_check_button);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), on_remove_avatar_button_clicked);


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
  g_object_set (G_OBJECT (page),
              "accessible-role",
              GTK_ACCESSIBLE_ROLE_GROUP,
              NULL);
}

gboolean
gis_account_page_local_validate (GisAccountPageLocal *page)
{
  return page->valid_name && page->valid_username;
}

gboolean
gis_account_page_local_create_user (GisAccountPageLocal  *local,
                                    GisPage              *page,
                                    GError              **error)
{
  return local_create_user (local, page, error);
}

gboolean
gis_account_page_local_apply (GisAccountPageLocal *local, GisPage *page)
{
  GisDriver *driver = GIS_PAGE (page)->driver;
  const gchar *username, *full_name;
  gboolean parental_controls_enabled;
  GdkTexture *texture = NULL;

  g_object_freeze_notify (G_OBJECT (driver));

  username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (local->username_combo));
  gis_driver_set_username (driver, username);

  full_name = gtk_editable_get_text (GTK_EDITABLE (local->fullname_entry));
  gis_driver_set_full_name (driver, full_name);

  texture = GDK_TEXTURE (adw_avatar_get_custom_image (ADW_AVATAR (local->avatar_image)));

  if (texture)
    {
      gis_driver_set_avatar (driver, texture);
      gis_driver_set_has_default_avatar (driver, FALSE);
    }
  else
    {
      texture = draw_avatar_to_texture (ADW_AVATAR (local->avatar_image), IMAGE_SIZE);
      gis_driver_set_avatar (driver, texture);
      gis_driver_set_has_default_avatar (driver, TRUE);
      g_object_unref (texture);
    }

#ifdef HAVE_PARENTAL_CONTROLS
  parental_controls_enabled = gtk_check_button_get_active (GTK_CHECK_BUTTON (local->enable_parental_controls_check_button));
#else
  parental_controls_enabled = FALSE;
#endif
  gis_driver_set_parental_controls_enabled (driver, parental_controls_enabled);

  g_object_thaw_notify (G_OBJECT (driver));

  return FALSE;
}

void
gis_account_page_local_shown (GisAccountPageLocal *local)
{
  gtk_widget_grab_focus (local->fullname_entry);
}
