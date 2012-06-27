/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GNOME_INITIAL_SETUP_H__
#define __GNOME_INITIAL_SETUP_H__

#include <gtk/gtk.h>

#include "gis-assistant.h"
#include "gis-utils.h"

G_BEGIN_DECLS

typedef struct _SetupData SetupData;

GtkWindow *gis_get_main_window (SetupData *setup);
GKeyFile *gis_get_overrides (SetupData *setup);
GisAssistant * gis_get_assistant (SetupData *setup);
void gis_add_summary_callback (SetupData *setup, GFunc callback, gpointer user_data);

G_END_DECLS

#endif /* __GNOME_INITIAL_SETUP_H__ */

