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

/* Network page {{{1 */

#define PAGE_ID "network"

#include "config.h"
#include "network-resources.h"
#include "gis-network-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#include <nm-client.h>
#include <nm-device-wifi.h>
#include <nm-access-point.h>
#include <nm-utils.h>
#include <nm-remote-settings.h>

#include "network-dialogs.h"

typedef enum {
  NM_AP_SEC_UNKNOWN,
  NM_AP_SEC_NONE,
  NM_AP_SEC_WEP,
  NM_AP_SEC_WPA,
  NM_AP_SEC_WPA2
} NMAccessPointSecurity;

struct _GisNetworkPagePrivate {
  GtkWidget *network_list;
  GtkWidget *scrolled_window;
  GtkWidget *no_network_label;
  GtkWidget *no_network_spinner;

  NMClient *nm_client;
  NMRemoteSettings *nm_settings;
  NMDevice *nm_device;
  gboolean refreshing;
  GtkSizeGroup *icons;

  guint refresh_timeout_id;
};
typedef struct _GisNetworkPagePrivate GisNetworkPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisNetworkPage, gis_network_page, GIS_TYPE_PAGE);

static GPtrArray *
get_strongest_unique_aps (const GPtrArray *aps)
{
  const GByteArray *ssid;
  const GByteArray *ssid_tmp;
  GPtrArray *unique = NULL;
  gboolean add_ap;
  guint i;
  guint j;
  NMAccessPoint *ap;
  NMAccessPoint *ap_tmp;

  /* we will have multiple entries for typical hotspots,
   * just keep the one with the strongest signal
   */
  unique = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  if (aps == NULL)
    goto out;

  for (i = 0; i < aps->len; i++) {
    ap = NM_ACCESS_POINT (g_ptr_array_index (aps, i));
    ssid = nm_access_point_get_ssid (ap);
    add_ap = TRUE;

    /* get already added list */
    for (j = 0; j < unique->len; j++) {
      ap_tmp = NM_ACCESS_POINT (g_ptr_array_index (unique, j));
      ssid_tmp = nm_access_point_get_ssid (ap_tmp);

      /* is this the same type and data? */
      if (nm_utils_same_ssid (ssid, ssid_tmp, TRUE)) {
        /* the new access point is stronger */
        if (nm_access_point_get_strength (ap) >
            nm_access_point_get_strength (ap_tmp)) {
          g_ptr_array_remove (unique, ap_tmp);
          add_ap = TRUE;
        } else {
          add_ap = FALSE;
        }
        break;
      }
    }
    if (add_ap) {
      g_ptr_array_add (unique, g_object_ref (ap));
    }
  }

 out:
  return unique;
}

static guint
get_access_point_security (NMAccessPoint *ap)
{
  NM80211ApFlags flags;
  NM80211ApSecurityFlags wpa_flags;
  NM80211ApSecurityFlags rsn_flags;
  guint type;

  flags = nm_access_point_get_flags (ap);
  wpa_flags = nm_access_point_get_wpa_flags (ap);
  rsn_flags = nm_access_point_get_rsn_flags (ap);

  if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
      wpa_flags == NM_802_11_AP_SEC_NONE &&
      rsn_flags == NM_802_11_AP_SEC_NONE)
    type = NM_AP_SEC_NONE;
  else if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
           wpa_flags == NM_802_11_AP_SEC_NONE &&
           rsn_flags == NM_802_11_AP_SEC_NONE)
    type = NM_AP_SEC_WEP;
  else if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
           wpa_flags != NM_802_11_AP_SEC_NONE &&
           rsn_flags != NM_802_11_AP_SEC_NONE)
    type = NM_AP_SEC_WPA;
  else
    type = NM_AP_SEC_WPA2;

  return type;
}

