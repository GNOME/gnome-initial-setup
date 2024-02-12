/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2020 Red Hat
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
 *     Matthias Clasen <mclasen@redhat.com>
 */

/* Welcome page {{{1 */

#define PAGE_ID "welcome"

#include "config.h"
#include "welcome-resources.h"
#include "gis-welcome-page.h"
#include "gis-assistant.h"


struct _GisWelcomePage
{
  GisPage parent;
};

typedef struct
{
  AdwStatusPage *status_page;
} GisWelcomePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisWelcomePage, gis_welcome_page, GIS_TYPE_PAGE)

static void
update_welcome_title (GisWelcomePage *page)
{
  GisWelcomePagePrivate *priv = gis_welcome_page_get_instance_private (page);
  g_autofree char *name = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);
  g_autofree char *text = NULL;

  if (!name)
    name = g_strdup ("GNOME");

  /* Translators: This is meant to be a warm, engaging welcome message,
   * like greeting somebody at the door. If the exclamation mark is not
   * suitable for this in your language you may replace it. The space
   * before the exclamation mark in this string is a typographical thin
   * space (U200a) to improve the spacing in the title, which you can
   * keep or remove. The %s is getting replaced with the name and version
   * of the OS, e.g. "GNOME 3.38"
   */
  text = g_strdup_printf (_("Welcome to %s !"), name);

  adw_status_page_set_title (ADW_STATUS_PAGE (priv->status_page), text);
}

static void
gis_welcome_page_constructed (GObject *object)
{
  GisWelcomePage *page = GIS_WELCOME_PAGE (object);

  G_OBJECT_CLASS (gis_welcome_page_parent_class)->constructed (object);

  update_welcome_title (page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);
}

static void
start_setup (GtkButton *button, GisWelcomePage *page)
{
  GisAssistant *assistant;

  assistant = GIS_ASSISTANT (gtk_widget_get_ancestor (GTK_WIDGET (page), GIS_TYPE_ASSISTANT));

  gis_assistant_next_page (assistant);
}

static void
gis_welcome_page_class_init (GisWelcomePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-welcome-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisWelcomePage, status_page);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), start_setup);

  page_class->page_id = PAGE_ID;
  object_class->constructed = gis_welcome_page_constructed;
}

static void
gis_welcome_page_init (GisWelcomePage *page)
{
  gtk_widget_init_template (GTK_WIDGET (page));

  gis_add_style_from_resource ("/org/gnome/initial-setup/gis-welcome-page.css");
}

GisPage *
gis_prepare_welcome_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_WELCOME_PAGE,
                       "driver", driver,
                       NULL);
}
