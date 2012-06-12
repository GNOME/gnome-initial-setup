/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GIS_GOA_PAGE_H__
#define __GIS_GOA_PAGE_H__

#include "gnome-initial-setup.h"

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

G_BEGIN_DECLS

void gis_prepare_online_page (SetupData *setup);

G_END_DECLS

#endif /* __GIS_GOA_PAGE_H__ */