static gint
ap_sort (GtkListBoxRow *a,
         GtkListBoxRow *b,
         gpointer data)
{
  GtkWidget *wa, *wb;
  guint sa, sb;

  wa = gtk_bin_get_child (GTK_BIN (a));
  wb = gtk_bin_get_child (GTK_BIN (b));

  sa = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (wa), "strength"));
  sb = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (wb), "strength"));
  if (sa > sb) return -1;
  if (sb > sa) return 1;

  return 0;
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
add_access_point (GisNetworkPage *page, NMAccessPoint *ap, NMAccessPoint *active)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  const GByteArray *ssid;
  gchar *ssid_text;
  const gchar *object_path;
  gboolean activated, activating;
  guint security;
  guint strength;
  const gchar *icon_name;
  GtkWidget *row;
  GtkWidget *widget;
  GtkWidget *box;

  ssid = nm_access_point_get_ssid (ap);
  object_path = nm_object_get_path (NM_OBJECT (ap));

  if (ssid == NULL)
    return;
  ssid_text = nm_utils_ssid_to_utf8 (ssid);

  if (active &&
      nm_utils_same_ssid (ssid, nm_access_point_get_ssid (active), TRUE)) {
    switch (nm_device_get_state (priv->nm_device))
      {
      case NM_DEVICE_STATE_PREPARE:
      case NM_DEVICE_STATE_CONFIG:
      case NM_DEVICE_STATE_NEED_AUTH:
      case NM_DEVICE_STATE_IP_CONFIG:
      case NM_DEVICE_STATE_SECONDARIES:
        activated = FALSE;
        activating = TRUE;
        break;
      case NM_DEVICE_STATE_ACTIVATED:
        activated = TRUE;
        activating = FALSE;
        break;
      default:
        activated = FALSE;
        activating = FALSE;
        break;
      }
  } else {
    activated = FALSE;
    activating = FALSE;
  }

  security = get_access_point_security (ap);
  strength = nm_access_point_get_strength (ap);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (row, 12);
  gtk_widget_set_margin_end (row, 12);
  widget = gtk_label_new (ssid_text);
  gtk_widget_set_margin_top (widget, 12);
  gtk_widget_set_margin_bottom (widget, 12);
  gtk_box_pack_start (GTK_BOX (row), widget, FALSE, FALSE, 0);

  if (activated) {
    widget = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (row), widget, FALSE, FALSE, 0);
  }

  widget = gtk_spinner_new ();
  gtk_widget_set_no_show_all (widget, TRUE);
  if (activating) {
    gtk_widget_show (widget);
    gtk_spinner_start (GTK_SPINNER (widget));
  }
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (row), widget, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (row), gtk_label_new (""), TRUE, FALSE, 0);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
  gtk_size_group_add_widget (priv->icons, box);
  gtk_box_pack_start (GTK_BOX (row), box, FALSE, FALSE, 0);

  if (security != NM_AP_SEC_UNKNOWN &&
      security != NM_AP_SEC_NONE) {
    widget = gtk_image_new_from_icon_name ("network-wireless-encrypted-symbolic", GTK_ICON_SIZE_MENU);
  } else {
    widget = gtk_label_new ("");
  }
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  if (strength < 20)
    icon_name = "network-wireless-signal-none-symbolic";
  else if (strength < 40)
    icon_name = "network-wireless-signal-weak-symbolic";
  else if (strength < 50)
    icon_name = "network-wireless-signal-ok-symbolic";
  else if (strength < 80)
    icon_name = "network-wireless-signal-good-symbolic";
  else
    icon_name = "network-wireless-signal-excellent-symbolic";
  widget = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  gtk_widget_show_all (row);

  g_object_set_data (G_OBJECT (row), "object-path", (gpointer) object_path);
  g_object_set_data (G_OBJECT (row), "ssid", (gpointer) ssid);
  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (strength));

  gtk_container_add (GTK_CONTAINER (priv->network_list), row);
}

static void
add_access_point_other (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  GtkWidget *row;
  GtkWidget *widget;

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (row, 12);
  gtk_widget_set_margin_end (row, 12);
  widget = gtk_label_new (C_("Wireless access point", "Other…"));
  gtk_widget_set_margin_top (widget, 12);
  gtk_widget_set_margin_bottom (widget, 12);
  gtk_box_pack_start (GTK_BOX (row), widget, FALSE, FALSE, 0);
  gtk_widget_show_all (row);

  g_object_set_data (G_OBJECT (row), "object-path", "ap-other...");
  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (0));

  gtk_container_add (GTK_CONTAINER (priv->network_list), row);
}

static void refresh_wireless_list (GisNetworkPage *page);

