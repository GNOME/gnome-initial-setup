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

#define ROW_SPAN 6

struct _UmPhotoDialog {
        GtkWidget *photo_popup;
        GtkWidget *popup_button;

#ifdef HAVE_CHEESE
        CheeseCameraDeviceMonitor *monitor;
        GtkWidget *take_photo_menuitem;
        guint num_cameras;
#endif /* HAVE_CHEESE */

        SelectAvatarCallback *callback;
        gpointer              data;
};

static void
none_icon_selected (GtkMenuItem   *menuitem,
                    UmPhotoDialog *um)
{
        um->callback (NULL, NULL, um->data);
}

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
webcam_icon_selected (GtkMenuItem   *menuitem,
                      UmPhotoDialog *um)
{
        GtkWidget *window;

        window = cheese_avatar_chooser_new ();
        gtk_window_set_transient_for (GTK_WINDOW (window),
                                      GTK_WINDOW (gtk_widget_get_toplevel (um->popup_button)));
        gtk_window_set_modal (GTK_WINDOW (window), TRUE);
        g_signal_connect (G_OBJECT (window), "response",
                          G_CALLBACK (webcam_response_cb), um);
        gtk_widget_show (window);
}

static void
update_photo_menu_status (UmPhotoDialog *um)
{
        if (um->num_cameras == 0)
                gtk_widget_set_sensitive (um->take_photo_menuitem, FALSE);
        else
                gtk_widget_set_sensitive (um->take_photo_menuitem, TRUE);
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
stock_icon_selected (GtkMenuItem   *menuitem,
                     UmPhotoDialog *um)
{
        const char *filename;

        filename = g_object_get_data (G_OBJECT (menuitem), "filename");
        um->callback (NULL, filename, um->data);
}

static GtkWidget *
menu_item_for_filename (UmPhotoDialog *um,
                        const char    *filename)
{
        GtkWidget *image, *menuitem;
        GFile *file;
        GIcon *icon;

        file = g_file_new_for_path (filename);
        icon = g_file_icon_new (file);
        g_object_unref (file);
        image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
        g_object_unref (icon);

        menuitem = gtk_menu_item_new ();
        gtk_container_add (GTK_CONTAINER (menuitem), image);
        gtk_widget_show_all (menuitem);

        g_object_set_data_full (G_OBJECT (menuitem), "filename",
                                g_strdup (filename), (GDestroyNotify) g_free);
        g_signal_connect (G_OBJECT (menuitem), "activate",
                          G_CALLBACK (stock_icon_selected), um);

        return menuitem;
}

static void
setup_photo_popup (UmPhotoDialog *um)
{
        GtkWidget *menu, *menuitem, *image;
        guint x, y;
        const gchar * const * dirs;
        guint i;
        GDir *dir;
        const char *face;
        gboolean none_item_shown;
        gboolean added_faces;

        menu = gtk_menu_new ();

        x = 0;
        y = 0;
        none_item_shown = added_faces = FALSE;

        dirs = g_get_system_data_dirs ();
        for (i = 0; dirs[i] != NULL; i++) {
                char *path;

                path = g_build_filename (dirs[i], "pixmaps", "faces", NULL);
                dir = g_dir_open (path, 0, NULL);
                if (dir == NULL) {
                        g_free (path);
                        continue;
                }

                while ((face = g_dir_read_name (dir)) != NULL) {
                        char *filename;

                        added_faces = TRUE;

                        filename = g_build_filename (path, face, NULL);
                        menuitem = menu_item_for_filename (um, filename);
                        g_free (filename);
                        if (menuitem == NULL)
                                continue;

                        gtk_menu_attach (GTK_MENU (menu), GTK_WIDGET (menuitem),
                                         x, x + 1, y, y + 1);
                        gtk_widget_show (menuitem);

                        x++;
                        if (x >= ROW_SPAN - 1) {
                                y++;
                                x = 0;
                        }
                }
                g_dir_close (dir);
                g_free (path);

                if (added_faces)
                        break;
        }

        if (!added_faces)
                goto skip_faces;

        image = gtk_image_new_from_icon_name ("avatar-default", GTK_ICON_SIZE_DIALOG);
        menuitem = gtk_menu_item_new ();
        gtk_container_add (GTK_CONTAINER (menuitem), image);
        gtk_widget_show_all (menuitem);
        gtk_menu_attach (GTK_MENU (menu), GTK_WIDGET (menuitem),
                         x, x + 1, y, y + 1);
        g_signal_connect (G_OBJECT (menuitem), "activate",
                          G_CALLBACK (none_icon_selected), um);
        gtk_widget_show (menuitem);
        none_item_shown = TRUE;
        y++;

skip_faces:
        if (!none_item_shown) {
                menuitem = gtk_menu_item_new_with_label (_("Disable image"));
                gtk_menu_attach (GTK_MENU (menu), GTK_WIDGET (menuitem),
                                 0, ROW_SPAN - 1, y, y + 1);
                g_signal_connect (G_OBJECT (menuitem), "activate",
                                  G_CALLBACK (none_icon_selected), um);
                gtk_widget_show (menuitem);
                y++;
        }

        /* Separator */
        menuitem = gtk_separator_menu_item_new ();
        gtk_menu_attach (GTK_MENU (menu), GTK_WIDGET (menuitem),
                         0, ROW_SPAN - 1, y, y + 1);
        gtk_widget_show (menuitem);

        y++;

#ifdef HAVE_CHEESE
        um->take_photo_menuitem = gtk_menu_item_new_with_label (_("Take a photo…"));
        gtk_menu_attach (GTK_MENU (menu), GTK_WIDGET (um->take_photo_menuitem),
                         0, ROW_SPAN - 1, y, y + 1);
        g_signal_connect (G_OBJECT (um->take_photo_menuitem), "activate",
                          G_CALLBACK (webcam_icon_selected), um);
        gtk_widget_set_sensitive (um->take_photo_menuitem, FALSE);
        gtk_widget_show (um->take_photo_menuitem);

        um->monitor = cheese_camera_device_monitor_new ();
        g_signal_connect (G_OBJECT (um->monitor), "added",
                          G_CALLBACK (device_added), um);
        g_signal_connect (G_OBJECT (um->monitor), "removed",
                          G_CALLBACK (device_removed), um);
        cheese_camera_device_monitor_coldplug (um->monitor);

        y++;
#endif /* HAVE_CHEESE */

        um->photo_popup = menu;
}

static void
popup_icon_menu (GtkToggleButton *button, UmPhotoDialog *um)
{
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) && !gtk_widget_get_visible (um->photo_popup)) {
                gtk_menu_popup (GTK_MENU (um->photo_popup),
                                NULL, NULL,
                                (GtkMenuPositionFunc) popup_menu_below_button, um->popup_button,
                                0, gtk_get_current_event_time ());
        } else {
                gtk_menu_popdown (GTK_MENU (um->photo_popup));
        }
}

