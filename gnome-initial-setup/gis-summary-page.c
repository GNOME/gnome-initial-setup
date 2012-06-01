
/* Other setup {{{1 */

static void
copy_account_file (SetupData   *setup,
                   const gchar *relative_path)
{
        const gchar *username;
        const gchar *homedir;
        GSList *dirs = NULL, *l;
        gchar *p, *tmp;
        gchar *argv[20];
        gint i;
        gchar *from;
        gchar *to;
        GError *error = NULL;

        username = act_user_get_user_name (setup->act_user);
        homedir = act_user_get_home_dir (setup->act_user);

        from = g_build_filename (g_get_home_dir (), relative_path, NULL);
        to = g_build_filename (homedir, relative_path, NULL);

        p = g_path_get_dirname (relative_path);
        while (strcmp (p, ".") != 0) {
                dirs = g_slist_prepend (dirs, g_build_filename (homedir, p, NULL));
                tmp = g_path_get_dirname (p);
                g_free (p);
                p = tmp;
        }

        i = 0;
        argv[i++] = "/usr/bin/pkexec";
        argv[i++] = "install";
        argv[i++] = "--owner";
        argv[i++] = (gchar *)username;
        argv[i++] = "--group";
        argv[i++] = (gchar *)username;
        argv[i++] = "--mode";
        argv[i++] = "755";
        argv[i++] = "--directory";
        for (l = dirs; l; l = l->next) {
                argv[i++] = l->data;
                if (i == 20) {
                        g_warning ("Too many subdirectories");
                        goto out;
                }
        }
        argv[i++] = NULL;

        if (!g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error)) {
                g_warning ("Failed to copy account data: %s", error->message);
                g_error_free (error);
                goto out;
        }

        i = 0;
        argv[i++] = "/usr/bin/pkexec";
        argv[i++] = "install";
        argv[i++] = "--owner";
        argv[i++] = (gchar *)username;
        argv[i++] = "--group";
        argv[i++] = (gchar *)username;
        argv[i++] = "--mode";
        argv[i++] = "755";
        argv[i++] = from;
        argv[i++] = to;
        argv[i++] = NULL;

        if (!g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error)) {
                g_warning ("Failed to copy account data: %s", error->message);
                g_error_free (error);
                goto out;
        }

out:
        g_slist_free_full (dirs, g_free);
        g_free (to);
        g_free (from);
}

static void
copy_account_data (SetupData *setup)
{
        /* here is where we copy all the things we just
         * configured, from the current users home dir to the
         * account that was created in the first step
         */
        g_debug ("Copying account data");
        g_settings_sync ();

        copy_account_file (setup, ".config/dconf/user");
        copy_account_file (setup, ".config/goa-1.0/accounts.conf");
        copy_account_file (setup, ".gnome2/keyrings/Default.keyring");
}

static void
connect_to_slave (SetupData *setup)
{
        GError *error = NULL;
        gboolean res;

        setup->greeter_client = gdm_greeter_client_new ();

        res = gdm_greeter_client_open_connection (setup->greeter_client, &error);

        if (!res) {
                g_warning ("Failed to open connection to slave: %s", error->message);
                g_error_free (error);
                g_clear_object (&setup->greeter_client);
                return;
        }
}

static void
on_ready_for_auto_login (GdmGreeterClient *client,
                         const char       *service_name,
                         SetupData        *setup)
{
        const gchar *username;

        username = act_user_get_user_name (setup->act_user);

        g_debug ("Initiating autologin for %s", username);
        gdm_greeter_client_call_begin_auto_login (client, username);
        gdm_greeter_client_call_start_session_when_ready (client,
                                                          service_name,
                                                          TRUE);
}

static void
begin_autologin (SetupData *setup)
{
        if (setup->greeter_client == NULL) {
                g_warning ("No slave connection; not initiating autologin");
                return;
        }

        if (setup->act_user == NULL) {
                g_warning ("No username; not initiating autologin");
                return;
        }

        g_debug ("Preparing to autologin");

        g_signal_connect (setup->greeter_client,
                          "ready",
                          G_CALLBACK (on_ready_for_auto_login),
                          setup);
        gdm_greeter_client_call_start_conversation (setup->greeter_client, "gdm-autologin");
}

static void
byebye_cb (GtkButton *button, SetupData *setup)
{
        begin_autologin (setup);
}

static void
tour_cb (GtkButton *button, SetupData *setup)
{
        gchar *filename;

        /* the tour is triggered by ~/.config/run-welcome-tour */
        filename = g_build_filename (g_get_home_dir (), ".config", "run-welcome-tour", NULL);
        g_file_set_contents (filename, "yes", -1, NULL);
        copy_account_file (setup, ".config/run-welcome-tour");
        g_free (filename);

        begin_autologin (setup);
}

static void
prepare_summary_page (SetupData *setup)
{
        GtkWidget *button;
        gchar *s;

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Summary", "summary-title",
                                          NULL, NULL);
        if (s)
                gtk_label_set_text (GTK_LABEL (WID ("summary-title")), s);
        g_free (s);

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Summary", "summary-details",
                                          NULL, NULL);
        if (s) {
                gtk_label_set_text (GTK_LABEL (WID ("summary-details")), s);
        }
        g_free (s);

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Summary", "summary-details2",
                                          NULL, NULL);
        if (s)
                gtk_label_set_text (GTK_LABEL (WID ("summary-details2")), s);
        g_free (s);

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Summary", "summary-start-button",
                                          NULL, NULL);
        if (s)
                gtk_button_set_label (GTK_BUTTON (WID ("summary-start-button")), s);
        g_free (s);

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Summary", "summary-tour-details",
                                          NULL, NULL);
        if (s)
                gtk_label_set_text (GTK_LABEL (WID ("summary-tour-details")), s);
        g_free (s);

        s = g_key_file_get_locale_string (setup->overrides,
                                          "Summary", "summary-tour-button",
                                          NULL, NULL);
        if (s)
                gtk_button_set_label (GTK_BUTTON (WID ("summary-tour-button")), s);
        g_free (s);

        button = WID("summary-start-button");
        g_signal_connect (button, "clicked",
                          G_CALLBACK (byebye_cb), setup);
        button = WID("summary-tour-button");
        g_signal_connect (button, "clicked",
                          G_CALLBACK (tour_cb), setup);
}
