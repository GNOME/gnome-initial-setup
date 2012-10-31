/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#define PAGE_ID "network"

/* Network page {{{1 */

#include "config.h"
#include "gis-network-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#include <nm-client.h>
#include <nm-device-wifi.h>
#include <nm-access-point.h>
#include <nm-utils.h>
#include <nm-remote-settings.h>

#include "panel-cell-renderer-signal.h"
#include "panel-cell-renderer-mode.h"
#include "panel-cell-renderer-security.h"

typedef struct _NetworkData NetworkData;

#define OBJ(type,name) ((type)gtk_builder_get_object(data->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

struct _NetworkData {
  SetupData *setup;

  GtkBuilder *builder;

  /* network data */
  NMClient *nm_client;
  NMRemoteSettings *nm_settings;
  NMDevice *nm_device;
  GtkListStore *ap_list;
  gboolean refreshing;

  GtkTreeRowReference *row;
  guint pulse;
};

enum {
  PANEL_WIRELESS_COLUMN_ID,
  PANEL_WIRELESS_COLUMN_TITLE,
  PANEL_WIRELESS_COLUMN_STRENGTH,
  PANEL_WIRELESS_COLUMN_MODE,
  PANEL_WIRELESS_COLUMN_SECURITY,
  PANEL_WIRELESS_COLUMN_ACTIVATING,
  PANEL_WIRELESS_COLUMN_ACTIVE,
  PANEL_WIRELESS_COLUMN_PULSE
};

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

static gboolean
bump_pulse (gpointer user_data)
{
  NetworkData *data = user_data;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeModel *model;

  data->pulse++;

  if (data->refreshing || !gtk_tree_row_reference_valid (data->row))
    return FALSE;

  model = (GtkTreeModel *)data->ap_list;
  path = gtk_tree_row_reference_get_path (data->row);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_list_store_set (data->ap_list, &iter,
                      PANEL_WIRELESS_COLUMN_PULSE, data->pulse,
                      -1);

  return TRUE;
}

