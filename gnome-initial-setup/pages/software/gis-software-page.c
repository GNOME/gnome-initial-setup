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

/* SOFTWARE pages {{{1 */

#define PAGE_ID "software"

#include "config.h"
#include "software-resources.h"
#include "gis-software-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct _GisSoftwarePagePrivate
{
  GtkWidget *more_popover;
  GtkWidget *proprietary_switch;
  GtkWidget *text_label;
};

typedef struct _GisSoftwarePagePrivate GisSoftwarePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSoftwarePage, gis_software_page, GIS_TYPE_PAGE);

static char *
get_item (const char *buffer, const char *name)
{
  char *label, *start, *end, *result;
  char end_char;

  result = NULL;
  start = NULL;
  end = NULL;
  label = g_strconcat (name, "=", NULL);
  if ((start = strstr (buffer, label)) != NULL)
    {
      start += strlen (label);
      end_char = '\n';
      if (*start == '"')
        {
          start++;
          end_char = '"';
        }

      end = strchr (start, end_char);
    }

    if (start != NULL && end != NULL)
      {
        result = g_strndup (start, end - start);
      }

  g_free (label);

  return result;
}

static void
update_distro_name (GisSoftwarePage *page)
{
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);
  char *buffer;
  char *name;
  char *text;

  name = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
      name = get_item (buffer, "NAME");
      g_free (buffer);
    }

  if (!name)
    name = g_strdup ("GNOME");

  /* Translators: the parameter here is the name of a distribution,
   * like "Fedora" or "Ubuntu". It falls back to "GNOME" if we can't
   * detect any distribution.
   */
  text = g_strdup_printf (_("Proprietary software sources provide access to additional software, including web browsers and games. This software typically has restrictions on use and access to source code, and is not provided by %s."), name);
  gtk_label_set_label (GTK_LABEL (priv->text_label), text);
  g_free (text);

  g_free (name);
}

static void
gis_software_page_constructed (GObject *object)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (object);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_software_page_parent_class)->constructed (object);

  update_distro_name (page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_software_page_dispose (GObject *object)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (object);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_software_page_parent_class)->dispose (object);
}

static void
gis_software_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Software Sources"));
  update_distro_name (GIS_SOFTWARE_PAGE (page));
}

static gboolean
activate_link (const char *label,
               const char *uri,
               gpointer    data)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (data);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);

  gtk_widget_show (priv->more_popover);
}

static gboolean
state_set (GtkSwitch *sw,
           gboolean   state,
           gpointer   data)
{
  g_print ("%s proprietary software sources\n", state ? "Enable" : "Disable");

  gtk_switch_set_state (sw, state);

  return TRUE;
}

static void
gis_software_page_class_init (GisSoftwarePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-software-page.ui");
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, more_popover);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, proprietary_switch);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, text_label);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), activate_link);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), state_set);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_software_page_locale_changed;
  object_class->constructed = gis_software_page_constructed;
  object_class->dispose = gis_software_page_dispose;
}

static void
gis_software_page_init (GisSoftwarePage *page)
{
  g_resources_register (software_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_prepare_software_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_SOFTWARE_PAGE,
                                     "driver", driver,
                                     NULL));
}
