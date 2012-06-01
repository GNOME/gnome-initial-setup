
/* EULA pages {{{1 */

/* heavily lifted from g_output_stream_splice */
static void
splice_buffer (GInputStream  *stream,
               GtkTextBuffer *buffer,
               GError       **error)
{
        char contents[8192];
        gssize n_read;
        GtkTextIter iter;

        while (TRUE) {
                n_read = g_input_stream_read (stream, contents, sizeof (contents), NULL, error);

                /* error or eof */
                if (n_read <= 0)
                        break;

                gtk_text_buffer_get_end_iter (buffer, &iter);
                gtk_text_buffer_insert (buffer, &iter, contents, n_read);
        }
}

static GtkWidget *
build_eula_text_view (GFile *eula)
{
        GInputStream *input_stream = NULL;
        GError *error = NULL;
        GtkWidget *widget = NULL;
        GtkTextBuffer *buffer;
        GtkTextIter start, end;

        input_stream = G_INPUT_STREAM (g_file_read (eula, NULL, &error));
        if (error != NULL)
                goto out;

        buffer = gtk_text_buffer_new (NULL);
        splice_buffer (input_stream, buffer, &error);
        if (error != NULL)
                goto out;

        /* monospace the text */
        gtk_text_buffer_create_tag (buffer, "monospace", "family", "monospace", NULL);
        gtk_text_buffer_get_start_iter (buffer, &start);
        gtk_text_buffer_get_end_iter (buffer, &end);
        gtk_text_buffer_apply_tag_by_name (buffer, "monospace", &start, &end);

        widget = gtk_text_view_new_with_buffer (buffer);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), FALSE);
        gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (widget), FALSE);

 out:
        if (error != NULL) {
                g_printerr ("Error while reading EULA: %s", error->message);
                g_error_free (error);
        }

        g_clear_object (&input_stream);
        return widget;
}

static void
eula_checkbox_toggled (GtkToggleButton *checkbox,
                       SetupData       *setup)
{
        gtk_assistant_set_page_complete (setup->assistant,
                                         g_object_get_data (G_OBJECT (checkbox), "assistant-page"),
                                         gtk_toggle_button_get_active (checkbox));
}

static void
build_eula_page (SetupData *setup,
                 GFile     *eula)
{
        GtkWidget *text_view;
        GtkWidget *vbox;
        GtkWidget *scrolled_window;
        GtkWidget *checkbox;

        text_view = build_eula_text_view (eula);
        if (text_view == NULL)
                return;

        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                             GTK_SHADOW_ETCHED_IN);
        gtk_widget_set_vexpand (scrolled_window, TRUE);
        gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

        checkbox = gtk_check_button_new_with_mnemonic (_("I have _agreed to the "
                                                         "terms and conditions in "
                                                         "this end user license "
                                                         "agreement."));

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_add (GTK_CONTAINER (vbox), scrolled_window);
        gtk_container_add (GTK_CONTAINER (vbox), checkbox);

        /* XXX: 1 is the location after the welcome page.
         * Remove this hardcoded thing. */
        gtk_assistant_insert_page (setup->assistant, vbox, 1);
        gtk_assistant_set_page_complete (setup->assistant, vbox, FALSE);

        gtk_widget_show_all (GTK_WIDGET (vbox));
        g_signal_connect (checkbox, "toggled",
                          G_CALLBACK (eula_checkbox_toggled),
                          setup);
        g_object_set_data (G_OBJECT (checkbox), "assistant-page", vbox);
}

static void
prepare_eula_pages (SetupData *setup)
{
        gchar *eulas_dir_path;
        GFile *eulas_dir;
        GError *error = NULL;
        GFileEnumerator *enumerator = NULL;
        GFileInfo *info;

        eulas_dir_path = g_build_filename (UIDIR, "eulas", NULL);
        eulas_dir = g_file_new_for_path (eulas_dir_path);
        g_free (eulas_dir_path);

        if (!g_file_query_exists (eulas_dir, NULL))
                goto out;

        enumerator = g_file_enumerate_children (eulas_dir,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL,
                                                &error);

        if (error != NULL)
                goto out;

        while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL) {
                GFile *eula = g_file_get_child (eulas_dir, g_file_info_get_name (info));
                build_eula_page (setup, eula);
        }

        if (error != NULL)
                goto out;

 out:
        if (error != NULL) {
                g_printerr ("Error while parsing eulas: %s", error->message);
                g_error_free (error);
        }

        g_object_unref (eulas_dir);
        g_clear_object (&enumerator);
}
