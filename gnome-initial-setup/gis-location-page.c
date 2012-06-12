
/* Location page {{{1 */

static void
set_timezone_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
        LocationData *data = user_data;
        GError *error;

        error = NULL;
        if (!timedate1_call_set_timezone_finish (data->dtm,
                                                 res,
                                                 &error)) {
                /* TODO: display any error in a user friendly way */
                g_warning ("Could not set system timezone: %s", error->message);
                g_error_free (error);
        }
}


static void
queue_set_timezone (LocationData *data)
{
        /* for now just do it */
        if (data->current_location) {
                timedate1_call_set_timezone (data->dtm,
                                             data->current_location->zone,
                                             TRUE,
                                             NULL,
                                             set_timezone_cb,
                                             data);
        }
}

static void
update_timezone (LocationData *data)
{
        SetupData *setup = data->setup;
        GString *str;
        gchar *location;
        gchar *timezone;
        gchar *c;

        str = g_string_new ("");
        for (c = data->current_location->zone; *c; c++) {
                switch (*c) {
                case '_':
                        g_string_append_c (str, ' ');
                        break;
                case '/':
                        g_string_append (str, " / ");
                        break;
                default:
                        g_string_append_c (str, *c);
                }
        }

        c = strstr (str->str, " / ");
        location = g_strdup (c + 3);
        timezone = g_strdup (str->str);

        gtk_label_set_label (OBJ(GtkLabel*,"current-location-label"), location);
        gtk_label_set_label (OBJ(GtkLabel*,"current-timezone-label"), timezone);

        g_free (location);
        g_free (timezone);

        g_string_free (str, TRUE);
}

static void
location_changed_cb (CcTimezoneMap *map,
                     TzLocation    *location,
                     LocationData  *data)
{
  g_debug ("location changed to %s/%s", location->country, location->zone);

  data->current_location = location;

  update_timezone (data);

  queue_set_timezone (data);
}

static void
set_location_from_gweather_location (LocationData     *data,
                                     GWeatherLocation *gloc)
{
        SetupData *setup = data->setup;
        GWeatherTimezone *zone = gweather_location_get_timezone (gloc);
        gchar *city = gweather_location_get_city_name (gloc);

        if (zone != NULL) {
                const gchar *name;
                const gchar *id;
                GtkLabel *label;

                label = OBJ(GtkLabel*, "current-timezone-label");

                name = gweather_timezone_get_name (zone);
                id = gweather_timezone_get_tzid (zone);
                if (name == NULL) {
                        /* Why does this happen ? */
                        name = id;
                }
                gtk_label_set_label (label, name);
                cc_timezone_map_set_timezone (data->map, id);
        }

        if (city != NULL) {
                GtkLabel *label;

                label = OBJ(GtkLabel*, "current-location-label");
                gtk_label_set_label (label, city);
        }

        g_free (city);
}

static void
location_changed (GObject *object, GParamSpec *param, LocationData *data)
{
        GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);
        GWeatherLocation *gloc;

        gloc = gweather_location_entry_get_location (entry);
        if (gloc == NULL)
                return;

        set_location_from_gweather_location (data, gloc);

        gweather_location_unref (gloc);
}

#define WANT_GEOCLUE 0

#if WANT_GEOCLUE
static void
position_callback (GeocluePosition      *pos,
		   GeocluePositionFields fields,
		   int                   timestamp,
		   double                latitude,
		   double                longitude,
		   double                altitude,
		   GeoclueAccuracy      *accuracy,
		   GError               *error,
		   LocationData         *data)
{
	if (error) {
		g_printerr ("Error getting position: %s\n", error->message);
		g_error_free (error);
	} else {
		if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE &&
		    fields & GEOCLUE_POSITION_FIELDS_LONGITUDE) {
                        GWeatherLocation *city = gweather_location_find_nearest_city (latitude, longitude);
                        set_location_from_gweather_location (data, city);
		} else {
			g_print ("Position not available.\n");
		}
	}
}

static void
determine_location (GtkWidget    *widget,
                    LocationData *data)
{
	GeoclueMaster *master;
	GeoclueMasterClient *client;
	GeocluePosition *position = NULL;
        GError *error = NULL;

	master = geoclue_master_get_default ();
	client = geoclue_master_create_client (master, NULL, NULL);
	g_object_unref (master);

	if (!geoclue_master_client_set_requirements (client, 
	                                             GEOCLUE_ACCURACY_LEVEL_LOCALITY,
	                                             0, TRUE,
	                                             GEOCLUE_RESOURCE_ALL,
	                                             NULL)){
		g_printerr ("Setting requirements failed");
                goto out;
	}

	position = geoclue_master_client_create_position (client, &error);
	if (position == NULL) {
		g_warning ("Creating GeocluePosition failed: %s", error->message);
                goto out;
	}

	geoclue_position_get_position_async (position,
	                                     (GeocluePositionCallback) position_callback,
	                                     data);

 out:
        g_clear_error (&error);
        g_object_unref (client);
        g_object_unref (position);
}
#endif

static void
prepare_location_page (SetupData *setup)
{
        GtkWidget *frame, *map, *entry;
        GWeatherLocation *world;
        GError *error;
        const gchar *timezone;
        LocationData *data = &(setup->location_data);

        data->setup = setup;

        frame = WID("location-map-frame");

        error = NULL;
        data->dtm = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                      "org.freedesktop.timedate1",
                                                      "/org/freedesktop/timedate1",
                                                      NULL,
                                                      &error);
        if (data->dtm == NULL) {
                g_error ("Failed to create proxy for timedated: %s", error->message);
                exit (1);
        }

        data->map = cc_timezone_map_new ();
        map = (GtkWidget *)data->map;
        gtk_widget_set_hexpand (map, TRUE);
        gtk_widget_set_vexpand (map, TRUE);
        gtk_widget_set_halign (map, GTK_ALIGN_FILL);
        gtk_widget_set_valign (map, GTK_ALIGN_FILL);
        gtk_widget_show (map);

        gtk_container_add (GTK_CONTAINER (frame), map);

        world = gweather_location_new_world (FALSE);
        entry = gweather_location_entry_new (world);
        gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Search for a location"));
        gtk_widget_set_halign (entry, GTK_ALIGN_END);
        gtk_widget_show (entry);

        frame = WID("location-page");
        gtk_grid_attach (GTK_GRID (frame), entry, 1, 1, 1, 1);

        timezone = timedate1_get_timezone (data->dtm);

        if (!cc_timezone_map_set_timezone (data->map, timezone)) {
                g_warning ("Timezone '%s' is unhandled, setting %s as default", timezone, DEFAULT_TZ);
                cc_timezone_map_set_timezone (data->map, DEFAULT_TZ);
        }
        else {
                g_debug ("System timezone is '%s'", timezone);
        }

        data->current_location = cc_timezone_map_get_location (data->map);
        update_timezone (data);

        g_signal_connect (G_OBJECT (entry), "notify::location",
                          G_CALLBACK (location_changed), data);

        g_signal_connect (map, "location-changed",
                          G_CALLBACK (location_changed_cb), data);

#if WANT_GEOCLUE
        g_signal_connect (WID ("location-auto-button"), "clicked",
                          G_CALLBACK (determine_location), data);
#else
        gtk_widget_hide (WID ("location-auto-button"));
#endif
}
