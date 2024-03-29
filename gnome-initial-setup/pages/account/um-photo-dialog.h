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

#ifndef __UM_PHOTO_DIALOG_H__
#define __UM_PHOTO_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_PHOTO_DIALOG (um_photo_dialog_get_type())

G_DECLARE_FINAL_TYPE (UmPhotoDialog, um_photo_dialog, UM, PHOTO_DIALOG, GtkPopover)

typedef struct _UmPhotoDialog UmPhotoDialog;
typedef void (SelectAvatarCallback) (const gchar *filename,
                                     gpointer     data);

UmPhotoDialog *um_photo_dialog_new      (SelectAvatarCallback  callback,
                                         gpointer              data);
void           um_photo_dialog_free     (UmPhotoDialog *dialog);
void           um_photo_dialog_set_generated_avatar_text (UmPhotoDialog *dialog,
                                                          const gchar   *name);

G_END_DECLS

#endif