static void
add_access_point (NetworkData *data, NMAccessPoint *ap, NMAccessPoint *active)
{
  const GByteArray *ssid;
  const gchar *ssid_text;
  const gchar *object_path;
  GtkTreeIter iter;
  gboolean activated, activating;

  ssid = nm_access_point_get_ssid (ap);
  object_path = nm_object_get_path (NM_OBJECT (ap));

  if (ssid == NULL)
    return;
  ssid_text = nm_utils_escape_ssid (ssid->data, ssid->len);

  if (active &&
      nm_utils_same_ssid (ssid, nm_access_point_get_ssid (active), TRUE)) {
    switch (nm_device_get_state (data->nm_device))
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

  gtk_list_store_append (data->ap_list, &iter);
  gtk_list_store_set (data->ap_list, &iter,
                      PANEL_WIRELESS_COLUMN_ID, object_path,
                      PANEL_WIRELESS_COLUMN_TITLE, ssid_text,
                      PANEL_WIRELESS_COLUMN_STRENGTH, nm_access_point_get_strength (ap),
                      PANEL_WIRELESS_COLUMN_MODE, nm_access_point_get_mode (ap),
                      PANEL_WIRELESS_COLUMN_SECURITY, get_access_point_security (ap),
                      PANEL_WIRELESS_COLUMN_ACTIVATING, activating,
                      PANEL_WIRELESS_COLUMN_ACTIVE, activated,
                      PANEL_WIRELESS_COLUMN_PULSE, data->pulse,
                      -1);
  if (activating) {
    GtkTreePath *path;
    GtkTreeModel *model;

    model = (GtkTreeModel*)data->ap_list;
    path = gtk_tree_model_get_path (model, &iter);
    data->row = gtk_tree_row_reference_new (model, path);
    gtk_tree_path_free (path);

    g_timeout_add (80, bump_pulse, data);
  }

}

static void
add_access_point_other (NetworkData *data)
{
  GtkTreeIter iter;

  gtk_list_store_append (data->ap_list, &iter);
  gtk_list_store_set (data->ap_list, &iter,
                      PANEL_WIRELESS_COLUMN_ID, "ap-other...",
                      /* TRANSLATORS: this is when the access point is not listed
                       *                           * in the dropdown (or hidden) and the user has to select
                       *                           * another entry manually */

                      PANEL_WIRELESS_COLUMN_TITLE, C_("Wireless access point", "Other..."),
                      /* always last */
                      PANEL_WIRELESS_COLUMN_STRENGTH, 0,
                      PANEL_WIRELESS_COLUMN_MODE, NM_802_11_MODE_UNKNOWN,
                      PANEL_WIRELESS_COLUMN_SECURITY, NM_AP_SEC_UNKNOWN,
                      PANEL_WIRELESS_COLUMN_ACTIVATING, FALSE,
                      PANEL_WIRELESS_COLUMN_ACTIVE, FALSE,
                      PANEL_WIRELESS_COLUMN_PULSE, data->pulse,
                      -1);
}

static void
select_and_scroll_to_ap (NetworkData *data, NMAccessPoint *ap)
{
  GtkTreeModel *model;
  GtkTreeView *tv;
  GtkTreeViewColumn *col;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkTreePath *path;
  gchar *ssid_target;
  const GByteArray *ssid;
  const gchar *ssid_text;

  model = (GtkTreeModel *)data->ap_list;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  tv = OBJ(GtkTreeView*, "network-list");
  col = OBJ(GtkTreeViewColumn*, "network-list-column");
  selection = OBJ(GtkTreeSelection*, "network-list-selection");

  ssid = nm_access_point_get_ssid (ap);
  ssid_text = nm_utils_escape_ssid (ssid->data, ssid->len);

  do {
    gtk_tree_model_get (model, &iter,
                        PANEL_WIRELESS_COLUMN_TITLE, &ssid_target,
                        -1);
    if (g_strcmp0 (ssid_target, ssid_text) == 0) {
      g_free (ssid_target);
      gtk_tree_selection_select_iter (selection, &iter);
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_scroll_to_cell (tv, path, col, FALSE, 0, 0);
      gtk_tree_path_free (path);
      break;
    }
    g_free (ssid_target);

  } while (gtk_tree_model_iter_next (model, &iter));
}

static void refresh_wireless_list (NetworkData *data);

static gboolean
refresh_again (gpointer user_data)
{
  NetworkData *data = user_data;

  refresh_wireless_list (data);

  return FALSE;
}

static void
refresh_without_device (NetworkData *data)
{
  GtkWidget *label;
  GtkWidget *spinner;
  GtkWidget *swin;

  swin = WID("network-scrolledwindow");
  label = WID("no-network-label");
  spinner = WID("no-network-spinner");

  if (nm_client_get_state (data->nm_client) == NM_STATE_CONNECTED_GLOBAL)
    /* advance page */
    gis_assistant_next_page (gis_get_assistant (data->setup));
  if (data->nm_device != NULL)
    gtk_label_set_text (GTK_LABEL (label), _("Network is not available."));
  else
    gtk_label_set_text (GTK_LABEL (label), _("No network devices found."));

  gtk_widget_hide (swin);
  gtk_widget_hide (spinner);
  gtk_widget_show (label);
}

static void
refresh_wireless_list (NetworkData *data)
{
  NMDeviceState state = NM_DEVICE_STATE_UNAVAILABLE;
  NMAccessPoint *active_ap = NULL;
  NMAccessPoint *ap;
  const GPtrArray *aps;
  GPtrArray *unique_aps;
  guint i;
  GtkWidget *label;
  GtkWidget *spinner;
  GtkWidget *swin;

  data->refreshing = TRUE;

  if (NM_IS_DEVICE_WIFI (data->nm_device)) {
    state = nm_device_get_state (data->nm_device);

    active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (data->nm_device));

    gtk_tree_view_set_model (OBJ(GtkTreeView*, "network-list"), NULL);
    gtk_list_store_clear (data->ap_list);
    if (data->row) {
      gtk_tree_row_reference_free (data->row);
      data->row = NULL;
    }

    aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (data->nm_device));
  }

  swin = WID("network-scrolledwindow");
  label = WID("no-network-label");
  spinner = WID("no-network-spinner");

  if (state == NM_DEVICE_STATE_UNMANAGED ||
      state == NM_DEVICE_STATE_UNAVAILABLE) {
    refresh_without_device (data);
    goto out;
  }
  else if (aps == NULL || aps->len == 0) {
    gtk_label_set_text (GTK_LABEL (label), _("Checking for available wireless networks"));
    gtk_widget_hide (swin);
    gtk_widget_show (spinner);
    gtk_widget_show (label);
    g_timeout_add_seconds (1, refresh_again, data);

    goto out;
  }
  else {
    gtk_widget_show (swin);
    gtk_widget_hide (spinner);
    gtk_widget_hide (label);
  }

  unique_aps = get_strongest_unique_aps (aps);
  for (i = 0; i < unique_aps->len; i++) {
    ap = NM_ACCESS_POINT (g_ptr_array_index (unique_aps, i));
    add_access_point (data, ap, active_ap);
  }
  g_ptr_array_unref (unique_aps);
  add_access_point_other (data);

 out:
  gtk_tree_view_set_model (OBJ(GtkTreeView*, "network-list"), (GtkTreeModel*)data->ap_list);

  if (active_ap)
    select_and_scroll_to_ap (data, active_ap);

  data->refreshing = FALSE;
}

