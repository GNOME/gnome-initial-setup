/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Welcome page {{{1 */

#include "config.h"
#include "gis-welcome-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

void
gis_prepare_welcome_page (WelcomeData *data)
{
        gchar *s;
        SetupData *setup = data->setup;
        GKeyFile *overrides = gis_get_overrides (setup);

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
}
