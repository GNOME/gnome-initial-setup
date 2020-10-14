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
        GtkWidget *recent_pictures;

#ifdef HAVE_CHEESE
        CheeseCameraDeviceMonitor *monitor;
        GCancellable *cancellable;
        guint num_cameras;
#endif /* HAVE_CHEESE */

        GListStore *recent_faces;
        GListStore *faces;
        GFile *generated_avatar;
        gboolean custom_avatar_was_chosen;

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
                um->custom_avatar_was_chosen = TRUE;

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

static void
setup_cheese_camera_device_monitor (UmPhotoDialog *um)
{
        g_signal_connect (G_OBJECT (um->monitor), "added", G_CALLBACK (device_added), um);
        g_signal_connect (G_OBJECT (um->monitor), "removed", G_CALLBACK (device_removed), um);
        cheese_camera_device_monitor_coldplug (um->monitor);
}

static void
cheese_camera_device_monitor_new_cb (GObject *source,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
        UmPhotoDialog *um = user_data;
        GObject *ret;

        ret = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, NULL);
        if (ret == NULL)
                return;

        um->monitor = CHEESE_CAMERA_DEVICE_MONITOR (ret);
        setup_cheese_camera_device_monitor (um);
}
#else /* ! HAVE_CHEESE */
static void
webcam_icon_selected (UmPhotoDialog *um)
{
  g_warning ("Webcam icon selected, but compiled without Cheese support");
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
        um->custom_avatar_was_chosen = TRUE;

        gtk_popover_popdown (GTK_POPOVER (um));
}

static void
generated_avatar_activated (GtkFlowBox      *flowbox,
                            GtkFlowBoxChild *child,
                            UmPhotoDialog   *um)
{
        face_widget_activated (flowbox, child, um);
        um->custom_avatar_was_chosen = FALSE;
}

static GtkWidget *
create_face_widget (gpointer item,
                    gpointer user_data)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        GtkWidget *image;
        g_autofree gchar *path = g_file_get_path (G_FILE (item));

        pixbuf = gdk_pixbuf_new_from_file_at_size (path,
                                                   AVATAR_PIXEL_SIZE,
                                                   AVATAR_PIXEL_SIZE,
                                                   NULL);

        if (pixbuf != NULL)
                image = gtk_image_new_from_pixbuf (round_image (pixbuf));
        else
                image = gtk_image_new ();

        gtk_image_set_pixel_size (GTK_IMAGE (image), AVATAR_PIXEL_SIZE);

        gtk_widget_show (image);

        g_object_set_data_full (G_OBJECT (image),
                                "filename", g_steal_pointer (&path),
                                (GDestroyNotify) g_free);

        return image;
}

static GStrv
get_settings_facesdirs (void)
{
        g_autoptr(GSettingsSchema) schema = NULL;
        g_autoptr(GPtrArray) facesdirs = g_ptr_array_new ();
        g_autoptr(GSettings) settings = g_settings_new ("org.gnome.desktop.interface");
        g_auto(GStrv) settings_dirs = g_settings_get_strv (settings, "avatar-directories");

        if (settings_dirs != NULL) {
                int i;
                for (i = 0; settings_dirs[i] != NULL; i++) {
                        char *path = settings_dirs[i];
                        if (path != NULL && g_strcmp0 (path, "") != 0)
                                g_ptr_array_add (facesdirs, g_strdup (path));
                }
        }

        // NULL terminated array
        g_ptr_array_add (facesdirs, NULL);
        return (GStrv) g_steal_pointer (&facesdirs->pdata);
}

static GStrv
get_system_facesdirs (void)
{
        g_autoptr(GPtrArray) facesdirs = NULL;
        const char * const * data_dirs;
        int i;

        facesdirs = g_ptr_array_new ();

        data_dirs = g_get_system_data_dirs ();
        for (i = 0; data_dirs[i] != NULL; i++) {
                char *path = g_build_filename (data_dirs[i], "pixmaps", "faces", NULL);
                g_ptr_array_add (facesdirs, path);
        }

        // NULL terminated array
        g_ptr_array_add (facesdirs, NULL);
        return (GStrv) g_steal_pointer (&facesdirs->pdata);
}

