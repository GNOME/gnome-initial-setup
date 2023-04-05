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

#include "network-dialogs.h"

#include "gis-page-header.h"

typedef enum {
  NM_AP_SEC_UNKNOWN,
  NM_AP_SEC_NONE,
  NM_AP_SEC_WEP,
  NM_AP_SEC_WPA,
  NM_AP_SEC_WPA2
} NMAccessPointSecurity;

struct _GisNetworkPagePrivate {
  GtkWidget *network_list;
  GtkWidget *no_network_label;
  GtkWidget *no_network_spinner;
  GtkWidget *turn_on_label;
  GtkWidget *turn_on_switch;

  NMClient *nm_client;
  NMDevice *nm_device;
  gboolean refreshing;
  GtkSizeGroup *icons;

  guint refresh_timeout_id;

  /* TRUE if the page has ever been shown to the user. */
  gboolean ever_shown;
};
typedef struct _GisNetworkPagePrivate GisNetworkPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisNetworkPage, gis_network_page, GIS_TYPE_PAGE);

static GPtrArray *
get_strongest_unique_aps (const GPtrArray *aps)
{
  GBytes *ssid;
  GBytes *ssid_tmp;
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

    if (!ssid)
      continue;

    /* get already added list */
    for (j = 0; j < unique->len; j++) {
      ap_tmp = NM_ACCESS_POINT (g_ptr_array_index (unique, j));
      ssid_tmp = nm_access_point_get_ssid (ap_tmp);

      /* is this the same type and data? */
      if (ssid_tmp &&
          nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
                              g_bytes_get_data (ssid_tmp, NULL), g_bytes_get_size (ssid_tmp), TRUE)) {
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

  wa = gtk_list_box_row_get_child (a);
  wb = gtk_list_box_row_get_child (b);

  sa = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (wa), "strength"));
  sb = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (wb), "strength"));
  if (sa > sb) return -1;
  if (sb > sa) return 1;

  return 0;
}

static void
add_access_point (GisNetworkPage *page, NMAccessPoint *ap, NMAccessPoint *active)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  GBytes *ssid;
  GBytes *ssid_active = NULL;
  gchar *ssid_text;
  const gchar *object_path;
  gboolean activated, activating;
  guint security;
  guint strength;
  const gchar *icon_name;
  GtkWidget *row;
  GtkWidget *widget;
  GtkWidget *grid;
  GtkWidget *state_widget = NULL;

  ssid = nm_access_point_get_ssid (ap);
  object_path = nm_object_get_path (NM_OBJECT (ap));

  if (ssid == NULL)
    return;
  ssid_text = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));

  if (active)
    ssid_active = nm_access_point_get_ssid (active);
  if (ssid_active && nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
                                         g_bytes_get_data (ssid_active, NULL), g_bytes_get_size (ssid_active), TRUE)) {
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
  gtk_box_append (GTK_BOX (row), widget);

  if (activated) {
    state_widget = gtk_image_new_from_icon_name ("object-select-symbolic");
  } else if (activating) {
    state_widget = gtk_spinner_new ();
    gtk_spinner_start (GTK_SPINNER (state_widget));
  }

  if (state_widget) {
    gtk_widget_set_halign (state_widget, GTK_ALIGN_START);
    gtk_widget_set_valign (state_widget, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (state_widget, TRUE);
    gtk_box_append (GTK_BOX (row), state_widget);
  }

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
  gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
  gtk_size_group_add_widget (priv->icons, grid);
  gtk_box_append (GTK_BOX (row), grid);

  if (security != NM_AP_SEC_UNKNOWN &&
      security != NM_AP_SEC_NONE) {
    widget = gtk_image_new_from_icon_name ("network-wireless-encrypted-symbolic");
    gtk_grid_attach (GTK_GRID (grid), widget, 0, 0, 1, 1);
  }

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
  widget = gtk_image_new_from_icon_name (icon_name);
  gtk_widget_set_halign (widget, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 0, 1, 1);

  /* if this connection is the active one or is being activated, then make sure
   * it's sorted before all others */
  if (activating || activated)
    strength = G_MAXUINT;

  g_object_set_data (G_OBJECT (row), "object-path", (gpointer) object_path);
  g_object_set_data (G_OBJECT (row), "ssid", (gpointer) ssid);
  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (strength));

  gtk_list_box_append (GTK_LIST_BOX (priv->network_list), row);
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
  gtk_box_append (GTK_BOX (row), widget);

  g_object_set_data (G_OBJECT (row), "object-path", "ap-other...");
  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (0));

  gtk_list_box_append (GTK_LIST_BOX (priv->network_list), row);
}

