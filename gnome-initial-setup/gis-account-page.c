
/* Account page {{{1 */

static gboolean skip_account = FALSE;

static void
update_account_page_status (SetupData *setup)
{
        gboolean complete;

        complete = setup->valid_name && setup->valid_username &&
                   (setup->valid_password ||
                    setup->password_mode == ACT_USER_PASSWORD_MODE_NONE);

        gis_assistant_set_page_complete (gis_get_assistant (setup), WID("account-page"), complete);
        gtk_widget_set_sensitive (WID("local-account-done-button"), complete);
}

static void
fullname_changed (GtkWidget *w, GParamSpec *pspec, SetupData *setup)
{
        GtkWidget *combo;
        GtkWidget *entry;
        GtkTreeModel *model;
        const char *name;

        name = gtk_entry_get_text (GTK_ENTRY (w));

        combo = WID("account-username-combo");
        entry = gtk_bin_get_child (GTK_BIN (combo));
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

        gtk_list_store_clear (GTK_LIST_STORE (model));

        setup->valid_name = is_valid_name (name);
        setup->user_data_unsaved = TRUE;

        if (!setup->valid_name) {
                gtk_entry_set_text (GTK_ENTRY (entry), "");
                return;
        }

        generate_username_choices (name, GTK_LIST_STORE (model));

        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

        update_account_page_status (setup);
}

static void
username_changed (GtkComboBoxText *combo, SetupData *setup)
{
        const gchar *username;
        gchar *tip;
        GtkWidget *entry;

        username = gtk_combo_box_text_get_active_text (combo);

        setup->valid_username = is_valid_username (username, &tip);
        setup->user_data_unsaved = TRUE;

        entry = gtk_bin_get_child (GTK_BIN (combo));

        if (tip) {
                set_entry_validation_error (GTK_ENTRY (entry), tip);
                g_free (tip);
        }
        else {
                clear_entry_validation_error (GTK_ENTRY (entry));
        }

        update_account_page_status (setup);
}

static void
password_check_changed (GtkWidget *w, GParamSpec *pspec, SetupData *setup)
{
        GtkWidget *password_entry;
        GtkWidget *confirm_entry;
        gboolean need_password;

        need_password = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

        if (need_password) {
                setup->password_mode = ACT_USER_PASSWORD_MODE_REGULAR;
                setup->valid_password = FALSE;
        }
        else {
                setup->password_mode = ACT_USER_PASSWORD_MODE_NONE;
                setup->valid_password = TRUE;
        }

        password_entry = WID("account-password-entry");
        confirm_entry = WID("account-confirm-entry");

        gtk_entry_set_text (GTK_ENTRY (password_entry), "");
        gtk_entry_set_text (GTK_ENTRY (confirm_entry), "");
        gtk_widget_set_sensitive (password_entry, need_password);
        gtk_widget_set_sensitive (confirm_entry, need_password);

        setup->user_data_unsaved = TRUE;
        update_account_page_status (setup);
}

static void
admin_check_changed (GtkWidget *w, GParamSpec *pspec, SetupData *setup)
{
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
                setup->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;
        }
        else {
                setup->account_type = ACT_USER_ACCOUNT_TYPE_STANDARD;
        }

        setup->user_data_unsaved = TRUE;
        update_account_page_status (setup);
}

#define MIN_PASSWORD_LEN 6

static void
update_password_entries (SetupData *setup)
{
        const gchar *password;
        const gchar *verify;
        const gchar *username;
        GtkWidget *password_entry;
        GtkWidget *confirm_entry;
        GtkWidget *username_combo;
        gdouble strength;
        const gchar *hint;
        const gchar *long_hint = NULL;

        password_entry = WID("account-password-entry");
        confirm_entry = WID("account-confirm-entry");
        username_combo = WID("account-username-combo");

        password = gtk_entry_get_text (GTK_ENTRY (password_entry));
        verify = gtk_entry_get_text (GTK_ENTRY (confirm_entry));
        username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (username_combo));

        strength = pw_strength (password, NULL, username, &hint, &long_hint);

        if (strength == 0.0) {
                setup->valid_password = FALSE;
                setup->password_reason = long_hint ? long_hint : hint;
        }
        else if (strcmp (password, verify) != 0) {
                setup->valid_password = FALSE;
                setup->password_reason = _("Passwords do not match");
        }
        else {
                setup->valid_password = TRUE;
        }
}

static void
password_changed (GtkWidget *w, GParamSpec *pspec, SetupData *setup)
{
        update_password_entries (setup);

        setup->user_data_unsaved = TRUE;
        update_account_page_status (setup);
}

static void
confirm_changed (GtkWidget *w, GParamSpec *pspec, SetupData *setup)
{
        clear_entry_validation_error (GTK_ENTRY (w));
        update_password_entries (setup);

        setup->user_data_unsaved = TRUE;
        update_account_page_status (setup);
}

