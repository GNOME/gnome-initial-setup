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
#include <adwaita.h>

#include "um-photo-dialog.h"
#include "um-utils.h"

#define ROW_SPAN 5
#define AVATAR_PIXEL_SIZE 72

struct _UmPhotoDialog {
        GtkPopover parent;

        GtkWidget *flowbox;

        GListStore *faces;

        SelectAvatarCallback *callback;
        gpointer              data;
};

G_DEFINE_TYPE (UmPhotoDialog, um_photo_dialog, GTK_TYPE_POPOVER)


static void
webcam_icon_selected (UmPhotoDialog *um)
{
  g_warning ("Webcam icon selected, but compiled without Cheese support");
}

static void
face_widget_activated (GtkFlowBox      *flowbox,
                       GtkFlowBoxChild *child,
                       UmPhotoDialog   *um)
{
        const char *filename;
        GtkWidget  *image;

        image = gtk_flow_box_child_get_child (child);
        filename = g_object_get_data (G_OBJECT (image), "filename");

        um->callback (filename, um->data);

        gtk_popover_popdown (GTK_POPOVER (um));
}

static GtkWidget *
create_face_widget (gpointer item,
                    gpointer user_data)
{
        g_autoptr(GdkTexture) texture = NULL;
        g_autofree gchar *path = g_file_get_path (G_FILE (item));
        GtkWidget *image;

        image = adw_avatar_new (AVATAR_PIXEL_SIZE, NULL, TRUE);

        texture = gdk_texture_new_from_file (G_FILE (item), NULL);
        adw_avatar_set_custom_image (ADW_AVATAR (image), GDK_PAINTABLE (texture));

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

                        type = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE);
                        if (type != G_FILE_TYPE_REGULAR && type != G_FILE_TYPE_SYMBOLIC_LINK)
                                continue;

                        target = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);
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

        facesdirs = get_settings_facesdirs ();
        added_faces = add_faces_from_dirs (um->faces, facesdirs, TRUE);

        if (!added_faces) {
                facesdirs = get_system_facesdirs ();
                add_faces_from_dirs (um->faces, facesdirs, FALSE);
        }
}

UmPhotoDialog *
um_photo_dialog_new (SelectAvatarCallback  callback,
                     gpointer              data)
{
        UmPhotoDialog *um;

        um = g_object_new (UM_TYPE_PHOTO_DIALOG, NULL);

        setup_photo_popup (um);

        um->callback = callback;
        um->data = data;

        return um;
}

void
um_photo_dialog_dispose (GObject *object)
{
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
        gtk_widget_class_bind_template_callback (wclass, webcam_icon_selected);

        oclass->dispose = um_photo_dialog_dispose;
}