static gboolean refresh_wireless_list (GisNetworkPage *page);

static void
cancel_periodic_refresh (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);

  if (priv->refresh_timeout_id == 0)
    return;

  g_debug ("Stopping periodic/scheduled Wi-Fi list refresh");

  g_clear_handle_id (&priv->refresh_timeout_id, g_source_remove);
}

static gboolean
refresh_again (gpointer user_data)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (user_data);
  refresh_wireless_list (page);
  return G_SOURCE_REMOVE;
}

static void
start_periodic_refresh (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  static const guint periodic_wifi_refresh_timeout_sec = 10;

  cancel_periodic_refresh (page);

  g_debug ("Starting periodic Wi-Fi list refresh (every %u secs)",
           periodic_wifi_refresh_timeout_sec);
  priv->refresh_timeout_id = g_timeout_add_seconds (periodic_wifi_refresh_timeout_sec,
                                                    refresh_again, page);
}

static gboolean
refresh_wireless_list (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  NMAccessPoint *active_ap = NULL;
  NMAccessPoint *ap;
  const GPtrArray *aps;
  GPtrArray *unique_aps;
  GtkWidget *child;
  guint i;
  gboolean enabled;

  g_debug ("Refreshing Wi-Fi networks list");

  priv->refreshing = TRUE;

  g_assert (NM_IS_DEVICE_WIFI (priv->nm_device));

  cancel_periodic_refresh (page);

  active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (priv->nm_device));

  while ((child = gtk_widget_get_first_child (priv->network_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (priv->network_list), child);

  aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (priv->nm_device));
  enabled = nm_client_wireless_get_enabled (priv->nm_client);

  if (aps == NULL || aps->len == 0) {
    gboolean hw_enabled;

    hw_enabled = nm_client_wireless_hardware_get_enabled (priv->nm_client);

    if (!enabled || !hw_enabled) {
      gtk_label_set_text (GTK_LABEL (priv->no_network_label), _("Wireless networking is disabled"));
      gtk_widget_set_visible (priv->no_network_label, TRUE);
      gtk_widget_set_visible (priv->no_network_spinner, FALSE);

      gtk_widget_set_visible (priv->turn_on_label, hw_enabled);
      gtk_widget_set_visible (priv->turn_on_switch, hw_enabled);
    } else {
      gtk_label_set_text (GTK_LABEL (priv->no_network_label), _("Checking for available wireless networks"));
      gtk_widget_set_visible (priv->no_network_spinner, TRUE);
      gtk_widget_set_visible (priv->no_network_label, TRUE);
      gtk_widget_set_visible (priv->turn_on_label, FALSE);
      gtk_widget_set_visible (priv->turn_on_switch, FALSE);
    }

    gtk_widget_set_visible (priv->network_list, FALSE);
    goto out;

  } else {
    gtk_widget_set_visible (priv->no_network_spinner, FALSE);
    gtk_widget_set_visible (priv->no_network_label, FALSE);
    gtk_widget_set_visible (priv->turn_on_label, FALSE);
    gtk_widget_set_visible (priv->turn_on_switch, FALSE);
    gtk_widget_set_visible (priv->network_list, TRUE);
  }

  unique_aps = get_strongest_unique_aps (aps);
  for (i = 0; i < unique_aps->len; i++) {
    ap = NM_ACCESS_POINT (g_ptr_array_index (unique_aps, i));
    add_access_point (page, ap, active_ap);
  }
  g_ptr_array_unref (unique_aps);
  add_access_point_other (page);

 out:

  if (enabled)
    start_periodic_refresh (page);

  priv->refreshing = FALSE;

  return G_SOURCE_REMOVE;
}

/* Avoid repeated calls to refreshing the wireless list by making it refresh at
 * most once per second */
static void
schedule_refresh_wireless_list (GisNetworkPage *page)
{
  static const guint refresh_wireless_list_timeout_sec = 1;
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);

  cancel_periodic_refresh (page);

  g_debug ("Delaying Wi-Fi list refresh (for %u sec)",
           refresh_wireless_list_timeout_sec);

  priv->refresh_timeout_id = g_timeout_add_seconds (refresh_wireless_list_timeout_sec,
                                                    (GSourceFunc) refresh_wireless_list,
                                                    page);
}

