/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GIS_EULA_PAGES_H__
#define __GIS_EULA_PAGES_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

typedef struct _EulasData EulasData;

struct _EulasData {
  SetupData *setup;
};

void gis_prepare_eula_pages (EulasData *data);

G_END_DECLS

#endif /* __GIS_EULA_PAGES_H__ */
