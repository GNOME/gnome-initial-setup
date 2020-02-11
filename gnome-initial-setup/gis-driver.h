/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __GIS_DRIVER_H__
#define __GIS_DRIVER_H__

#include "gis-assistant.h"
#include "gis-page.h"
#include <act/act-user-manager.h>
#if HAVE_GDM
# include <gdm/gdm-client.h>
#else
typedef GObject GdmClient;
typedef GObject GdmGreeter;
typedef GObject GdmUserVerifier;
#endif

G_BEGIN_DECLS

#define GIS_TYPE_DRIVER               (gis_driver_get_type ())
#define GIS_DRIVER(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_DRIVER, GisDriver))
#define GIS_DRIVER_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_DRIVER, GisDriverClass))
#define GIS_IS_DRIVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_DRIVER))
#define GIS_IS_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_DRIVER))
#define GIS_DRIVER_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_DRIVER, GisDriverClass))

typedef struct _GisDriver        GisDriver;
typedef struct _GisDriverClass   GisDriverClass;

typedef enum {
  UM_LOCAL,
  UM_ENTERPRISE,
  NUM_MODES,
} UmAccountMode;

struct _GisDriver
{
  GtkApplication parent;
};

struct _GisDriverClass
{
  GtkApplicationClass parent_class;

  void (* rebuild_pages) (GisDriver *driver);
  void (* locale_changed) (GisDriver *driver);
};

typedef enum {
  GIS_DRIVER_MODE_NEW_USER,
  GIS_DRIVER_MODE_EXISTING_USER,
} GisDriverMode;

GType gis_driver_get_type (void);

GisAssistant *gis_driver_get_assistant (GisDriver *driver);

void gis_driver_set_user_permissions (GisDriver   *driver,
                                      ActUser     *user,
                                      const gchar *password);

void gis_driver_get_user_permissions (GisDriver    *driver,
                                      ActUser     **user,
                                      const gchar **password);

void gis_driver_set_account_mode (GisDriver     *driver,
                                  UmAccountMode  mode);

UmAccountMode gis_driver_get_account_mode (GisDriver *driver);

void gis_driver_set_user_language (GisDriver   *driver,
                                   const gchar *lang_id,
                                   gboolean     update_locale);

const gchar *gis_driver_get_user_language (GisDriver *driver);

void gis_driver_set_username (GisDriver   *driver,
                              const gchar *username);
const gchar *gis_driver_get_username (GisDriver *driver);

gboolean gis_driver_get_gdm_objects (GisDriver        *driver,
                                     GdmGreeter      **greeter,
                                     GdmUserVerifier **user_verifier);

GisDriverMode gis_driver_get_mode (GisDriver *driver);

gboolean gis_driver_is_small_screen (GisDriver *driver);

GKeyFile *gis_driver_get_vendor_conf_file (GisDriver *driver);

void gis_driver_add_page (GisDriver *driver,
                          GisPage   *page);

void gis_driver_hide_window (GisDriver *driver);

void gis_driver_save_data (GisDriver *driver);

gboolean gis_driver_conf_get_boolean (GisDriver *driver,
                                      const gchar *group,
                                      const gchar *key,
                                      gboolean default_value);

GStrv gis_driver_conf_get_string_list (GisDriver *driver,
                                       const gchar *group,
                                       const gchar *key,
                                       gsize *out_length);

gchar *gis_driver_conf_get_string (GisDriver *driver,
                                   const gchar *group,
                                   const gchar *key);

GisDriver *gis_driver_new (GisDriverMode mode);

G_END_DECLS

#endif /* __GIS_DRIVER_H__ */
