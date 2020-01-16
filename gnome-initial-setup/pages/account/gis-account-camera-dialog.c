/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2019 Red Hat
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
 *     Felipe Borges <felipeborges@gnome.org>
 */

#include <cheese/cheese-camera.h>
#include <cheese/cheese-widget.h>

#include "gis-account-camera-dialog.h"
#include "cc-crop-area.h"

struct _GisAccountCameraDialog
{
  GtkDialog parent;

  GtkStack     *headerbar_stack;
  GtkStack     *stack;
  CheeseWidget *camera_feed;
  CcCropArea   *crop_area;

  gulong photo_taken_id;

  SelectAvatarCallback *callback;
  gpointer              callback_data;
};

G_DEFINE_TYPE (GisAccountCameraDialog, gis_account_camera_dialog, GTK_TYPE_DIALOG)

#define CAMERA_FEED "camera-feed"
#define CROP_VIEW "crop-view"

static void
gis_account_camera_dialog_set_mode (GisAccountCameraDialog *self,
                                    const gchar            *mode)
{
  gtk_stack_set_visible_child_name (self->headerbar_stack, mode);
  gtk_stack_set_visible_child_name (self->stack, mode);
}

static void
on_take_another_button_clicked (GtkButton              *button,
                                GisAccountCameraDialog *self)
{
  gis_account_camera_dialog_set_mode (self, CAMERA_FEED);
  cc_crop_area_set_picture (CC_CROP_AREA (self->crop_area), NULL);
}

static void
cheese_widget_photo_taken_cb (CheeseCamera           *camera,
                              GdkPixbuf              *pixbuf,
                              GisAccountCameraDialog *self)
{
  cc_crop_area_set_picture (CC_CROP_AREA (self->crop_area), pixbuf);
}

static void
on_take_picture_button_clicked (GtkButton              *button,
                                GisAccountCameraDialog *self)
{
  GObject *camera;

  camera = cheese_widget_get_camera (self->camera_feed);

  if (self->photo_taken_id == 0) {
    self->photo_taken_id = g_signal_connect (camera, "photo-taken",
                                             G_CALLBACK (cheese_widget_photo_taken_cb),
                                             self);
  }

  if (cheese_camera_take_photo_pixbuf (CHEESE_CAMERA (camera))) {
    // fire the flash here
    gis_account_camera_dialog_set_mode (self, CROP_VIEW);
  } else {
    g_assert_not_reached ();
  }
}

static void
on_crop_done_button_clicked (GtkButton              *button,
                             GisAccountCameraDialog *self)
{
  GdkPixbuf *pixbuf = cc_crop_area_get_picture (CC_CROP_AREA (self->crop_area));

  gdk_pixbuf_save (pixbuf, "/tmp/photo", "png", NULL, NULL);

  self->callback (NULL, "/tmp/photo", self->callback_data);

  gtk_widget_hide (GTK_WIDGET (self));
}

GtkWidget *
gis_account_camera_dialog_new (SelectAvatarCallback callback,
                               gpointer             data)
{
  GisAccountCameraDialog *self;

  self = g_object_new (GIS_TYPE_ACCOUNT_CAMERA_DIALOG,
                       "use-header-bar", 1,
                       NULL);

  self->callback = callback;
  self->callback_data = data;

  return GTK_WIDGET (self);
}

static void
gis_account_camera_dialog_class_init (GisAccountCameraDialogClass *klass)
{
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (wclass, "/org/gnome/initial-setup/gis-account-camera-dialog.ui");

  gtk_widget_class_bind_template_child (wclass, GisAccountCameraDialog, headerbar_stack);
  gtk_widget_class_bind_template_child (wclass, GisAccountCameraDialog, stack);
  gtk_widget_class_bind_template_child (wclass, GisAccountCameraDialog, crop_area);
  gtk_widget_class_bind_template_child (wclass, GisAccountCameraDialog, camera_feed);

  gtk_widget_class_bind_template_callback (wclass, on_take_picture_button_clicked);
  gtk_widget_class_bind_template_callback (wclass, on_take_another_button_clicked);
  gtk_widget_class_bind_template_callback (wclass, on_crop_done_button_clicked);
}

static void
gis_account_camera_dialog_init (GisAccountCameraDialog *self)
{
  volatile GType type G_GNUC_UNUSED;

  /* register types that the builder needs */
  type = cc_crop_area_get_type ();

  gtk_widget_init_template (GTK_WIDGET (self));

  cc_crop_area_set_constrain_aspect (self->crop_area, TRUE);
}
