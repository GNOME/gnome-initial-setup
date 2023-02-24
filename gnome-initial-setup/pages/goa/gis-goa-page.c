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

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gis-page-header.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#define VENDOR_GOA_GROUP "goa"
#define VENDOR_PROVIDERS_KEY "providers"

struct _GisGoaPagePrivate {
  GtkWidget *accounts_list;

  GoaClient *goa_client;
  GHashTable *providers;
  gboolean accounts_exist;
  char *window_export_handle;
};
typedef struct _GisGoaPagePrivate GisGoaPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisGoaPage, gis_goa_page, GIS_TYPE_PAGE);

struct _ProviderWidget {
  GisGoaPage *page;
  GVariant *provider;
  GoaAccount *displayed_account;

  GtkWidget *row;
  GtkWidget *arrow_icon;
};
typedef struct _ProviderWidget ProviderWidget;

G_GNUC_NULL_TERMINATED
static char *
run_goa_helper_sync (const char *command,
                     ...)
{
  g_autoptr(GPtrArray) argv = NULL;
  g_autofree char *output = NULL;
  g_autoptr(GError) error = NULL;
  const char *param;
  va_list args;
  int status;

  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_strdup (LIBEXECDIR "/gnome-initial-setup-goa-helper"));
  g_ptr_array_add (argv, g_strdup (command));

  va_start (args, command);
  while ((param = va_arg (args, const char*)) != NULL)
    g_ptr_array_add (argv, g_strdup (param));
  va_end (args);

  g_ptr_array_add (argv, NULL);

  if (!g_spawn_sync (NULL,
                     (char **) argv->pdata,
                     NULL,
                     0,
                     NULL,
                     NULL,
                     &output,
                     NULL,
                     &status,
                     &error))
    {
      g_warning ("Failed to run online accounts helper: %s", error->message);
      return NULL;
    }

  if (!g_spawn_check_exit_status (status, NULL))
    return NULL;

  if (output == NULL || *output == '\0')
    return NULL;

  return g_steal_pointer (&output);
}

static void
run_goa_helper_in_thread_func (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  g_autofree char *output = NULL;
  g_autoptr(GError) error = NULL;
  GPtrArray *argv = task_data;
  int status;

  g_spawn_sync (NULL,
                (char **) argv->pdata,
                NULL, 0, NULL, NULL,
                &output,
                NULL,
                &status,
                &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!g_spawn_check_exit_status (status, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&output), g_free);
}

static void
run_goa_helper_async (const gchar         *command,
                      const gchar         *param,
                      const gchar         *window_handle,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr(GPtrArray) argv = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_strdup (LIBEXECDIR "/gnome-initial-setup-goa-helper"));
  g_ptr_array_add (argv, g_strdup (command));
  g_ptr_array_add (argv, g_strdup (param));
  g_ptr_array_add (argv, g_strdup (window_handle));
  g_ptr_array_add (argv, NULL);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, run_goa_helper_async);
  g_task_set_task_data (task, g_steal_pointer (&argv), (GDestroyNotify) g_ptr_array_unref);
  g_task_run_in_thread (task, run_goa_helper_in_thread_func);
}

static void
sync_provider_widget (ProviderWidget *provider_widget)
{
  gboolean has_account = (provider_widget->displayed_account != NULL);

  gtk_widget_set_visible (provider_widget->arrow_icon, !has_account);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (provider_widget->row), !has_account);

  adw_action_row_set_subtitle (ADW_ACTION_ROW (provider_widget->row),
                               has_account ? goa_account_get_presentation_identity (provider_widget->displayed_account) : NULL);

}

static void
on_create_account_finish_cb (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autofree char *new_account_id = NULL;
  g_autoptr(GError) error = NULL;

  new_account_id = g_task_propagate_pointer (G_TASK (result), &error);

  if (error)
    g_warning ("Error creating account: %s", error->message);
}

static void
add_account_to_provider (ProviderWidget *provider_widget)
{
  GisGoaPage *page = provider_widget->page;
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  g_autofree char *provider_type = NULL;

  if (!priv->window_export_handle)
    return;

  g_variant_get (provider_widget->provider, "(ssviu)", &provider_type, NULL, NULL, NULL, NULL);

  run_goa_helper_async ("create-account",
                        provider_type,
                        priv->window_export_handle,
                        NULL,
                        on_create_account_finish_cb,
                        page);
}

static gboolean
is_gicon_symbolic (GtkWidget *widget,
                   GIcon     *icon)
{
  g_autoptr(GtkIconPaintable) icon_paintable = NULL;
  GtkIconTheme *icon_theme;

  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  icon_paintable = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                                   icon,
                                                   32,
                                                   gtk_widget_get_scale_factor (widget),
                                                   gtk_widget_get_direction (widget),
                                                   0);

  return icon_paintable && gtk_icon_paintable_is_symbolic (icon_paintable);
}

