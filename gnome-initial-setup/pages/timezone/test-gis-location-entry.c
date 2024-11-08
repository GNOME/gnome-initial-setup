#include "config.h"

#include "gis-location-entry.h"

#include <adwaita.h>

static void
entry_location_changed (GObject    *object,
                        GParamSpec *param,
                        gpointer    data)
{
  GtkLabel *label = GTK_LABEL (data);
  GisLocationEntry *entry = GIS_LOCATION_ENTRY (object);
  g_autoptr(GWeatherLocation) location = NULL;

  location = gis_location_entry_get_location (entry);
  if (location != NULL)
    {
      const gchar *name = gweather_location_get_name (location);
      GTimeZone *zone = gweather_location_get_timezone (location);
      const char *tzid = zone ? g_time_zone_get_identifier (zone) : "no timezone";
      g_autofree gchar *message = g_strdup_printf ("%s (%s)", name, tzid);

      gtk_label_set_text (label, message);
    }
  else
    {
      gtk_label_set_text (label, "No location selected");
    }
}

static void
activate_cb (GtkApplication *app)
{
  GtkWidget *window = gtk_application_window_new (app);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *label = gtk_label_new ("Pick a location…");
  GtkWidget *entry = gis_location_entry_new (NULL);

  gtk_box_append (GTK_BOX (box), label);
  gtk_box_append (GTK_BOX (box), entry);

  g_signal_connect (entry, "notify::location",
                    G_CALLBACK (entry_location_changed), label);

  gtk_window_set_title (GTK_WINDOW (window), "Hello");
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 768);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr (AdwApplication) app = NULL;

  app = adw_application_new ("org.gnome.InitialSetup.TestLocationEntry", G_APPLICATION_FLAGS_NONE);

  g_signal_connect (app, "activate", G_CALLBACK (activate_cb), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