static void
connection_activate_cb (GObject *object,
                        GAsyncResult *result,
                        gpointer user_data)
{
  NMClient *client = NM_CLIENT (object);
  NMActiveConnection *connection = NULL;
  g_autoptr(GError) error = NULL;

  connection = nm_client_activate_connection_finish (client, result, &error);
  if (connection != NULL) {
    g_clear_object (&connection);
  } else {
    /* failed to activate */
    g_warning ("Failed to activate a connection: %s", error->message);
  }
}

static void
connection_add_activate_cb (GObject *object,
                            GAsyncResult *result,
                            gpointer user_data)
{
  NMClient *client = NM_CLIENT (object);
  NMActiveConnection *connection = NULL;
  g_autoptr(GError) error = NULL;

  connection = nm_client_add_and_activate_connection_finish (client, result, &error);
  if (connection != NULL) {
    g_clear_object (&connection);
  } else {
    /* failed to activate */
    g_warning ("Failed to add and activate a connection: %s", error->message);
  }
}

static void
connect_to_hidden_network (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (page));

  cc_network_panel_connect_to_hidden_network (GTK_WIDGET (root), priv->nm_client);
}

static void
row_activated (GtkListBox *box,
               GtkListBoxRow *row,
               GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  gchar *object_path;
  const GPtrArray *list;
  GPtrArray *filtered;
  NMConnection *connection;
  NMConnection *connection_to_activate;
  NMSettingWireless *setting;
  GBytes *ssid;
  GBytes *ssid_target;
  GtkWidget *child;
  int i;

  if (priv->refreshing)
    return;

  child = gtk_list_box_row_get_child (row);
  object_path = g_object_get_data (G_OBJECT (child), "object-path");
  ssid_target = g_object_get_data (G_OBJECT (child), "ssid");

  if (g_strcmp0 (object_path, "ap-other...") == 0) {
    connect_to_hidden_network (page);
    goto out;
  }

  list = nm_client_get_connections (priv->nm_client);
  filtered = nm_device_filter_connections (priv->nm_device, list);

  connection_to_activate = NULL;

  for (i = 0; i < filtered->len; i++) {
    connection = NM_CONNECTION (filtered->pdata[i]);
    setting = nm_connection_get_setting_wireless (connection);
    if (!NM_IS_SETTING_WIRELESS (setting))
      continue;

    ssid = nm_setting_wireless_get_ssid (setting);
    if (ssid == NULL)
      continue;

    if (nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
                            g_bytes_get_data (ssid_target, NULL), g_bytes_get_size (ssid_target), TRUE)) {
      connection_to_activate = connection;
      break;
    }
  }
  g_ptr_array_unref (filtered);

  if (connection_to_activate != NULL) {
    nm_client_activate_connection_async (priv->nm_client,
                                         connection_to_activate,
                                         priv->nm_device, NULL,
                                         NULL,
                                         connection_activate_cb, page);
    return;
  }

  nm_client_add_and_activate_connection_async (priv->nm_client,
                                               NULL,
                                               priv->nm_device, object_path,
                                               NULL,
                                               connection_add_activate_cb, page);

 out:
  schedule_refresh_wireless_list (page);
}

static void
connection_state_changed (NMActiveConnection *c, GParamSpec *pspec, GisNetworkPage *page)
{
  schedule_refresh_wireless_list (page);
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
}

static void
sync_complete (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  gboolean has_device;
  gboolean activated;
  gboolean visible;

  has_device = priv->nm_device != NULL;
  activated = priv->nm_device != NULL
    && nm_device_get_state (priv->nm_device) == NM_DEVICE_STATE_ACTIVATED;

  if (priv->ever_shown) {
    visible = TRUE;
  } else if (!has_device) {
    g_debug ("No network device found, hiding network page");
    visible = FALSE;
  } else if (activated) {
    g_debug ("Activated network device found, hiding network page");
    visible = FALSE;
  } else {
    visible = TRUE;
  }

  gis_page_set_complete (GIS_PAGE (page), activated);
  gtk_widget_set_visible (GTK_WIDGET (page), visible);

  if (has_device)
    schedule_refresh_wireless_list (page);
}

static void
device_state_changed (GObject *object, GParamSpec *param, GisNetworkPage *page)
{
  sync_complete (page);
}