static gboolean
add_faces_from_dirs (GListStore *faces, GStrv facesdirs, gboolean add_all)
{
        gboolean added_faces = FALSE;
        const gchar *target;
        int i;
        GFileType type;

        for (i = 0; facesdirs[i] != NULL; i++) {
                g_autoptr(GFileEnumerator) enumerator = NULL;
                g_autoptr(GFile) dir = NULL;
                const char *path = facesdirs[i];
                gpointer infoptr;

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

                        type = g_file_info_get_file_type (info);
                        if (type != G_FILE_TYPE_REGULAR && type != G_FILE_TYPE_SYMBOLIC_LINK)
                                continue;

                        target = g_file_info_get_symlink_target (info);
                        if (target != NULL && g_str_has_prefix (target , "legacy/"))
                                continue;

                        face_file = g_file_get_child (dir, g_file_info_get_name (info));
                        g_list_store_append (faces, face_file);
                        added_faces = TRUE;
                }

                g_file_enumerator_close (enumerator, NULL, NULL);

                if (added_faces && !add_all)
                        break;
        }
        return added_faces;
}

static void
setup_photo_popup (UmPhotoDialog *um)
{
        g_auto(GStrv) facesdirs;
        gboolean added_faces = FALSE;

        um->faces = g_list_store_new (G_TYPE_FILE);
        gtk_flow_box_bind_model (GTK_FLOW_BOX (um->flowbox),
                                 G_LIST_MODEL (um->faces),
                                 create_face_widget,
                                 um,
                                 NULL);

        g_signal_connect (um->flowbox, "child-activated",
                          G_CALLBACK (face_widget_activated), um);

        um->recent_faces = g_list_store_new (G_TYPE_FILE);
        gtk_flow_box_bind_model (GTK_FLOW_BOX (um->recent_pictures),
                                 G_LIST_MODEL (um->recent_faces),
                                 create_face_widget,
                                 um,
                                 NULL);
        g_signal_connect (um->recent_pictures, "child-activated",
                          G_CALLBACK (generated_avatar_activated), um);
        um->custom_avatar_was_chosen = FALSE;

        facesdirs = get_settings_facesdirs ();
        added_faces = add_faces_from_dirs (um->faces, facesdirs, TRUE);

        if (!added_faces) {
                facesdirs = get_system_facesdirs ();
                add_faces_from_dirs (um->faces, facesdirs, FALSE);
        }

#ifdef HAVE_CHEESE
        um->cancellable = g_cancellable_new ();
        g_async_initable_new_async (CHEESE_TYPE_CAMERA_DEVICE_MONITOR,
                                    G_PRIORITY_DEFAULT,
                                    um->cancellable,
                                    cheese_camera_device_monitor_new_cb,
                                    um,
                                    NULL);
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

void
um_photo_dialog_generate_avatar (UmPhotoDialog *um,
                                 const gchar   *name)
{
        cairo_surface_t *surface;
        gchar *filename;

        surface = generate_user_picture (name);

        /* Save into a tmp file that later gets copied by AccountsService */
        filename = g_build_filename (g_get_user_runtime_dir (), "avatar.png", NULL);
        um->generated_avatar = g_file_new_for_path (filename);
        cairo_surface_write_to_png (surface, g_file_get_path (um->generated_avatar));
        g_free (filename);

        /* Overwrite the first item */
        if (g_list_model_get_item (G_LIST_MODEL (um->recent_faces), 0) != NULL)
                g_list_store_remove (um->recent_faces, 0);

        g_list_store_insert (um->recent_faces, 0,
                             um->generated_avatar);
        gtk_widget_show_all (um->recent_pictures);

        if (!um->custom_avatar_was_chosen) {
                um->callback (NULL, g_file_get_path (um->generated_avatar), um->data);
        }
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

        g_cancellable_cancel (um->cancellable);
        g_clear_object (&um->cancellable);
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
        gtk_widget_class_bind_template_child (wclass, UmPhotoDialog, recent_pictures);
        gtk_widget_class_bind_template_child (wclass, UmPhotoDialog, take_picture_button);
        gtk_widget_class_bind_template_callback (wclass, webcam_icon_selected);

        oclass->dispose = um_photo_dialog_dispose;
}