static void
device_state_changed (NMDevice *device, GParamSpec *pspec, NetworkData *data)
{
  refresh_wireless_list (data);
}

static void
connection_activate_cb (NMClient *client,
                        NMActiveConnection *connection,
                        GError *error,
                        gpointer user_data)
{
  NetworkData *data = user_data;

  if (connection == NULL) {
    /* failed to activate */
    refresh_wireless_list (data);
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
connect_to_hidden_network_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  NetworkData *data = user_data;
  GError *error = NULL;
  GVariant *result = NULL;

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (result == NULL) {
    g_warning ("failed to connect to hidden network: %s",
               error->message);
    g_error_free (error);
  }

  refresh_wireless_list (data);
}

static void
connect_to_hidden_network (NetworkData *data)
{
  GDBusProxy *proxy;
  GVariant *res = NULL;
  GError *error = NULL;

  /* connect to NM applet */
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                         G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                         NULL,
                                         "org.gnome.network_manager_applet",
                                         "/org/gnome/network_manager_applet",
                                         "org.gnome.network_manager_applet",
                                         NULL,
                                         &error);
  if (proxy == NULL) {
    g_warning ("failed to connect to NM applet: %s",
               error->message);
    g_error_free (error);
    goto out;
  }

  /* try to show the hidden network UI */
  g_dbus_proxy_call (proxy,
                     "ConnectToHiddenNetwork",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     5000, /* don't wait forever */
                     NULL,
                     connect_to_hidden_network_cb,
                     data);

 out:
  if (proxy != NULL)
    g_object_unref (proxy);
  if (res != NULL)
    g_variant_unref (res);
}

static void
wireless_selection_changed (GtkTreeSelection *selection, NetworkData *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *object_path;
  gchar *ssid_target;
  GSList *list, *filtered, *l;
  NMConnection *connection;
  NMConnection *connection_to_activate;
  NMSettingWireless *setting;
  const GByteArray *ssid;
  const gchar *ssid_tmp;

  if (data->refreshing)
    return;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      PANEL_WIRELESS_COLUMN_ID, &object_path,
                      PANEL_WIRELESS_COLUMN_TITLE, &ssid_target,
                      -1);

  gtk_list_store_set (data->ap_list, &iter,
                      PANEL_WIRELESS_COLUMN_ACTIVATING, TRUE,
                      -1);

  if (g_strcmp0 (object_path, "ap-other...") == 0) {
    connect_to_hidden_network (data);
    goto out;
  }

  list = nm_remote_settings_list_connections (data->nm_settings);
  filtered = nm_device_filter_connections (data->nm_device, list);

  connection_to_activate = NULL;

  for (l = filtered; l; l = l->next) {
    connection = NM_CONNECTION (l->data);
    setting = nm_connection_get_setting_wireless (connection);
    if (!NM_IS_SETTING_WIRELESS (setting))
      continue;

    ssid = nm_setting_wireless_get_ssid (setting);
    if (ssid == NULL)
      continue;
    ssid_tmp = nm_utils_escape_ssid (ssid->data, ssid->len);
    if (g_strcmp0 (ssid_target, ssid_tmp) == 0) {
      connection_to_activate = connection;
      break;
    }
  }
  g_slist_free (list);
  g_slist_free (filtered);

  if (connection_to_activate != NULL) {
    nm_client_activate_connection (data->nm_client,
                                   connection_to_activate,
                                   data->nm_device, NULL,
                                   connection_activate_cb, data);
    goto out;
  }

  nm_client_add_and_activate_connection (data->nm_client,
                                         NULL,
                                         data->nm_device, object_path,
                                         connection_add_activate_cb, data);

 out:
  g_free (object_path);
  g_free (ssid_target);

  refresh_wireless_list (data);
}

static void
connection_state_changed (NMActiveConnection *c, GParamSpec *pspec, NetworkData *data)
{
  refresh_wireless_list (data);
}

