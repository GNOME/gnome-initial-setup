
/* Welcome page {{{1 */

static void
prepare_welcome_page (SetupData *setup)
{
        gchar *s;

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Welcome", "welcome-image",
                                          NULL, NULL);

        if (s && g_file_test (s, G_FILE_TEST_EXISTS))
                gtk_image_set_from_file (GTK_IMAGE (WID ("welcome-image")), s);

        g_free (s);

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Welcome", "welcome-title",
                                          NULL, NULL);
        if (s)
                gtk_label_set_text (GTK_LABEL (WID ("welcome-title")), s);
        g_free (s);

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Welcome", "welcome-subtitle",
                                          NULL, NULL);
        if (s)
                gtk_label_set_text (GTK_LABEL (WID ("welcome-subtitle")), s);
        g_free (s);
}
