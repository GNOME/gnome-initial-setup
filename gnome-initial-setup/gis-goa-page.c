/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Online accounts page {{{1 */

#include "config.h"
#include "gis-goa-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

static GtkWidget *
create_provider_button (const gchar *type, const gchar *name, GIcon *icon)
{
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;

  button = gtk_button_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (box, GTK_ALIGN_START);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (button), box);

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);

  label = gtk_label_new (name);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  gtk_widget_show (box);
  gtk_widget_show (image);
  gtk_widget_show (label);

  g_object_set_data (G_OBJECT (button), "provider-type", (gpointer)type);

  return button;
}

static void
add_account (GtkButton *button, gpointer user_data)
{
  GoaData *data = user_data;
  GtkWidget *dialog;
  GtkWidget *goa_dialog;
  GtkWidget *vbox;
  const gchar *provider_type;
  GoaProvider *provider;
  GError *error;
  SetupData *setup = data->setup;

  dialog = WID("online-accounts-dialog");
  gtk_widget_hide (dialog);

  provider_type = g_object_get_data (G_OBJECT (button), "provider-type");

  g_debug ("Adding online account: %s", provider_type);

  provider = goa_provider_get_for_provider_type (provider_type);

  goa_dialog = gtk_dialog_new ();

  gtk_container_set_border_width (GTK_CONTAINER (goa_dialog), 12);
  gtk_window_set_modal (GTK_WINDOW (goa_dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (goa_dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (goa_dialog), GTK_WINDOW (gis_get_assistant (setup)));
  /* translators: This is the title of the "Add Account" dialogue.
   * The title is not visible when using GNOME Shell
   */
  gtk_window_set_title (GTK_WINDOW (goa_dialog), _("Add Account"));
  gtk_dialog_add_button (GTK_DIALOG (goa_dialog),
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (goa_dialog));
  gtk_widget_set_vexpand (vbox, TRUE);

  gtk_widget_show_all (goa_dialog);
  gtk_window_present (GTK_WINDOW (goa_dialog));

  error = NULL;
  goa_provider_add_account (provider,
                            data->goa_client,
                            GTK_DIALOG (goa_dialog),
                            GTK_BOX (vbox),
                            &error);
  gtk_widget_destroy (goa_dialog);

  if (error &&
      !(error->domain == GOA_ERROR && error->code == GOA_ERROR_DIALOG_DISMISSED))
        {
          dialog = gtk_message_dialog_new (GTK_WINDOW (gis_get_assistant (setup)),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("Error creating account"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    "%s",
                                                    error->message);
          gtk_widget_show (dialog);
          gtk_dialog_run (GTK_DIALOG (dialog));
          gtk_widget_destroy (dialog);
    }
}

static void
populate_online_account_dialog (GoaData *data)
{
  GtkWidget *dialog;
  GtkWidget *content_area;
  GList *providers, *l;
  GoaProvider *provider;
  gchar *provider_name;
  const gchar *provider_type;
  GIcon *provider_icon;
  GtkWidget *button;
  SetupData *setup = data->setup;

  dialog = WID("online-accounts-dialog");
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  providers = goa_provider_get_all ();
  for (l = providers; l; l = l->next)
    {
      provider = GOA_PROVIDER (l->data);
      provider_type = goa_provider_get_provider_type (provider);
      provider_name = goa_provider_get_provider_name (provider, NULL);
      provider_icon = goa_provider_get_provider_icon (provider, NULL);
      button = create_provider_button (provider_type, provider_name, provider_icon);
      gtk_container_add (GTK_CONTAINER (content_area), button);
      gtk_widget_show (button);
      g_free (provider_name);

      g_signal_connect (button, "clicked", G_CALLBACK (add_account), setup);
    }

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

static void
show_online_account_dialog (GtkButton *button, gpointer user_data)
{
  GoaData *data = user_data;
  SetupData *setup = data->setup;
  GtkWidget *dialog;

  dialog = WID("online-accounts-dialog");

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
remove_account_cb (GoaAccount   *account,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GoaData *data = user_data;
  GError *error;

  error = NULL;
  if (!goa_account_call_remove_finish (account, res, &error))
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (GTK_WINDOW (gis_get_assistant (data->setup)),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Error removing account"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);
      gtk_widget_show_all (dialog);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
}


static void
confirm_remove_account (GtkButton *button, gpointer user_data)
{
  GoaData *data = user_data;
  GtkWidget *dialog;
  GoaObject *object;
  gint response;

  object = g_object_get_data (G_OBJECT (button), "goa-object");

  dialog = gtk_message_dialog_new (GTK_WINDOW (gis_get_assistant (data->setup)),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_CANCEL,
                                   _("Are you sure you want to remove the account?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("This will not remove the account on the server."));
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Remove"), GTK_RESPONSE_OK);
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_OK)
    {
      goa_account_call_remove (goa_object_peek_account (object),
                               NULL, /* GCancellable */
                               (GAsyncReadyCallback) remove_account_cb,
                               data);
    }
}


static void
add_account_to_list (GoaData *data, GoaObject *object)
{
  GtkWidget *list;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  GoaAccount *account;
  GIcon *icon;
  gchar *markup;
  SetupData *setup = data->setup;

  account = goa_object_peek_account (object);

  icon = g_icon_new_for_string (goa_account_get_provider_icon (account), NULL);
  markup = g_strdup_printf ("<b>%s</b>\n"
                            "<small><span foreground=\"#555555\">%s</span></small>",
                            goa_account_get_provider_name (account),
                            goa_account_get_presentation_identity (account));

  list = WID ("online-accounts-list");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_hexpand (box, TRUE);

  g_object_set_data (G_OBJECT (box), "account-id",
                     (gpointer)goa_account_get_id (account));

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  label = gtk_label_new (markup);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  button = gtk_button_new_with_label (_("Remove"));
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

  g_object_set_data_full (G_OBJECT (button), "goa-object",
                          g_object_ref (object), g_object_unref);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (confirm_remove_account), setup);

  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);

  gtk_widget_show_all (box);

  gtk_container_add (GTK_CONTAINER (list), box);
}

static void
remove_account_from_list (GoaData *data, GoaObject *object)
{
  GtkWidget *list;
  GList *children, *l;
  GtkWidget *child;
  GoaAccount *account;
  const gchar *account_id, *id;
  SetupData *setup = data->setup;

  account = goa_object_peek_account (object);

  account_id = goa_account_get_id (account);

  list = WID ("online-accounts-list");

  children = gtk_container_get_children (GTK_CONTAINER (list));
  for (l = children; l; l = l->next)
    {
      child = GTK_WIDGET (l->data);

      id = (const gchar *)g_object_get_data (G_OBJECT (child), "account-id");

      if (g_strcmp0 (id, account_id) == 0)
        {
          gtk_widget_destroy (child);
          break;
        }
    }
  g_list_free (children);
}

static void
populate_account_list (GoaData *data)
{
  GList *accounts, *l;
  GoaObject *object;

  accounts = goa_client_get_accounts (data->goa_client);
  for (l = accounts; l; l = l->next)
    {
      object = GOA_OBJECT (l->data);
      add_account_to_list (data, object);
    }

  g_list_free_full (accounts, (GDestroyNotify) g_object_unref);
}

static void
goa_account_added (GoaClient *client, GoaObject *object, gpointer user_data)
{
  GoaData *data = user_data;

  g_debug ("Online account added");

  add_account_to_list (data, object);
}

static void
goa_account_removed (GoaClient *client, GoaObject *object, gpointer user_data)
{
  GoaData *data = user_data;

  g_debug ("Online account removed");

  remove_account_from_list (data, object);
}

void
gis_prepare_online_page (GoaData *data)
{
  GtkWidget *button;
  GError *error = NULL;
  SetupData *setup = data->setup;

  data->goa_client = goa_client_new_sync (NULL, &error);
  if (data->goa_client == NULL)
    {
       g_error ("Failed to get a GoaClient: %s", error->message);
       g_error_free (error);
       return;
    }

  populate_online_account_dialog (data);
  populate_account_list (data);

  button = WID("online-add-button");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (show_online_account_dialog), data);

  g_signal_connect (data->goa_client, "account-added",
                    G_CALLBACK (goa_account_added), data);
  g_signal_connect (data->goa_client, "account-removed",
                    G_CALLBACK (goa_account_removed), data);
}
