/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2020 Endless Mobile, Inc.
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
 *     Philip Withnall <withnall@endlessm.com>
 */

/* Parental controls page {{{1 */

#define PAGE_ID "parental-controls"

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libmalcontent-ui/malcontent-ui.h>

#include "parental-controls-resources.h"
#include "gis-page-header.h"
#include "gis-parental-controls-page.h"

struct _GisParentalControlsPage
{
  GisPage parent_instance;

  GtkWidget *user_controls;
};

G_DEFINE_TYPE (GisParentalControlsPage, gis_parental_controls_page, GIS_TYPE_PAGE)

static gboolean
page_validate (GisParentalControlsPage *page)
{
  /* TODO */
}

static void
update_page_validation (GisParentalControlsPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static gboolean
gis_parental_controls_page_apply (GisPage      *gis_page,
                                  GCancellable *cancellable)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);

  /* TODO */
}

static void
gis_parental_controls_page_save_data (GisPage *gis_page)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);

  /* TODO: create the user and set the parental controls */
}

static void
gis_parental_controls_page_shown (GisPage *gis_page)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);

  /* TODO */
}

/* TODO: I don’t think this does what I think it does */
static gboolean
gis_parental_controls_page_skip (GisPage *gis_page)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (gis_page);

  /* Skip showing the parental controls if they’re not enabled. */
  if (!gis_driver_get_parental_controls_enabled (GIS_PAGE (page)->driver))
    return TRUE;

  return FALSE;
}

static void
gis_parental_controls_page_constructed (GObject *object)
{
  GisParentalControlsPage *page = GIS_PARENTAL_CONTROLS_PAGE (object);

  G_OBJECT_CLASS (gis_parental_controls_page_parent_class)->constructed (object);

  /* TODO connect to signals from the user controls
  g_signal_connect (priv->page_local, "validation-changed",
                    G_CALLBACK (on_validation_changed), page); */

  update_page_validation (page);

  mct_user_controls_set_user (page->user_controls, selected_user);
  mct_user_controls_set_permission (self->user_controls, self->permission);

  /* TODO is this necessary? */
  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_parental_controls_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Parental Controls"));
}

static void
gis_parental_controls_page_class_init (GisParentalControlsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);

  object_class->constructed = gis_parental_controls_page_constructed;

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_parental_controls_page_locale_changed;
  page_class->apply = gis_parental_controls_page_apply;
  page_class->save_data = gis_parental_controls_page_save_data;
  page_class->shown = gis_parental_controls_page_shown;
  page_class->skip = gis_parental_controls_page_skip;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-parental-controls-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GisParentalControlsPage, user_controls);
}

static void
gis_parental_controls_page_init (GisParentalControlsPage *page)
{
  g_resources_register (parental_controls_get_resource ());

  /* Ensure types exist for widgets in the UI file. */
  g_type_ensure (GIS_TYPE_PAGE_HEADER);
  g_type_ensure (MCT_TYPE_USER_CONTROLS);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_parental_controls_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_PARENTAL_CONTROLS_PAGE,
                       "driver", driver,
                       NULL);
}