static void
find_best_device (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  const GPtrArray *devices;
  guint i;

  /* FIXME: deal with multiple devices and devices being removed */
  if (priv->nm_device != NULL) {
    g_debug ("Already showing network device %s",
             nm_device_get_description (priv->nm_device));
    return;
  }

  devices = nm_client_get_devices (priv->nm_client);
  g_return_if_fail (devices != NULL);
  for (i = 0; i < devices->len; i++) {
    NMDevice *device = g_ptr_array_index (devices, i);

    if (!nm_device_get_managed (device))
      continue;

    if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI) {
      /* FIXME deal with multiple, dynamic devices */
      priv->nm_device = g_object_ref (device);
      g_debug ("Showing network device %s",
               nm_device_get_description (priv->nm_device));

      g_signal_connect (priv->nm_device, "notify::state",
                        G_CALLBACK (device_state_changed), page);
      g_signal_connect (priv->nm_client, "notify::active-connections",
                        G_CALLBACK (active_connections_changed), page);

      break;
    }
  }

  sync_complete (page);
}
static void
device_notify_managed (NMDevice   *device,
                       GParamSpec *param,
                       void       *user_data)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (user_data);

  find_best_device (page);
}

static void
client_device_added (NMClient *client,
                     NMDevice *device,
                     void     *user_data)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (user_data);

  g_signal_connect_object (device,
                           "notify::managed",
                           G_CALLBACK (device_notify_managed),
                           G_OBJECT (page),
                           0);

  find_best_device (page);
}

static void
client_device_removed (NMClient *client,
                       NMDevice *device,
                       void     *user_data)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (user_data);

  /* TODO: reset page if priv->nm_device == device */
  g_signal_handlers_disconnect_by_func (device, device_notify_managed, page);

  find_best_device (page);
}

static void
monitor_network_devices (GisNetworkPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  const GPtrArray *devices;
  guint i;

  g_assert (priv->nm_client != NULL);
  devices = nm_client_get_devices (priv->nm_client);
  g_return_if_fail (devices != NULL);
  for (i = 0; devices != NULL && i < devices->len; i++) {
    NMDevice *device = g_ptr_array_index (devices, i);

    g_signal_connect_object (device,
                             "notify::managed",
                             G_CALLBACK (device_notify_managed),
                             G_OBJECT (page),
                             0);
  }

  g_signal_connect_object (priv->nm_client,
                           "device-added",
                           G_CALLBACK (client_device_added),
                           G_OBJECT (page),
                           0);
  g_signal_connect_object (priv->nm_client,
                           "device-removed",
                           G_CALLBACK (client_device_removed),
                           G_OBJECT (page),
                           0);

  find_best_device (page);
}

static void
gis_network_page_constructed (GObject *object)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (object);
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);
  g_autoptr(GError) error = NULL;

  G_OBJECT_CLASS (gis_network_page_parent_class)->constructed (object);

  gis_page_set_skippable (GIS_PAGE (page), TRUE);

  priv->ever_shown = g_getenv ("GIS_ALWAYS_SHOW_NETWORK_PAGE") != NULL;
  priv->icons = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->network_list), GTK_SELECTION_NONE);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->network_list), ap_sort, NULL, NULL);
  g_signal_connect (priv->network_list, "row-activated",
                    G_CALLBACK (row_activated), page);

  priv->nm_client = nm_client_new (NULL, &error);
  if (!priv->nm_client) {
    g_warning ("Can't create NetworkManager client, hiding network page: %s",
               error->message);
    sync_complete (page);
    return;
  }

  g_object_bind_property (priv->nm_client, "wireless-enabled",
                          priv->turn_on_switch, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  monitor_network_devices (page);
}

static void
gis_network_page_dispose (GObject *object)
{
  GisNetworkPage *page = GIS_NETWORK_PAGE (object);
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (page);

  g_clear_object (&priv->nm_client);
  g_clear_object (&priv->nm_device);
  g_clear_object (&priv->icons);

  cancel_periodic_refresh (page);

  G_OBJECT_CLASS (gis_network_page_parent_class)->dispose (object);
}

static void
gis_network_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Network"));
}

static void
gis_network_page_shown (GisPage *page)
{
  GisNetworkPagePrivate *priv = gis_network_page_get_instance_private (GIS_NETWORK_PAGE (page));

  priv->ever_shown = TRUE;
}

static void
gis_network_page_class_init (GisNetworkPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-network-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, network_list);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, no_network_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, no_network_spinner);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, turn_on_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisNetworkPage, turn_on_switch);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_network_page_locale_changed;
  page_class->shown = gis_network_page_shown;
  object_class->constructed = gis_network_page_constructed;
  object_class->dispose = gis_network_page_dispose;
}

static void
gis_network_page_init (GisNetworkPage *page)
{
  g_type_ensure (GIS_TYPE_PAGE_HEADER);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_network_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_NETWORK_PAGE,
                       "driver", driver,
                       NULL);
}