static void
add_provider_to_list (GisGoaPage *page, GVariant *provider)
{
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);
  g_autoptr(GIcon) provider_icon = NULL;
  g_autofree char *provider_name = NULL;
  g_autofree char *provider_type = NULL;
  g_autoptr(GVariant) icon_variant = NULL;
  ProviderWidget *provider_widget;
  GtkWidget *row;
  GtkWidget *image;
  GtkWidget *arrow_icon;

  g_variant_get (provider, "(ssviu)",
                 &provider_type,
                 &provider_name,
                 &icon_variant,
                 NULL,
                 NULL);

  row = adw_action_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), TRUE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), provider_name);

  provider_icon = g_icon_deserialize (icon_variant);
  image = gtk_image_new_from_gicon (provider_icon);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  if (is_gicon_symbolic (GTK_WIDGET (page), provider_icon))
    {
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_NORMAL);
      gtk_widget_add_css_class (image, "symbolic-circular");
    }
  else
    {
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
      gtk_widget_add_css_class (image, "lowres-icon");
    }

  arrow_icon = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), arrow_icon);

  provider_widget = g_new0 (ProviderWidget, 1);
  provider_widget->page = page;
  provider_widget->provider = g_variant_ref (provider);
  provider_widget->row = row;
  provider_widget->arrow_icon = arrow_icon;

  g_object_set_data_full (G_OBJECT (row), "widget", provider_widget, g_free);

  g_hash_table_insert (priv->providers, g_steal_pointer (&provider_type), provider_widget);

  gtk_list_box_append (GTK_LIST_BOX (priv->accounts_list), row);
}

static void
populate_provider_list (GisGoaPage *page)
{
  g_autoptr(GVariant) providers_variant = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *listed_providers = NULL;
  GVariantIter iter;
  GVariant *provider;
  g_auto(GStrv) conf_providers =
    gis_driver_conf_get_string_list (GIS_PAGE (page)->driver, VENDOR_GOA_GROUP, VENDOR_PROVIDERS_KEY, NULL);
  GStrv providers = conf_providers ? conf_providers :
    (gchar *[]) { "google", "owncloud", "windows_live", "facebook", NULL };

  listed_providers = run_goa_helper_sync ("list-providers", NULL);
  providers_variant = g_variant_parse (G_VARIANT_TYPE ("a(ssviu)"),
                                       listed_providers,
                                       NULL,
                                       NULL,
                                       &error);

  if (error)
    {
      g_warning ("Error listing providers: %s", error->message);
      gtk_widget_hide (GTK_WIDGET (page));
      return;
    }

  /* This code will read the keyfile containing vendor customization options and
   * look for options under the "goa" group, and supports the following keys:
   *   - providers (optional): list of online account providers to offer
   *
   * This is how this file might look on a vendor image:
   *
   *   [goa]
   *   providers=owncloud;imap_smtp
   */

  g_variant_iter_init (&iter, providers_variant);

  while ((provider = g_variant_iter_next_value (&iter)))
    {
      g_autofree gchar *id = NULL;

      g_variant_get (provider, "(ssviu)", &id, NULL, NULL, NULL, NULL);

      if (g_strv_contains ((const char * const *)providers, id))
        add_provider_to_list (page, provider);
    }
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

#ifdef GDK_WINDOWING_WAYLAND
static void
wayland_window_exported_cb (GdkToplevel *toplevel,
                            const char  *handle,
                            gpointer     data)

{
  GisGoaPage *page = data;
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);

  priv->window_export_handle = g_strdup_printf ("wayland:%s", handle);
}
#endif

static void
export_window_handle (GisGoaPage *page)
{
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (page));
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (native))))
    {
      GdkSurface *surface = gtk_native_get_surface (native);
      guint32 xid = (guint32) gdk_x11_surface_get_xid (surface);

      priv->window_export_handle = g_strdup_printf ("x11:%x", xid);
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (native))))
    {
      GdkSurface *surface = gtk_native_get_surface (native);

      gdk_wayland_toplevel_export_handle (GDK_TOPLEVEL (surface),
                                          wayland_window_exported_cb,
                                          page,
                                          NULL);
    }
#endif
}

static void
unexport_window_handle (GisGoaPage *page)
{
  GisGoaPagePrivate *priv = gis_goa_page_get_instance_private (page);

  if (!priv->window_export_handle)
    return;

#ifdef GDK_WINDOWING_WAYLAND
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (page));

  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (native))))
    {
      GdkSurface *surface = gtk_native_get_surface (native);
      gdk_wayland_toplevel_unexport_handle (GDK_TOPLEVEL (surface));
    }
#endif
}


static void
gis_goa_page_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gis_goa_page_parent_class)->realize (widget);

  export_window_handle (GIS_GOA_PAGE (widget));
}

static void
gis_goa_page_unrealize (GtkWidget *widget)
{
  unexport_window_handle (GIS_GOA_PAGE (widget));

  GTK_WIDGET_CLASS (gis_goa_page_parent_class)->unrealize (widget);
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

  priv->providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-goa-page.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GisGoaPage, accounts_list);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_goa_page_locale_changed;
  object_class->constructed = gis_goa_page_constructed;
  object_class->dispose = gis_goa_page_dispose;
  widget_class->realize = gis_goa_page_realize;
  widget_class->unrealize = gis_goa_page_unrealize;

  gis_add_style_from_resource ("/org/gnome/initial-setup/gis-goa-page.css");
}

static void
gis_goa_page_init (GisGoaPage *page)
{
  g_type_ensure (GIS_TYPE_PAGE_HEADER);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_goa_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_GOA_PAGE,
                       "driver", driver,
                       NULL);
}
