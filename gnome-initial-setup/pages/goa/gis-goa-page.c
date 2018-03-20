/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Online accounts page {{{1 */

#define PAGE_ID "goa"

#include "config.h"
#include "gis-goa-page.h"
#include "goa-resources.h"

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

struct _GisGoaPagePrivate {
  GtkWidget *accounts_list;

  GoaClient *goa_client;
  GHashTable *providers;
  gboolean accounts_exist;
};
typedef struct _GisGoaPagePrivate GisGoaPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisGoaPage, gis_goa_page, GIS_TYPE_PAGE);

struct _ProviderWidget {
  GisGoaPage *page;
  GoaProvider *provider;
  GoaAccount *displayed_account;

  GtkWidget *row;
  GtkWidget *checkmark;
  GtkWidget *account_label;
};
typedef struct _ProviderWidget ProviderWidget;

static void
sync_provider_widget (ProviderWidget *provider_widget)
{
  gboolean has_account = (provider_widget->displayed_account != NULL);

  gtk_widget_set_visible (provider_widget->checkmark, has_account);
  gtk_widget_set_visible (provider_widget->account_label, has_account);
  gtk_widget_set_sensitive (provider_widget->row, !has_account);

  if (has_account) {
    char *markup;
    markup = g_strdup_printf ("<small><span foreground=\"#555555\">%s</span></small>",
                              goa_account_get_presentation_identity (provider_widget->displayed_account));
    gtk_label_set_markup (GTK_LABEL (provider_widget->account_label), markup);
    g_free (markup);
  }
}

static void
add_account_to_provider (ProviderWidget *provider_widget)
{
  GisGoaPage *page = provider_widget->page;
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  GtkWindow *parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page)));
  GError *error = NULL;
  GtkWidget *dialog;

  dialog = gtk_dialog_new_with_buttons (_("Add Account"),
                                        parent,
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_USE_HEADER_BAR,
                                        NULL, NULL);

  goa_provider_add_account (provider_widget->provider,
                            priv->goa_client,
                            GTK_DIALOG (dialog),
                            GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                            &error);

  /* this will fire the `account-added` signal, which will do
   * the syncing of displayed_account on its own */

  if (error) {
    if (!g_error_matches (error, GOA_ERROR, GOA_ERROR_DIALOG_DISMISSED))
      g_warning ("fart %s", error->message);
    goto out;
  }

 out:
  gtk_widget_destroy (dialog);
}

static void
add_provider_to_list (GisGoaPage *page, const char *provider_type)
{
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *checkmark;
  GtkWidget *account_label;
  GIcon *icon;
  gchar *markup, *provider_name;
  GoaProvider *provider;
  ProviderWidget *provider_widget;

  provider = goa_provider_get_for_provider_type (provider_type);
  if (provider == NULL)
    return;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set (box, "margin", 4, NULL);
  gtk_widget_set_hexpand (box, TRUE);

  icon = goa_provider_get_provider_icon (provider, NULL);
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  g_object_unref (icon);

  provider_name = goa_provider_get_provider_name (provider, NULL);
  markup = g_strdup_printf ("<b>%s</b>", provider_name);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  g_free (markup);
  g_free (provider_name);

  checkmark = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);

  account_label = gtk_label_new (NULL);

  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (box), checkmark, FALSE, FALSE, 8);
  gtk_box_pack_end (GTK_BOX (box), account_label, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (row), box);

  gtk_widget_show (label);
  gtk_widget_show (image);
  gtk_widget_show (box);
  gtk_widget_show (row);

  provider_widget = g_new0 (ProviderWidget, 1);
  provider_widget->page = page;
  provider_widget->provider = provider;
  provider_widget->row = row;
  provider_widget->checkmark = checkmark;
  provider_widget->account_label = account_label;

  g_object_set_data_full (G_OBJECT (row), "widget", provider_widget, g_free);

  g_hash_table_insert (priv->providers, (char *) provider_type, provider_widget);

  gtk_container_add (GTK_CONTAINER (priv->accounts_list), row);
}

static void
populate_provider_list (GisGoaPage *page)
{
  add_provider_to_list (page, "google");
  add_provider_to_list (page, "owncloud");
  add_provider_to_list (page, "windows_live");
  add_provider_to_list (page, "facebook");
}

static void
sync_visibility (GisGoaPage *page)
{
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  GisAssistant *assistant = gis_driver_get_assistant (GIS_PAGE (page)->driver);
  GNetworkMonitor *network_monitor = g_network_monitor_get_default ();
  gboolean visible;

  if (gis_assistant_get_current_page (assistant) == GIS_PAGE (page))
    return;

  visible = (priv->accounts_exist || g_network_monitor_get_network_available (network_monitor));
  gtk_widget_set_visible (GTK_WIDGET (page), visible);
}

