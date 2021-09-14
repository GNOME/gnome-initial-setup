/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2016, 2021 Red Hat
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
 *     Kalev Lember <klember@redhat.com>
 *     Michael Catanzaro <mcatanzaro@redhat.com>
 */

/* SOFTWARE pages {{{1 */

#define PAGE_ID "software"

#include "config.h"
#include "software-resources.h"
#include "gis-software-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "gis-page-header.h"

struct _GisSoftwarePagePrivate
{
  GtkWidget *proprietary_switch;
  GtkWidget *header;
};

typedef struct _GisSoftwarePagePrivate GisSoftwarePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSoftwarePage, gis_software_page, GIS_TYPE_PAGE);

static void
gis_software_page_constructed (GObject *object)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (object);

  G_OBJECT_CLASS (gis_software_page_parent_class)->constructed (object);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

/* Distro-specific stuff is isolated here so that the rest of this page can be
 * used by other distros. Feel free to add your distro here.
 */
static char *
find_fedora_third_party (void)
{
  return g_find_program_in_path ("fedora-third-party");
}

static gboolean
should_show_software_page (void)
{
  g_autofree char *has_fedora_third_party = find_fedora_third_party ();
  return has_fedora_third_party != NULL;
}

static gboolean
gis_software_page_apply (GisPage      *gis_page,
                         GCancellable *cancellable)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (gis_page);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);
  g_autofree char *program = NULL;
  g_autoptr (GSubprocessLauncher) launcher = NULL;
  g_autoptr (GSubprocess) subprocess = NULL;
  g_autoptr (GError) error = NULL;

  program = find_fedora_third_party ();

  if (program)
    {
      const char *arg1;

      if (gtk_switch_get_state (GTK_SWITCH (priv->proprietary_switch)))
        arg1 = "enable";
      else
        arg1 = "disable";

      gis_pkexec (program, arg1, "root", &error);
      if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s failed: %s", program, error->message);
    }

  return FALSE;
}

static void
gis_software_page_locale_changed (GisPage *gis_page)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (gis_page);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);

  gis_page_set_title (GIS_PAGE (page), _("Third-Party Repositories"));
  g_object_set (priv->header, "subtitle", _("Third-party repositories provide access to additional software from selected external sources. They include popular apps, as well as firmware that is important for some devices. Some proprietary software is included."), NULL);
}

static gboolean
state_set (GtkSwitch *sw,
           gboolean   state,
           gpointer   data)
{
  gtk_switch_set_state (sw, state);

  return TRUE;
}

static void
gis_software_page_class_init (GisSoftwarePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-software-page.ui");
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, proprietary_switch);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, header);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), state_set);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_software_page_locale_changed;
  page_class->apply = gis_software_page_apply;
  object_class->constructed = gis_software_page_constructed;
}

static void
gis_software_page_init (GisSoftwarePage *page)
{
  g_resources_register (software_get_resource ());
  g_type_ensure (GIS_TYPE_PAGE_HEADER);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_software_page (GisDriver *driver)
{
  GisPage *page = NULL;
  if (should_show_software_page ())
    {
      page = g_object_new (GIS_TYPE_SOFTWARE_PAGE,
                           "driver", driver,
                           NULL);
    }

  return page;
}