static gboolean
refresh_again (gpointer user_data)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (user_data);
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);

  refresh_wireless_list (page);
  return G_SOURCE_REMOVE;
}

static void
refresh_wireless_list (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  NMAccessPoint *active_ap = NULL;
  NMAccessPoint *ap;
  const GPtrArray *aps;
  GPtrArray *unique_aps;
  guint i;
  GList *children, *l;

  priv->refreshing = TRUE;

  g_assert (NM_IS_DEVICE_WIFI (priv->nm_device));

  if (priv->refresh_timeout_id != 0)
    {
      g_source_remove (priv->refresh_timeout_id);
      priv->refresh_timeout_id = 0;
    }

  active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (priv->nm_device));

  children = gtk_container_get_children (GTK_CONTAINER (priv->network_list));
  for (l = children; l; l = l->next)
    gtk_container_remove (GTK_CONTAINER (priv->network_list), l->data);
  g_list_free (children);

  aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (priv->nm_device));

  if (aps == NULL || aps->len == 0) {
    gtk_label_set_text (GTK_LABEL (priv->no_network_label), _("Checking for available wireless networks"));
    gtk_widget_show (priv->no_network_spinner);
    gtk_widget_show (priv->no_network_label);
    gtk_widget_hide (priv->scrolled_window);
    priv->refresh_timeout_id = g_timeout_add_seconds (1, refresh_again, page);

    goto out;
  } else {
    gtk_widget_hide (priv->no_network_spinner);
    gtk_widget_hide (priv->no_network_label);
    gtk_widget_show (priv->scrolled_window);
  }

  unique_aps = get_strongest_unique_aps (aps);
  for (i = 0; i < unique_aps->len; i++) {
    ap = NM_ACCESS_POINT (g_ptr_array_index (unique_aps, i));
    add_access_point (page, ap, active_ap);
  }
  g_ptr_array_unref (unique_aps);
  add_access_point_other (page);

 out:
  priv->refreshing = FALSE;
}

static void
connection_activate_cb (NMClient *client,
                        NMActiveConnection *connection,
                        GError *error,
                        gpointer user_data)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (user_data);

  if (connection == NULL) {
    /* failed to activate */
    refresh_wireless_list (page);
  }
}

static void
connection_add_activate_cb (NMClient *client,
                            NMActiveConnection *connection,
                            const char *path,
                            GError *error,
                            gpointer user_data)
{
  connection_activate_cb (client, connection, error, user_data);
}

static void
connect_to_hidden_network (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  cc_network_panel_connect_to_hidden_network (gtk_widget_get_toplevel (GTK_WIDGET (page)),
                                              priv->nm_client,
                                              priv->nm_settings);
}

static void
row_activated (GtkListBox *box,
               GtkListBoxRow *row,
               GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  gchar *object_path;
  GSList *list, *filtered, *l;
  NMConnection *connection;
  NMConnection *connection_to_activate;
  NMSettingWireless *setting;
  const GByteArray *ssid_target;
  const GByteArray *ssid;
  GtkWidget *child;

  if (priv->refreshing)
    return;

  child = gtk_bin_get_child (GTK_BIN (row));
  object_path = g_object_get_data (G_OBJECT (child), "object-path");
  ssid_target = g_object_get_data (G_OBJECT (child), "ssid");

  if (g_strcmp0 (object_path, "ap-other...") == 0) {
    connect_to_hidden_network (page);
    goto out;
  }

  list = nm_remote_settings_list_connections (priv->nm_settings);
  filtered = nm_device_filter_connections (priv->nm_device, list);

  connection_to_activate = NULL;

  for (l = filtered; l; l = l->next) {
    connection = NM_CONNECTION (l->data);
    setting = nm_connection_get_setting_wireless (connection);
    if (!NM_IS_SETTING_WIRELESS (setting))
      continue;

    ssid = nm_setting_wireless_get_ssid (setting);
    if (ssid == NULL)
      continue;

    if (nm_utils_same_ssid (ssid, ssid_target, TRUE)) {
      connection_to_activate = connection;
      break;
    }
  }
  g_slist_free (list);
  g_slist_free (filtered);

  if (connection_to_activate != NULL) {
    nm_client_activate_connection (priv->nm_client,
                                   connection_to_activate,
                                   priv->nm_device, NULL,
                                   connection_activate_cb, page);
    goto out;
  }

  nm_client_add_and_activate_connection (priv->nm_client,
                                         NULL,
                                         priv->nm_device, object_path,
                                         connection_add_activate_cb, page);

 out:
  refresh_wireless_list (page);
}