static void
sync_accounts (GisGoaPage *page)
{
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  GList *accounts, *l;

  accounts = goa_client_get_accounts (priv->goa_client);

  for (l = accounts; l != NULL; l = l->next) {
    GoaObject *object = GOA_OBJECT (l->data);
    GoaAccount *account = goa_object_get_account (object);
    const char *account_type = goa_account_get_provider_type (account);
    ProviderWidget *provider_widget;

    provider_widget = g_hash_table_lookup (priv->providers, account_type);
    if (!provider_widget)
      continue;

    priv->accounts_exist = TRUE;

    if (provider_widget->displayed_account)
      continue;

    provider_widget->displayed_account = account;
    sync_provider_widget (provider_widget);
  }

  g_list_free_full (accounts, (GDestroyNotify) g_object_unref);

  sync_visibility (page);
  gis_page_set_skippable (GIS_PAGE (page), !priv->accounts_exist);
  gis_page_set_complete (GIS_PAGE (page), priv->accounts_exist);
}

static void
accounts_changed (GoaClient *client, GoaObject *object, gpointer user_data)
{
  GisGoaPage *page = GIS_GOA_PAGE (user_data);
  sync_accounts (page);
}

static void
network_status_changed (GNetworkMonitor *monitor,
                        gboolean         available,
                        gpointer         user_data)
{
  GisGoaPage *page = GIS_GOA_PAGE (user_data);
  sync_visibility (page);
}

static void
update_header_func (GtkListBoxRow *child,
                    GtkListBoxRow *before,
                    gpointer       user_data)
{
  GtkWidget *header;

  if (before == NULL)
    return;

  header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_list_box_row_set_header (child, header);
  gtk_widget_show (header);
}

static void
row_activated (GtkListBox    *box,
               GtkListBoxRow *row,
               GisGoaPage    *page)
{
  ProviderWidget *provider_widget;

  if (row == NULL)
    return;

  provider_widget = g_object_get_data (G_OBJECT (row), "widget");
  g_assert (provider_widget != NULL);
  g_assert (provider_widget->displayed_account == NULL);
  add_account_to_provider (provider_widget);
}

static void
gis_goa_page_constructed (GObject *object)
{
  GisGoaPage *page = GIS_GOA_PAGE (object);
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  GError *error = NULL;
  GNetworkMonitor *network_monitor = g_network_monitor_get_default ();

  G_OBJECT_CLASS (gis_goa_page_parent_class)->constructed (object);

  gis_page_set_skippable (GIS_PAGE (page), TRUE);

  priv->providers = g_hash_table_new (g_str_hash, g_str_equal);

  priv->goa_client = goa_client_new_sync (NULL, &error);

  if (priv->goa_client == NULL) {
    g_warning ("Failed to get a GoaClient: %s", error->message);
    g_error_free (error);
    return;
  }

  g_signal_connect (priv->goa_client, "account-added",
                    G_CALLBACK (accounts_changed), page);
  g_signal_connect (priv->goa_client, "account-removed",
                    G_CALLBACK (accounts_changed), page);
  g_signal_connect (network_monitor, "network-changed",
                    G_CALLBACK (network_status_changed), page);

  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->accounts_list),
                                update_header_func,
                                NULL, NULL);
  g_signal_connect (priv->accounts_list, "row-activated",
                    G_CALLBACK (row_activated), page);

  populate_provider_list (page);
  sync_accounts (page);
}

static void
gis_goa_page_dispose (GObject *object)
{
  GisGoaPage *page = GIS_GOA_PAGE (object);
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  GNetworkMonitor *network_monitor = g_network_monitor_get_default ();

  g_clear_object (&priv->goa_client);

  g_signal_handlers_disconnect_by_func (network_monitor, G_CALLBACK (network_status_changed), page);

  G_OBJECT_CLASS (gis_goa_page_parent_class)->dispose (object);
}

static void
gis_goa_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Online Accounts"));
}

static void
gis_goa_page_class_init (GisGoaPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-goa-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisGoaPage, accounts_list);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_goa_page_locale_changed;
  object_class->constructed = gis_goa_page_constructed;
  object_class->dispose = gis_goa_page_dispose;
}

static void
gis_goa_page_init (GisGoaPage *page)
{
  g_resources_register (goa_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_goa_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_GOA_PAGE,
                       "driver", driver,
                       NULL);
}
