/* gweather-location-entry.h - Location-selecting text entry
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-location-entry.h must not be included individually, include gweather.h instead"
#endif

#include <gtk/gtk.h>
#include <libgweather/gweather-location.h>

G_BEGIN_DECLS

typedef struct _GWeatherLocationEntry GWeatherLocationEntry;
typedef struct _GWeatherLocationEntryClass GWeatherLocationEntryClass;
typedef struct _GWeatherLocationEntryPrivate GWeatherLocationEntryPrivate;

#define GWEATHER_TYPE_LOCATION_ENTRY            (gweather_location_entry_get_type ())
#define GWEATHER_LOCATION_ENTRY(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GWEATHER_TYPE_LOCATION_ENTRY, GWeatherLocationEntry))
#define GWEATHER_LOCATION_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GWEATHER_TYPE_LOCATION_ENTRY, GWeatherLocationEntryClass))
#define GWEATHER_IS_LOCATION_ENTRY(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GWEATHER_TYPE_LOCATION_ENTRY))
#define GWEATHER_IS_LOCATION_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GWEATHER_TYPE_LOCATION_ENTRY))
#define GWEATHER_LOCATION_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GWEATHER_TYPE_LOCATION_ENTRY, GWeatherLocationEntryClass))

struct _GWeatherLocationEntry {
    GtkSearchEntry parent;

    /*< private >*/
    GWeatherLocationEntryPrivate *priv;
};

struct _GWeatherLocationEntryClass {
    GtkSearchEntryClass parent_class;
};

GWEATHER_AVAILABLE_IN_ALL
GType                   gweather_location_entry_get_type        (void);

GWEATHER_AVAILABLE_IN_ALL
GtkWidget *             gweather_location_entry_new             (GWeatherLocation *top);
GWEATHER_AVAILABLE_IN_ALL
void                    gweather_location_entry_set_location    (GWeatherLocationEntry *entry,
                                                                 GWeatherLocation *loc);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_entry_get_location    (GWeatherLocationEntry *entry);
GWEATHER_AVAILABLE_IN_ALL
gboolean                gweather_location_entry_has_custom_text (GWeatherLocationEntry *entry);
GWEATHER_AVAILABLE_IN_ALL
gboolean                gweather_location_entry_set_city        (GWeatherLocationEntry *entry,
                                                                 const char *city_name,
                                                                 const char *code);

G_END_DECLS
