/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GIS_WELCOME_PAGE_H__
#define __GIS_WELCOME_PAGE_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

typedef struct _WelcomeData WelcomeData;

struct _WelcomeData {
  SetupData *setup;
};

void gis_prepare_welcome_page (WelcomeData *data);

G_END_DECLS

#endif /* __GIS_WELCOME_PAGE_H__ */
