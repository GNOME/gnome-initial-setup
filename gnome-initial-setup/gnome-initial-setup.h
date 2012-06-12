/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GNOME_INITIAL_SETUP_H__
#define __GNOME_INITIAL_SETUP_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _SetupData SetupData;

/* ugly hacks ugghh */
#ifndef GIS_COMP
struct _SetupData {
    GtkBuilder *builder;
};
#endif

#define OBJ(type,name) ((type)gtk_builder_get_object(setup->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

G_END_DECLS

#endif /* __GNOME_INITIAL_SETUP_H__ */

