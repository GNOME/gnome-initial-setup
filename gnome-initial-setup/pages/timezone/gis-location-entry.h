/* gweather-location-entry.h - Location-selecting text entry
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>
#include <libgweather/gweather.h>

G_BEGIN_DECLS

typedef struct _GisLocationEntry GisLocationEntry;
typedef struct _GisLocationEntryClass GisLocationEntryClass;
typedef struct _GisLocationEntryPrivate GisLocationEntryPrivate;

#define GIS_TYPE_LOCATION_ENTRY (gis_location_entry_get_type ())
#define GIS_LOCATION_ENTRY(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GIS_TYPE_LOCATION_ENTRY, GisLocationEntry))
#define GIS_LOCATION_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIS_TYPE_LOCATION_ENTRY, GisLocationEntryClass))
#define GIS_IS_LOCATION_ENTRY(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GIS_TYPE_LOCATION_ENTRY))
#define GIS_IS_LOCATION_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIS_TYPE_LOCATION_ENTRY))
#define GIS_LOCATION_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIS_TYPE_LOCATION_ENTRY, GisLocationEntryClass))

struct _GisLocationEntry {
    AdwBin parent;

    /*< private >*/
    GisLocationEntryPrivate *priv;
};

struct _GisLocationEntryClass {
    AdwBinClass parent_class;
};

GType                   gis_location_entry_get_type        (void);

GtkWidget *             gis_location_entry_new             (GWeatherLocation *top);
void                    gis_location_entry_set_location    (GisLocationEntry *entry,
                                                            GWeatherLocation *loc);
GWeatherLocation *      gis_location_entry_get_location    (GisLocationEntry *entry);
gboolean                gis_location_entry_set_city        (GisLocationEntry *entry,
                                                            const char       *city_name,
                                                            const char       *code);

G_END_DECLS
