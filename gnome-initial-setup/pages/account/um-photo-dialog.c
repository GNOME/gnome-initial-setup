/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef HAVE_CHEESE
#include <cheese-avatar-chooser.h>
#include <cheese-camera-device.h>
#include <cheese-camera-device-monitor.h>
#endif /* HAVE_CHEESE */

#include "um-photo-dialog.h"
#include "um-utils.h"

#define ROW_SPAN 5
#define AVATAR_PIXEL_SIZE 72

struct _UmPhotoDialog {
        GtkPopover parent;

        GtkWidget *popup_button;
        GtkWidget *take_picture_button;
        GtkWidget *flowbox;

#ifdef HAVE_CHEESE
        CheeseCameraDeviceMonitor *monitor;
        guint num_cameras;
#endif /* HAVE_CHEESE */

        GListStore *faces;

        SelectAvatarCallback *callback;
        gpointer              data;
};

G_DEFINE_TYPE (UmPhotoDialog, um_photo_dialog, GTK_TYPE_POPOVER)

#ifdef HAVE_CHEESE
static gboolean
destroy_chooser (GtkWidget *chooser)
{
        gtk_widget_destroy (chooser);
        return FALSE;
}

static void
webcam_response_cb (GtkDialog     *dialog,
                    int            response,
                    UmPhotoDialog  *um)
{
        if (response == GTK_RESPONSE_ACCEPT) {
                GdkPixbuf *pb, *pb2;

                g_object_get (G_OBJECT (dialog), "pixbuf", &pb, NULL);
                pb2 = gdk_pixbuf_scale_simple (pb, 96, 96, GDK_INTERP_BILINEAR);

                um->callback (pb2, NULL, um->data);

                g_object_unref (pb2);
                g_object_unref (pb);
        }
        if (response != GTK_RESPONSE_DELETE_EVENT &&
            response != GTK_RESPONSE_NONE)
                g_idle_add ((GSourceFunc) destroy_chooser, dialog);
}

static void
webcam_icon_selected (UmPhotoDialog *um)
{
        GtkWidget *window;

        window = cheese_avatar_chooser_new ();
        gtk_window_set_transient_for (GTK_WINDOW (window),
                                      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (um))));
        gtk_window_set_modal (GTK_WINDOW (window), TRUE);
        g_signal_connect (G_OBJECT (window), "response",
                          G_CALLBACK (webcam_response_cb), um);
        gtk_widget_show (window);

        gtk_popover_popdown (GTK_POPOVER (um));
}

static void
update_photo_menu_status (UmPhotoDialog *um)
{
        gtk_widget_set_visible (um->take_picture_button, um->num_cameras != 0);
}

static void
device_added (CheeseCameraDeviceMonitor *monitor,
              CheeseCameraDevice        *device,
              UmPhotoDialog             *um)
{
        um->num_cameras++;
        update_photo_menu_status (um);
}

static void
device_removed (CheeseCameraDeviceMonitor *monitor,
                const char                *id,
                UmPhotoDialog             *um)
{
        um->num_cameras--;
        update_photo_menu_status (um);
}

#endif /* HAVE_CHEESE */

static void
face_widget_activated (GtkFlowBox      *flowbox,
                       GtkFlowBoxChild *child,
                       UmPhotoDialog   *um)
{
        const char *filename;
        GtkWidget  *image;

        image = gtk_bin_get_child (GTK_BIN (child));
        filename = g_object_get_data (G_OBJECT (image), "filename");

        um->callback (NULL, filename, um->data);

        gtk_popover_popdown (GTK_POPOVER (um));
}

static GtkWidget *
create_face_widget (gpointer item,
                    gpointer user_data)
{
        GtkWidget *image;
        GIcon *icon;

        icon = g_file_icon_new (G_FILE (item));
        image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size (GTK_IMAGE (image), AVATAR_PIXEL_SIZE);
        g_object_unref (icon);

        gtk_widget_show (image);

        g_object_set_data_full (G_OBJECT (image),
                                "filename", g_file_get_path (G_FILE (item)),
                                (GDestroyNotify) g_free);

        return image;
}