static gboolean
on_popup_button_button_pressed (GtkToggleButton *button,
                                GdkEventButton *event,
                                UmPhotoDialog  *um)
{
        if (event->button == 1) {
                if (!gtk_widget_get_visible (um->photo_popup)) {
                        popup_icon_menu (button, um);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
                } else {
                        gtk_menu_popdown (GTK_MENU (um->photo_popup));
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
                }

                return TRUE;
        }

        return FALSE;
}

static void
on_photo_popup_unmap (GtkWidget     *popup_menu,
                      UmPhotoDialog *um)
{
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (um->popup_button), FALSE);
}

static void
popup_button_draw (GtkWidget      *widget,
                   cairo_t        *cr,
                   UmPhotoDialog  *um)
{
        if (gtk_widget_get_state (gtk_bin_get_child (GTK_BIN (widget))) != GTK_STATE_PRELIGHT &&
            !gtk_widget_is_focus (widget)) {
                return;
        }

        down_arrow (gtk_widget_get_style_context (widget),
                    cr,
                    gtk_widget_get_allocated_width (widget) - 12,
                    gtk_widget_get_allocated_height (widget) - 12,
                    12, 12);
}

static void
popup_button_focus_changed (GObject       *button,
                            GParamSpec    *pspec,
                            UmPhotoDialog *um)
{
        gtk_widget_queue_draw (gtk_bin_get_child (GTK_BIN (button)));
}

UmPhotoDialog *
um_photo_dialog_new (GtkWidget            *button,
                     SelectAvatarCallback  callback,
                     gpointer              data)
{
        UmPhotoDialog *um;

        um = g_new0 (UmPhotoDialog, 1);

        /* Set up the popup */
        um->popup_button = button;
        setup_photo_popup (um);
        g_signal_connect (button, "toggled",
                          G_CALLBACK (popup_icon_menu), um);
        g_signal_connect (button, "button-press-event",
                          G_CALLBACK (on_popup_button_button_pressed), um);
        g_signal_connect (button, "notify::is-focus",
                          G_CALLBACK (popup_button_focus_changed), um);
        g_signal_connect_after (button, "draw",
                                G_CALLBACK (popup_button_draw), um);

        g_signal_connect (um->photo_popup, "unmap",
                          G_CALLBACK (on_photo_popup_unmap), um);

        um->callback = callback;
        um->data = data;

        return um;
}

void
um_photo_dialog_free (UmPhotoDialog *um)
{
        gtk_widget_destroy (um->photo_popup);

#ifdef HAVE_CHEESE
        if (um->monitor)
                g_object_unref (um->monitor);
#endif

        g_free (um);
}
