/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GIS_LOCATION_PAGE_H__
#define __GIS_LOCATION_PAGE_H__

#include "gnome-initial-setup.h"

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/location-entry.h>

#include "cc-timezone-map.h"
#include "timedated.h"

G_BEGIN_DECLS

typedef struct _LocationData LocationData;

struct _LocationData {
  SetupData *setup;

  /* location data */
  CcTimezoneMap *map;
  TzLocation *current_location;
  Timedate1 *dtm;
};

void gis_prepare_location_page (LocationData *data);

G_END_DECLS

#endif /* __GIS_LOCATION_PAGE_H__ */
