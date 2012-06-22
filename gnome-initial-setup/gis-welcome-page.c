/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Welcome page {{{1 */

#include "config.h"
#include "gis-welcome-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#define OBJ(type,name) ((type)gtk_builder_get_object(builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

void
gis_prepare_welcome_page (SetupData *setup)
{
  gchar *s;
  GKeyFile *overrides = gis_get_overrides (setup);
  GisAssistant *assistant = gis_get_assistant (setup);
  GtkBuilder *builder = gis_builder ("gis-welcome-page");

  s = g_key_file_get_locale_string (overrides,
                                    "Welcome", "welcome-image",
                                    NULL, NULL);

  if (s && g_file_test (s, G_FILE_TEST_EXISTS))
    gtk_image_set_from_file (GTK_IMAGE (WID ("welcome-image")), s);

  g_free (s);

  s = g_key_file_get_locale_string (overrides,
                                    "Welcome", "welcome-title",
                                    NULL, NULL);
  if (s)
    gtk_label_set_text (GTK_LABEL (WID ("welcome-title")), s);
  g_free (s);

  s = g_key_file_get_locale_string (overrides,
                                    "Welcome", "welcome-subtitle",
                                    NULL, NULL);
  if (s)
    gtk_label_set_text (GTK_LABEL (WID ("welcome-subtitle")), s);
  g_free (s);

  g_key_file_unref (overrides);

  gis_assistant_add_page (assistant, WID ("welcome-page"));
  gis_assistant_set_page_complete (assistant, WID ("welcome-page"), TRUE);

  g_object_unref (builder);
}