static gboolean
confirm_entry_focus_out (GtkWidget     *entry,
                         GdkEventFocus *event,
                         SetupData     *setup)
{
        const gchar *password;
        const gchar *verify;
        GtkEntry *password_entry;
        GtkEntry *confirm_entry;

        password_entry = OBJ(GtkEntry*, "account-password-entry");
        confirm_entry = OBJ(GtkEntry*, "account-confirm-entry");
        password = gtk_entry_get_text (password_entry);
        verify = gtk_entry_get_text (confirm_entry);

        if (strlen (password) > 0 && strlen (verify) > 0) {
                if (!setup->valid_password) {
                        set_entry_validation_error (confirm_entry,
                                                    setup->password_reason);
                }
                else {
                        clear_entry_validation_error (confirm_entry);
                }
        }

        return FALSE;
}

static void
set_user_avatar (SetupData *setup)
{
        GFile *file = NULL;
        GFileIOStream *io_stream = NULL;
        GOutputStream *stream = NULL;
        GError *error = NULL;

        if (setup->avatar_filename != NULL) {
                act_user_set_icon_file (setup->act_user, setup->avatar_filename);
                return;
        }

        if (setup->avatar_pixbuf == NULL) {
                return;
        }

        file = g_file_new_tmp ("usericonXXXXXX", &io_stream, &error);
        if (error != NULL)
                goto out;

        stream = g_io_stream_get_output_stream (G_IO_STREAM (io_stream));
        if (!gdk_pixbuf_save_to_stream (setup->avatar_pixbuf, stream, "png", NULL, &error, NULL))
                goto out;

        act_user_set_icon_file (setup->act_user, g_file_get_path (file)); 

 out:
        if (error != NULL) {
                g_warning ("failed to save image: %s", error->message);
                g_error_free (error);
        }
        g_clear_object (&stream);
        g_clear_object (&io_stream);
        g_clear_object (&file);
}

static void
create_user (SetupData *setup)
{
        const gchar *username;
        const gchar *fullname;
        GError *error;

        username = gtk_combo_box_text_get_active_text (OBJ(GtkComboBoxText*, "account-username-combo"));
        fullname = gtk_entry_get_text (OBJ(GtkEntry*, "account-fullname-entry"));

        error = NULL;
        setup->act_user = act_user_manager_create_user (setup->act_client, username, fullname, setup->account_type, &error);
        if (error != NULL) {
                g_warning ("Failed to create user: %s", error->message);
                g_error_free (error);
        }

        set_user_avatar (setup);
}

static void save_account_data (SetupData *setup);

gulong when_loaded;

static void
save_when_loaded (ActUser *user, GParamSpec *pspec, SetupData *setup)
{
        g_signal_handler_disconnect (user, when_loaded);
        when_loaded = 0;

        save_account_data (setup);
}

static void
clear_account_page (SetupData *setup)
{
        GtkWidget *fullname_entry;
        GtkWidget *username_combo;
        GtkWidget *password_check;
        GtkWidget *admin_check;
        GtkWidget *password_entry;
        GtkWidget *confirm_entry;
        gboolean need_password;

        fullname_entry = WID("account-fullname-entry");
        username_combo = WID("account-username-combo");
        password_check = WID("account-password-check");
        admin_check = WID("account-admin-check");
        password_entry = WID("account-password-entry");
        confirm_entry = WID("account-confirm-entry");

        setup->valid_name = FALSE;
        setup->valid_username = FALSE;
        setup->valid_password = TRUE;
        setup->password_mode = ACT_USER_PASSWORD_MODE_NONE;
        setup->account_type = ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;
        setup->user_data_unsaved = FALSE;

        need_password = setup->password_mode != ACT_USER_PASSWORD_MODE_NONE;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (password_check), need_password);
        gtk_widget_set_sensitive (password_entry, need_password);
        gtk_widget_set_sensitive (confirm_entry, need_password);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (admin_check), setup->account_type == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR);

        gtk_entry_set_text (GTK_ENTRY (fullname_entry), "");
        gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (username_combo))));
        gtk_entry_set_text (GTK_ENTRY (password_entry), "");
        gtk_entry_set_text (GTK_ENTRY (confirm_entry), "");
}

static void
save_account_data (SetupData *setup)
{
        const gchar *realname;
        const gchar *username;
        const gchar *password;

        if (!setup->user_data_unsaved) {
                return;
        }

        /* this can happen when going back */
        if (!setup->valid_name ||
            !setup->valid_username ||
            !setup->valid_password) {
                return;
        }

        if (setup->act_user == NULL) {
                create_user (setup);
        }

        if (setup->act_user == NULL) {
                g_warning ("User creation failed");
                clear_account_page (setup);
                return;
        }

        if (!act_user_is_loaded (setup->act_user)) {
                if (when_loaded == 0)
                        when_loaded = g_signal_connect (setup->act_user, "notify::is-loaded",
                                                        G_CALLBACK (save_when_loaded), setup);
                return;
        }

        realname = gtk_entry_get_text (OBJ (GtkEntry*, "account-fullname-entry"));
        username = gtk_combo_box_text_get_active_text (OBJ (GtkComboBoxText*, "account-username-combo"));
        password = gtk_entry_get_text (OBJ (GtkEntry*, "account-password-entry"));

        act_user_set_real_name (setup->act_user, realname);
        act_user_set_user_name (setup->act_user, username);
        act_user_set_account_type (setup->act_user, setup->account_type);
        if (setup->password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
                act_user_set_password (setup->act_user, password, NULL);
        }
        else {
                act_user_set_password_mode (setup->act_user, setup->password_mode);
        }

        gnome_keyring_create_sync ("Default", password ? password : "");
        gnome_keyring_set_default_keyring_sync ("Default");

        setup->user_data_unsaved = FALSE;
}

