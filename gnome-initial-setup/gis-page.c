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

#include <glib-object.h>

#include "gis-page.h"

G_DEFINE_ABSTRACT_TYPE (GisPage, gis_page, GTK_TYPE_BIN);

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_PAGE, GisPagePrivate))

struct _GisPagePrivate
{
  char *title;

  guint complete : 1;
  guint padding : 6;
};

enum
{
  PROP_0,
  PROP_DRIVER,
  PROP_TITLE,
  PROP_COMPLETE,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

static void
gis_page_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GisPage *page = GIS_PAGE (object);
  switch (prop_id)
    {
    case PROP_DRIVER:
      g_value_set_object (value, page->driver);
      break;
    case PROP_TITLE:
      g_value_set_string (value, page->priv->title);
      break;
    case PROP_COMPLETE:
      g_value_set_boolean (value, page->priv->complete);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_page_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GisPage *page = GIS_PAGE (object);
  switch (prop_id)
    {
    case PROP_DRIVER:
      page->driver = g_value_dup_object (value);
      break;
    case PROP_TITLE:
      gis_page_set_title (page, (char *) g_value_get_string (value));
      break;
    case PROP_COMPLETE:
      page->priv->complete = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_page_finalize (GObject *object)
{
  GisPage *page = GIS_PAGE (object);

  g_free (page->priv->title);

  G_OBJECT_CLASS (gis_page_parent_class)->finalize (object);
}

static void
gis_page_dispose (GObject *object)
{
  GisPage *page = GIS_PAGE (object);

  g_clear_object (&page->driver);
  g_clear_object (&page->builder);

  G_OBJECT_CLASS (gis_page_parent_class)->dispose (object);
}

static GtkBuilder *
gis_page_real_get_builder (GisPage *page)
{
  GisPageClass *klass = GIS_PAGE_GET_CLASS (page);
  GtkBuilder *builder;
  gchar *resource_path;
  GError *error = NULL;

  if (klass->page_id == NULL)
    {
      g_warning ("Null page ID. Won't construct builder.");
      return NULL;
    }

  resource_path = g_strdup_printf ("/ui/gis-%s-page.ui", klass->page_id);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, resource_path, &error);

  g_free (resource_path);

  if (error != NULL) {
    g_warning ("Error while loading %s: %s", resource_path, error->message);
    goto err;
  }

  return builder;
 err:
  g_object_unref (builder);
  return NULL;
}

static void
gis_page_constructed (GObject *object)
{
  GisPage *page = GIS_PAGE (object);
  GisPageClass *klass = GIS_PAGE_GET_CLASS (page);

  page->builder = klass->get_builder (page);

  G_OBJECT_CLASS (gis_page_parent_class)->constructed (object);
}

static void
gis_page_class_init (GisPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gis_page_constructed;
  object_class->dispose = gis_page_dispose;
  object_class->finalize = gis_page_finalize;
  object_class->get_property = gis_page_get_property;
  object_class->set_property = gis_page_set_property;

  klass->get_builder = gis_page_real_get_builder;

  obj_props[PROP_DRIVER] =
    g_param_spec_object ("driver", "", "", GIS_TYPE_DRIVER,
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  obj_props[PROP_TITLE] =
    g_param_spec_string ("title", "", "", "",
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
  obj_props[PROP_COMPLETE] =
    g_param_spec_boolean ("complete", "", "", FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);

  g_type_class_add_private (object_class, sizeof(GisPagePrivate));
}

static void
gis_page_init (GisPage *page)
{
  page->priv = GET_PRIVATE (page);
}

char *
gis_page_get_title (GisPage *page)
{
  if (page->priv->title != NULL)
    return page->priv->title;
  else
    return "";
}

void
gis_page_set_title (GisPage *page, char *title)
{
  g_clear_pointer (&page->priv->title, g_free);
  page->priv->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_TITLE]);
}

gboolean
gis_page_get_complete (GisPage *page)
{
  return page->priv->complete;
}

void
gis_page_set_complete (GisPage *page, gboolean complete)
{
  page->priv->complete = complete;
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_COMPLETE]);
}

GtkWidget *
gis_page_get_action_widget (GisPage *page)
{
  if (GIS_PAGE_GET_CLASS (page)->get_action_widget)
    return GIS_PAGE_GET_CLASS (page)->get_action_widget (page);
  return NULL;
}
