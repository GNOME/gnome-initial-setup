/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2016 Red Hat
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
 */

/* SOFTWARE pages {{{1 */

#define PAGE_ID "software"

#include "config.h"
#include "software-resources.h"
#include "gis-software-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#ifdef ENABLE_SOFTWARE_SOURCES
#define I_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE
#include <packagekit-glib2/packagekit.h>
#endif

struct _GisSoftwarePagePrivate
{
  GtkWidget *more_popover;
  GtkWidget *proprietary_switch;
  GtkWidget *text_label;

  GSettings *software_settings;
  guint enable_count;
#ifdef ENABLE_SOFTWARE_SOURCES
  PkTask *task;
#endif
};

typedef struct _GisSoftwarePagePrivate GisSoftwarePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSoftwarePage, gis_software_page, GIS_TYPE_PAGE);

static void
gis_software_page_constructed (GObject *object)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (object);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_software_page_parent_class)->constructed (object);

  priv->software_settings = g_settings_new ("org.gnome.software");
#ifdef ENABLE_SOFTWARE_SOURCES
  priv->task = pk_task_new ();
#endif

  gtk_switch_set_active (GTK_SWITCH (priv->proprietary_switch),
                         g_settings_get_boolean (priv->software_settings, "show-nonfree-software"));

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_software_page_dispose (GObject *object)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (object);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);

  g_clear_object (&priv->software_settings);
#ifdef ENABLE_SOFTWARE_SOURCES
  g_clear_object (&priv->task);
#endif

  G_OBJECT_CLASS (gis_software_page_parent_class)->dispose (object);
}

static void
repo_enabled_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      data)
{
#ifdef ENABLE_SOFTWARE_SOURCES
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (data);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);
  g_autoptr(GError) error = NULL;
  g_autoptr(PkResults) results = NULL;

  results = pk_client_generic_finish (PK_CLIENT (source),
                                      res,
                                      &error);
  if (!results)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
#if PK_CHECK_VERSION(1,1,4)
      if (!g_error_matches (error, PK_CLIENT_ERROR, 0xff + PK_ERROR_ENUM_REPO_ALREADY_SET))
#endif
        g_critical ("Failed to enable repository: %s", error->message);
    }

  priv->enable_count--;
  if (priv->enable_count == 0)
    {
      /* all done */
      gis_page_apply_complete (GIS_PAGE (page), TRUE);
    }
#endif
}

gboolean
enable_repos (GisSoftwarePage *page,
              gchar **repo_ids,
              gboolean enable,
              GCancellable *cancellable)
{
#ifdef ENABLE_SOFTWARE_SOURCES
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);
  guint i;

  /* enable each repo */
  for (i = 0; repo_ids[i] != NULL; i++)
    {
      g_debug ("%s proprietary software source: %s", enable ? "Enable" : "Disable", repo_ids[i]);

      priv->enable_count++;
      pk_client_repo_enable_async (PK_CLIENT (priv->task),
                                   repo_ids[i],
                                   enable,
                                   cancellable,
                                   NULL, NULL,
                                   repo_enabled_cb,
                                   page);
    }
#endif

  return TRUE;
}

static gboolean
gis_software_page_apply (GisPage *gis_page,
                         GCancellable *cancellable)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (gis_page);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);
  gboolean enable;
  g_auto(GStrv) repo_ids = NULL;

  enable = gtk_switch_get_active (GTK_SWITCH (priv->proprietary_switch));

  g_debug ("%s proprietary software repositories", enable ? "Enable" : "Disable");

  g_settings_set_boolean (priv->software_settings, "show-nonfree-software", enable);
  /* don't prompt for the same thing again in gnome-software */
  g_settings_set_boolean (priv->software_settings, "show-nonfree-prompt", FALSE);

  repo_ids = g_settings_get_strv (priv->software_settings, "nonfree-sources");
  if (repo_ids == NULL || g_strv_length (repo_ids) == 0)
    return FALSE;

  return enable_repos (page, repo_ids, enable, cancellable);
}

static void
gis_software_page_locale_changed (GisPage *gis_page)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (gis_page);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);
  g_autoptr(GString) str = g_string_new (NULL);

  gis_page_set_title (GIS_PAGE (page), _("Software Repositories"));

  g_string_append (str,
                   /* TRANSLATORS: this is the third party repositories info bar. */
                   _("Access additional software from selected third party sources."));
  g_string_append (str, " ");
  g_string_append (str,
                   /* TRANSLATORS: this is the third party repositories info bar. */
                   _("Some of this software is proprietary and therefore has restrictions on use, sharing, and access to source code."));
  gtk_label_set_label (GTK_LABEL (priv->text_label), str->str);
}

static gboolean
activate_link (const char *label,
               const char *uri,
               gpointer    data)
{
  GisSoftwarePage *page = GIS_SOFTWARE_PAGE (data);
  GisSoftwarePagePrivate *priv = gis_software_page_get_instance_private (page);

  gtk_widget_show (priv->more_popover);

  return TRUE;
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
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, more_popover);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, proprietary_switch);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSoftwarePage, text_label);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), activate_link);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), state_set);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_software_page_locale_changed;
  page_class->apply = gis_software_page_apply;
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
#ifdef ENABLE_SOFTWARE_SOURCES
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, "org.gnome.software", TRUE);
  if (schema != NULL && g_settings_schema_has_key (schema, "show-nonfree-software"))
    gis_driver_add_page (driver,
                         g_object_new (GIS_TYPE_SOFTWARE_PAGE,
                                       "driver", driver,
                                       NULL));

  if (schema != NULL)
    g_settings_schema_unref (schema);
#endif
}
