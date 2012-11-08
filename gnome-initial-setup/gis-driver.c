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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "gnome-initial-setup.h"

#include <stdlib.h>

#include "gis-assistant-gtk.h"

#ifdef HAVE_CLUTTER
#include <clutter-gtk/clutter-gtk.h>
#include "gis-assistant-clutter.h"
#endif

#ifdef HAVE_CLUTTER
#include "gis-assistant-clutter.h"
#endif

G_DEFINE_TYPE(GisDriver, gis_driver, GTK_TYPE_APPLICATION)

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_DRIVER, GisDriverPrivate))

enum {
  REBUILD_PAGES,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

struct _GisDriverPrivate {
  GtkWindow *main_window;
  GisAssistant *assistant;

  ActUser *user_account;
  const gchar *user_password;
};

static void
title_changed_cb (GisAssistant *assistant,
                  GParamSpec   *gparam,
                  GisDriver    *driver)
{
  GisDriverPrivate *priv = driver->priv;
  gtk_window_set_title (priv->main_window, gis_assistant_get_title (assistant));
}

static void
recenter_window (GdkScreen *screen, GisDriver *driver)
{
  GisDriverPrivate *priv = driver->priv;
  gtk_window_set_position (priv->main_window, GTK_WIN_POS_CENTER_ALWAYS);
}

static void
prepare_main_window (GisDriver *driver)
{
  GisDriverPrivate *priv = driver->priv;

  g_signal_connect (gtk_widget_get_screen (GTK_WIDGET (priv->main_window)),
                    "monitors-changed", G_CALLBACK (recenter_window), driver);

  g_signal_connect (priv->assistant, "notify::title",
                    G_CALLBACK (title_changed_cb), driver);
}

static gboolean
rebuild_pages (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[REBUILD_PAGES], 0);
  return FALSE;
}

GisAssistant *
gis_driver_get_assistant (GisDriver *driver)
{
  GisDriverPrivate *priv = driver->priv;
  return priv->assistant;
}

void
gis_driver_set_user_permissions (GisDriver   *driver,
                                 ActUser     *user,
                                 const gchar *password)
{
  GisDriverPrivate *priv = driver->priv;
  priv->user_account = user;
  priv->user_password = password;
}

void
gis_driver_get_user_permissions (GisDriver    *driver,
                                 ActUser     **user,
                                 const gchar **password)
{
  GisDriverPrivate *priv = driver->priv;
  *user = priv->user_account;
  *password = priv->user_password;
}

void
gis_driver_locale_changed (GisDriver *driver)
{
  g_idle_add ((GSourceFunc) rebuild_pages, driver);
}

static GType
get_assistant_type (void)
{
#ifdef HAVE_CLUTTER
  gboolean enable_animations;
  g_object_get (gtk_settings_get_default (),
                "gtk-enable-animations", &enable_animations,
                NULL);

  if (enable_animations && g_getenv ("GIS_DISABLE_CLUTTER") == NULL)
    return GIS_TYPE_ASSISTANT_CLUTTER;
#endif /* HAVE_CLUTTER */

  return GIS_TYPE_ASSISTANT_GTK;
}

static void
gis_driver_activate (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);
  GisDriverPrivate *priv = driver->priv;

  G_APPLICATION_CLASS (gis_driver_parent_class)->activate (app);

  gtk_window_present (GTK_WINDOW (priv->main_window));
}

static void
gis_driver_startup (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);
  GisDriverPrivate *priv = driver->priv;

  G_APPLICATION_CLASS (gis_driver_parent_class)->startup (app);

  priv->main_window = g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                                    "application", app,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "border-width", 12,
                                    "icon-name", "preferences-system",
                                    "deletable", FALSE,
                                    "resizable", FALSE,
                                    "window-position", GTK_WIN_POS_CENTER_ALWAYS,
                                    NULL);

  priv->assistant = g_object_new (get_assistant_type (), NULL);
  gtk_container_add (GTK_CONTAINER (priv->main_window), GTK_WIDGET (priv->assistant));

  gtk_widget_show (GTK_WIDGET (priv->assistant));

  prepare_main_window (driver);
  rebuild_pages (driver);
}

static void
gis_driver_real_rebuild_pages (GisDriver *driver)
{
  GisDriverPrivate *priv = driver->priv;
  gis_assistant_destroy_all_pages (priv->assistant);
}

static void
gis_driver_init (GisDriver *driver)
{
  driver->priv = GET_PRIVATE (driver);
}

static void
gis_driver_class_init (GisDriverClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GisDriverPrivate));

  application_class->startup = gis_driver_startup;
  application_class->activate = gis_driver_activate;

  klass->rebuild_pages = gis_driver_real_rebuild_pages;
  signals[REBUILD_PAGES] =
    g_signal_new ("rebuild-pages",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GisDriverClass, rebuild_pages),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

GisDriver *
gis_driver_new (void)
{
  return g_object_new (GIS_TYPE_DRIVER,
                       "application-id", "org.gnome.InitialSetup");
}