static void
connection_state_changed (NMActiveConnection *c, GParamSpec *pspec, GisNetworkPage *page)
{
  refresh_wireless_list (page);
}

static void
active_connections_changed (NMClient *client, GParamSpec *pspec, GisNetworkPage *page)
{
  const GPtrArray *connections;
  guint i;

  connections = nm_client_get_active_connections (client);
  for (i = 0; connections && (i < connections->len); i++) {
    NMActiveConnection *connection;

    connection = g_ptr_array_index (connections, i);
    if (!g_object_get_data (G_OBJECT (connection), "has-state-changed-handler")) {
      g_signal_connect (connection, "notify::state",
                        G_CALLBACK (connection_state_changed), page);
      g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (1));
    }
  }

  refresh_wireless_list (page);
}

static void
sync_complete (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  gboolean activated;

  activated = (nm_device_get_state (priv->nm_device) == NM_DEVICE_STATE_ACTIVATED);
  gis_page_set_complete (GIS_PAGE (page), activated);
}

static void
device_state_changed (GObject *object, GParamSpec *param, GisNetworkPage *page)
{
  sync_complete (page);
}

static void
gis_network_page_constructed (GObject *object)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (object);
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  const GPtrArray *devices;
  NMDevice *device;
  guint i;
  gboolean visible = FALSE;

  G_OBJECT_CLASS (gis_network_page_parent_class)->constructed (object);

  priv->icons = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  priv->nm_client = nm_client_new ();

  devices = nm_client_get_devices (priv->nm_client);
  if (devices) {
    for (i = 0; i < devices->len; i++) {
      device = g_ptr_array_index (devices, i);

      if (!nm_device_get_managed (device))
        continue;

      if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI) {
        /* FIXME deal with multiple, dynamic devices */
        priv->nm_device = g_object_ref (device);
        break;
      }
    }
  }

  if (priv->nm_device == NULL) {
    g_debug ("No network device found, hiding network page");
    goto out;
  }

  if (nm_device_get_state (priv->nm_device) == NM_DEVICE_STATE_ACTIVATED) {
    g_debug ("Activated network device found, hiding network page");
    goto out;
  }

  visible = TRUE;
  priv->nm_settings = nm_remote_settings_new (NULL);

  g_signal_connect (priv->nm_device, "notify::state",
                    G_CALLBACK (device_state_changed), page);
  g_signal_connect (priv->nm_client, "notify::active-connections",
                    G_CALLBACK (active_connections_changed), page);

  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->network_list), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->network_list), update_header_func, NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->network_list), ap_sort, NULL, NULL);
  g_signal_connect (priv->network_list, "row-activated",
                    G_CALLBACK (row_activated), page);

  refresh_wireless_list (page);
  sync_complete (page);

  gis_page_set_skippable (GIS_PAGE (page), TRUE);

 out:
  gtk_widget_set_visible (GTK_WIDGET (page), visible);
}

static void
gis_network_page_dispose (GObject *object)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (object);
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);

  g_clear_object (&priv->nm_client);
  g_clear_object (&priv->nm_settings);
  g_clear_object (&priv->nm_device);
  g_clear_object (&priv->icons);

  if (priv->refresh_timeout_id != 0)
    {
      g_source_remove (priv->refresh_timeout_id);
      priv->refresh_timeout_id = 0;
    }

  G_OBJECT_CLASS (gis_network_page_parent_class)->dispose (object);
}

static void
gis_network_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Network"));
}

static void
gis_network_page_class_init (GisNetworkPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-network-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, network_list);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, scrolled_window);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, no_network_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, no_network_spinner);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_network_page_locale_changed;
  object_class->constructed = gis_network_page_constructed;
  object_class->dispose = gis_network_page_dispose;
}

static void
gis_network_page_init (GisNetworkPage *page)
{
  g_resources_register (network_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_prepare_network_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_NETWORK_PAGE,
                                     "driver", driver,
                                     NULL));
}
