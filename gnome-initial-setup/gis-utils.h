/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __GIS_UTILS_H__
#define __GIS_UTILS_H__

#include "gnome-initial-setup.h"

#include <act/act-user-manager.h>

G_BEGIN_DECLS

void gis_copy_account_file (ActUser     *act_user,
                            const gchar *relative_path);

G_END_DECLS

#endif /* __GIS_UTILS_H__ */
