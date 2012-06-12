/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GIS_NETWORK_PAGE_H__
#define __GIS_NETWORK_PAGE_H__

#include "gnome-initial-setup.h"

#include <nm-client.h>
#include <nm-device-wifi.h>
#include <nm-access-point.h>
#include <nm-utils.h>
#include <nm-remote-settings.h>

G_BEGIN_DECLS

void gis_prepare_network_page (SetupData *setup);

G_END_DECLS

#endif /* __GIS_NETWORK_PAGE_H__ */