static void
active_connections_changed (NMClient *client, GParamSpec *pspec, NetworkData *data)
{
  const GPtrArray *connections;
  guint i;

  connections = nm_client_get_active_connections (client);
  for (i = 0; connections && (i < connections->len); i++) {
    NMActiveConnection *connection;

    connection = g_ptr_array_index (connections, i);
    if (!g_object_get_data (G_OBJECT (connection), "has-state-changed-handler")) {
      g_signal_connect (connection, "notify::state",
                        G_CALLBACK (connection_state_changed), data);
      g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (1));
    }
  }

  refresh_wireless_list (data);
}

void
gis_prepare_network_page (SetupData *setup)
{
  GtkTreeViewColumn *col;
  GtkCellRenderer *cell;
  GtkTreeSortable *sortable;
  GtkTreeSelection *selection;
  const GPtrArray *devices;
  NMDevice *device;
  guint i;
  DBusGConnection *bus;
  GError *error;
  NetworkData *data = g_slice_new0 (NetworkData);
  GisAssistant *assistant = gis_get_assistant (setup);

  data->setup = setup;
  data->builder = gis_builder (PAGE_ID);

  col = OBJ(GtkTreeViewColumn*, "network-list-column");

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, FALSE);
  /* black small diamond */
  g_object_set (cell, "text", "\342\254\251", NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
                                  "visible", PANEL_WIRELESS_COLUMN_ACTIVE,
                                  NULL);
  cell = gtk_cell_renderer_spinner_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, FALSE);
  gtk_cell_area_cell_set (gtk_cell_layout_get_area (GTK_CELL_LAYOUT (col)), cell, "align", FALSE, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
                                  "active", PANEL_WIRELESS_COLUMN_ACTIVATING,
                                  "visible", PANEL_WIRELESS_COLUMN_ACTIVATING,
                                  "pulse", PANEL_WIRELESS_COLUMN_PULSE,
                                  NULL);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "width", 400, "width-chars", 45, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, TRUE);
  gtk_cell_area_cell_set (gtk_cell_layout_get_area (GTK_CELL_LAYOUT (col)), cell, "align", TRUE, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
                                  "text", PANEL_WIRELESS_COLUMN_TITLE,
                                  NULL);

  cell = panel_cell_renderer_mode_new ();
  gtk_cell_renderer_set_padding (cell, 4, 0);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
                                  "mode", PANEL_WIRELESS_COLUMN_MODE,
                                  NULL);

  cell = panel_cell_renderer_signal_new ();
  gtk_cell_renderer_set_padding (cell, 4, 0);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
                                  "signal", PANEL_WIRELESS_COLUMN_STRENGTH,
                                  NULL);

  cell = panel_cell_renderer_security_new ();
  gtk_cell_renderer_set_padding (cell, 4, 0);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
                                  "security", PANEL_WIRELESS_COLUMN_SECURITY,
                                  NULL);

  data->ap_list = g_object_ref (OBJ(GtkListStore *, "liststore-wireless"));
  sortable = GTK_TREE_SORTABLE (data->ap_list);
  gtk_tree_sortable_set_sort_column_id (sortable,
                                        PANEL_WIRELESS_COLUMN_STRENGTH,
                                        GTK_SORT_DESCENDING);

  data->nm_client = nm_client_new ();

  g_signal_connect (data->nm_client, "notify::active-connections",
                    G_CALLBACK (active_connections_changed), data);

  devices = nm_client_get_devices (data->nm_client);
  if (devices) {
    for (i = 0; i < devices->len; i++) {
      device = g_ptr_array_index (devices, i);

      if (!nm_device_get_managed (device))
        continue;

      if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI) {
        /* FIXME deal with multiple, dynamic devices */
        data->nm_device = device;
        g_signal_connect (G_OBJECT (device), "notify::state",
                          G_CALLBACK (device_state_changed), data);
        break;
      }
    }
  }

  if (data->nm_device == NULL) {
    refresh_without_device (data);
    goto out;
  }

  error = NULL;
  bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
  if (!bus) {
    g_warning ("Error connecting to system D-Bus: %s",
               error->message);
    g_error_free (error);
  }
  data->nm_settings = nm_remote_settings_new (bus);

  selection = OBJ(GtkTreeSelection*, "network-list-selection");

  g_signal_connect (selection, "changed",
                    G_CALLBACK (wireless_selection_changed), data);

  refresh_wireless_list (data);

  gis_assistant_add_page (assistant, WID ("network-page"));
  gis_assistant_set_page_title (assistant, WID ("network-page"), _("Network"));
  gis_assistant_set_page_complete (assistant, WID ("network-page"), TRUE);

 out: ;
}
