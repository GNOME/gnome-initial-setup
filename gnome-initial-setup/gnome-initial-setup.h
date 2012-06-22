/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GNOME_INITIAL_SETUP_H__
#define __GNOME_INITIAL_SETUP_H__

#include <gtk/gtk.h>

#include "gis-assistant.h"

#include <act/act-user-manager.h>

G_BEGIN_DECLS

typedef struct _SetupData SetupData;

GtkBuilder *gis_get_builder (SetupData *setup);
GtkWindow *gis_get_main_window (SetupData *setup);
GKeyFile *gis_get_overrides (SetupData *setup);
GisAssistant * gis_get_assistant (SetupData *setup);
ActUser * gis_get_act_user (SetupData *setup);

#define OBJ(type,name) ((type)gtk_builder_get_object(gis_get_builder(setup),(name)))
#define WID(name) OBJ(GtkWidget*,name)

G_END_DECLS

#endif /* __GNOME_INITIAL_SETUP_H__ */