static void
show_local_account_dialog (GtkWidget *button,
                           SetupData *setup)
{
        GtkWidget *dialog;

        dialog = WID("local-account-dialog");

        gtk_window_present (GTK_WINDOW (dialog));
}

static void
hide_local_account_dialog (GtkButton *button, gpointer data)
{
        SetupData *setup = data;
        GtkWidget *dialog;

        dialog = WID("local-account-dialog");

        gtk_widget_hide (dialog);
        clear_account_page (setup);
}

static void
create_local_account (GtkButton *button, gpointer data)
{
        SetupData *setup = data;
        gtk_widget_hide (WID("local-account-dialog"));
}

static void
avatar_callback (GdkPixbuf   *pixbuf,
                 const gchar *filename,
                 gpointer     data)
{
        SetupData *setup = data;
        GtkWidget *image;
        GdkPixbuf *tmp;

        g_clear_object (&setup->avatar_pixbuf);
        g_free (setup->avatar_filename);
        setup->avatar_filename = NULL;

        image = WID("local-account-avatar-image");

        if (pixbuf) {
                setup->avatar_pixbuf = g_object_ref (pixbuf);
                tmp = gdk_pixbuf_scale_simple (pixbuf, 64, 64, GDK_INTERP_BILINEAR);
                gtk_image_set_from_pixbuf (GTK_IMAGE (image), tmp);
                g_object_unref (tmp);
        }
        else if (filename) {
                setup->avatar_filename = g_strdup (filename);
                tmp = gdk_pixbuf_new_from_file_at_size (filename, 64, 64, NULL);
                gtk_image_set_from_pixbuf (GTK_IMAGE (image), tmp);
                g_object_unref (tmp);
        }
        else {
                gtk_image_set_from_icon_name (GTK_IMAGE (image), "avatar-default",
                                                                GTK_ICON_SIZE_DIALOG);
        }
}

static void
prepare_account_page (SetupData *setup)
{
        GtkWidget *fullname_entry;
        GtkWidget *username_combo;
        GtkWidget *password_check;
        GtkWidget *admin_check;
        GtkWidget *password_entry;
        GtkWidget *confirm_entry;
        GtkWidget *local_account_cancel_button;
        GtkWidget *local_account_done_button;
        GtkWidget *local_account_avatar_button;

        if (!skip_account)
                gtk_widget_show (WID("account-page"));

        g_signal_connect (WID("account-new-local"), "clicked",
                          G_CALLBACK (show_local_account_dialog), setup);

        fullname_entry = WID("account-fullname-entry");
        username_combo = WID("account-username-combo");
        password_check = WID("account-password-check");
        admin_check = WID("account-admin-check");
        password_entry = WID("account-password-entry");
        confirm_entry = WID("account-confirm-entry");
        local_account_cancel_button = WID("local-account-cancel-button");
        local_account_done_button = WID("local-account-done-button");
        local_account_avatar_button = WID("local-account-avatar-button");
        setup->photo_dialog = (GtkWidget *)um_photo_dialog_new (local_account_avatar_button,
                                                                avatar_callback,
                                                                setup);

        g_signal_connect (fullname_entry, "notify::text",
                          G_CALLBACK (fullname_changed), setup);
        g_signal_connect (username_combo, "changed",
                          G_CALLBACK (username_changed), setup);
        g_signal_connect (password_check, "notify::active",
                           G_CALLBACK (password_check_changed), setup);
        g_signal_connect (admin_check, "notify::active",
                          G_CALLBACK (admin_check_changed), setup);
        g_signal_connect (password_entry, "notify::text",
                          G_CALLBACK (password_changed), setup);
        g_signal_connect (confirm_entry, "notify::text",
                          G_CALLBACK (confirm_changed), setup);
        g_signal_connect_after (confirm_entry, "focus-out-event",
                                G_CALLBACK (confirm_entry_focus_out), setup);
        g_signal_connect (local_account_cancel_button, "clicked",
                          G_CALLBACK (hide_local_account_dialog), setup);
        g_signal_connect (local_account_done_button, "clicked",
                          G_CALLBACK (create_local_account), setup);

        setup->act_client = act_user_manager_get_default ();

        clear_account_page (setup);
        update_account_page_status (setup);
}