static void
setup_photo_popup (UmPhotoDialog *um)
{
        GFileType type;
        const gchar *target;
        const gchar * const * dirs;
        guint i;
        gboolean added_faces;

        um->faces = g_list_store_new (G_TYPE_FILE);
        gtk_flow_box_bind_model (GTK_FLOW_BOX (um->flowbox),
                                 G_LIST_MODEL (um->faces),
                                 create_face_widget,
                                 um,
                                 NULL);

        g_signal_connect (um->flowbox, "child-activated",
                          G_CALLBACK (face_widget_activated), um);

        dirs = g_get_system_data_dirs ();
        for (i = 0; dirs[i] != NULL; i++) {
                g_autoptr(GFileEnumerator) enumerator = NULL;
                g_autoptr(GFile) dir = NULL;
                g_autofree gchar *path = NULL;
                gpointer infoptr;

                path = g_build_filename (dirs[i], "pixmaps", "faces", NULL);
                dir = g_file_new_for_path (path);

                enumerator = g_file_enumerate_children (dir,
                                                        G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                                        G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                                                        G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                                        G_FILE_QUERY_INFO_NONE,
                                                        NULL, NULL);
                if (enumerator == NULL)
                        continue;

                while ((infoptr = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
                        g_autoptr (GFileInfo) info = infoptr;
                        g_autoptr (GFile) face_file = NULL;

                        added_faces = TRUE;

                        type = g_file_info_get_file_type (info);
                        if (type != G_FILE_TYPE_REGULAR && type != G_FILE_TYPE_SYMBOLIC_LINK)
                                continue;

                        target = g_file_info_get_symlink_target (info);
                        if (target != NULL && g_str_has_prefix (target , "legacy/"))
                                continue;

                        face_file = g_file_get_child (dir, g_file_info_get_name (info));
                        g_list_store_append (um->faces, face_file);
                }

                if (added_faces)
                        break;
        }

#ifdef HAVE_CHEESE
        gtk_widget_set_visible (um->take_picture_button, TRUE);

        um->monitor = cheese_camera_device_monitor_new ();
        g_signal_connect (G_OBJECT (um->monitor), "added",
                          G_CALLBACK (device_added), um);
        g_signal_connect (G_OBJECT (um->monitor), "removed",
                          G_CALLBACK (device_removed), um);
        cheese_camera_device_monitor_coldplug (um->monitor);
#endif /* HAVE_CHEESE */
}

static void
popup_icon_menu (GtkToggleButton *button, UmPhotoDialog *um)
{
        gtk_popover_popup (GTK_POPOVER (um));
}

static gboolean
on_popup_button_button_pressed (GtkToggleButton *button,
                                GdkEventButton *event,
                                UmPhotoDialog  *um)
{
        if (event->button == 1) {
                if (!gtk_widget_get_visible (GTK_WIDGET (um))) {
                        popup_icon_menu (button, um);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
                } else {
                        gtk_popover_popdown (GTK_POPOVER (um));
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
                }

                return TRUE;
        }

        return FALSE;
}

UmPhotoDialog *
um_photo_dialog_new (GtkWidget            *button,
                     SelectAvatarCallback  callback,
                     gpointer              data)
{
        UmPhotoDialog *um;

        um = g_object_new (UM_TYPE_PHOTO_DIALOG,
                           "relative-to", button,
                           NULL);

        /* Set up the popup */
        um->popup_button = button;
        setup_photo_popup (um);
        g_signal_connect (button, "toggled",
                          G_CALLBACK (popup_icon_menu), um);
        g_signal_connect (button, "button-press-event",
                          G_CALLBACK (on_popup_button_button_pressed), um);

        um->callback = callback;
        um->data = data;

        return um;
}

void
um_photo_dialog_dispose (GObject *object)
{
#ifdef HAVE_CHEESE
        UmPhotoDialog *um = UM_PHOTO_DIALOG (object);

        g_clear_object (&um->monitor);
#endif

        G_OBJECT_CLASS (um_photo_dialog_parent_class)->dispose (object);
}

static void
um_photo_dialog_init (UmPhotoDialog *um)
{
        gtk_widget_init_template (GTK_WIDGET (um));
}

static void
um_photo_dialog_class_init (UmPhotoDialogClass *klass)
{
        GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
        GObjectClass *oclass = G_OBJECT_CLASS (klass);

        gtk_widget_class_set_template_from_resource (wclass, "/org/gnome/initial-setup/gis-account-avatar-chooser.ui");

        gtk_widget_class_bind_template_child (wclass, UmPhotoDialog, flowbox);
        gtk_widget_class_bind_template_child (wclass, UmPhotoDialog, take_picture_button);
#ifdef HAVE_CHEESE
        gtk_widget_class_bind_template_callback (wclass, webcam_icon_selected);
#endif

        oclass->dispose = um_photo_dialog_dispose;
}
