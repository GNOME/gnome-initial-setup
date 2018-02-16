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

#include "config.h"

#include <glib-object.h>

#include "gis-page.h"

struct _GisPagePrivate
{
  char *title;

  gboolean applying;
  GCancellable *apply_cancel;
  GisPageApplyCallback apply_cb;
  gpointer apply_data;

  guint complete : 1;
  guint skippable : 1;
  guint needs_accept : 1;
  guint padding : 5;
};
typedef struct _GisPagePrivate GisPagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GisPage, gis_page, GTK_TYPE_BIN);

enum
{
  PROP_0,
  PROP_DRIVER,
  PROP_TITLE,
  PROP_COMPLETE,
  PROP_SKIPPABLE,
  PROP_NEEDS_ACCEPT,
  PROP_APPLYING,
  PROP_SMALL_SCREEN,
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
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  switch (prop_id)
    {
    case PROP_DRIVER:
      g_value_set_object (value, page->driver);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_COMPLETE:
      g_value_set_boolean (value, priv->complete);
      break;
    case PROP_SKIPPABLE:
      g_value_set_boolean (value, priv->skippable);
      break;
    case PROP_NEEDS_ACCEPT:
      g_value_set_boolean (value, priv->needs_accept);
      break;
    case PROP_APPLYING:
      g_value_set_boolean (value, gis_page_get_applying (page));
      break;
    case PROP_SMALL_SCREEN:
      if (page->driver)
        g_object_get_property (G_OBJECT (page->driver), "small-screen", value);
      else
        g_value_set_boolean (value, FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
small_screen_changed (GisPage *page)
{
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_SMALL_SCREEN]);
}

static void
gis_page_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GisPage *page = GIS_PAGE (object);
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  switch (prop_id)
    {
    case PROP_DRIVER:
      page->driver = g_value_dup_object (value);
      g_signal_connect_swapped (page->driver, "notify::small-screen",
                                G_CALLBACK (small_screen_changed), page);
      small_screen_changed (page);
      break;
    case PROP_TITLE:
      gis_page_set_title (page, (char *) g_value_get_string (value));
      break;
    case PROP_SKIPPABLE:
      priv->skippable = g_value_get_boolean (value);
      break;
    case PROP_NEEDS_ACCEPT:
      priv->needs_accept = g_value_get_boolean (value);
      break;
    case PROP_COMPLETE:
      priv->complete = g_value_get_boolean (value);
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
  GisPagePrivate *priv = gis_page_get_instance_private (page);

  g_free (priv->title);
  g_assert (!priv->applying);
  g_assert (priv->apply_cb == NULL);
  g_assert (priv->apply_cancel == NULL);

  G_OBJECT_CLASS (gis_page_parent_class)->finalize (object);
}

static void
gis_page_dispose (GObject *object)
{
  GisPage *page = GIS_PAGE (object);
  GisPagePrivate *priv = gis_page_get_instance_private (page);

  if (priv->apply_cancel)
    g_cancellable_cancel (priv->apply_cancel);

  if (page->driver)
    g_signal_handlers_disconnect_by_func (page->driver, small_screen_changed, page);
  g_clear_object (&page->driver);

  G_OBJECT_CLASS (gis_page_parent_class)->dispose (object);
}

static void
gis_page_constructed (GObject *object)
{
  GisPage *page = GIS_PAGE (object);

  gis_page_locale_changed (page);

  G_OBJECT_CLASS (gis_page_parent_class)->constructed (object);

}

static gboolean
gis_page_real_apply (GisPage      *page,
                     GCancellable *cancellable)
{
  return FALSE;
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

  klass->apply = gis_page_real_apply;

  obj_props[PROP_DRIVER] =
    g_param_spec_object ("driver", "", "", GIS_TYPE_DRIVER,
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  obj_props[PROP_TITLE] =
    g_param_spec_string ("title", "", "", "",
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
  obj_props[PROP_COMPLETE] =
    g_param_spec_boolean ("complete", "", "", FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
  obj_props[PROP_SKIPPABLE] =
    g_param_spec_boolean ("skippable", "", "", FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
  obj_props[PROP_NEEDS_ACCEPT] =
    g_param_spec_boolean ("needs-accept", "", "", FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
  obj_props[PROP_APPLYING] =
    g_param_spec_boolean ("applying", "", "", FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  obj_props[PROP_SMALL_SCREEN] =
    g_param_spec_boolean ("small-screen", "", "", FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
gis_page_init (GisPage *page)
{
  gtk_widget_set_margin_start (GTK_WIDGET (page), 12);
  gtk_widget_set_margin_top (GTK_WIDGET (page), 12);
  gtk_widget_set_margin_bottom (GTK_WIDGET (page), 12);
  gtk_widget_set_margin_end (GTK_WIDGET (page), 12);
}

char *
gis_page_get_title (GisPage *page)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  if (priv->title != NULL)
    return priv->title;
  else
    return "";
}

void
gis_page_set_title (GisPage *page, char *title)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  g_clear_pointer (&priv->title, g_free);
  priv->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_TITLE]);
}

gboolean
gis_page_get_complete (GisPage *page)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  return priv->complete;
}

void
gis_page_set_complete (GisPage *page, gboolean complete)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  priv->complete = complete;
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_COMPLETE]);
}

gboolean
gis_page_get_skippable (GisPage *page)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  return priv->skippable;
}

void
gis_page_set_skippable (GisPage *page, gboolean skippable)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  priv->skippable = skippable;
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_SKIPPABLE]);
}

gboolean
gis_page_get_needs_accept (GisPage *page)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  return priv->needs_accept;
}

void
gis_page_set_needs_accept (GisPage *page, gboolean needs_accept)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  priv->needs_accept = needs_accept;
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_NEEDS_ACCEPT]);
}

void
gis_page_locale_changed (GisPage *page)
{
  if (GIS_PAGE_GET_CLASS (page)->locale_changed)
    return GIS_PAGE_GET_CLASS (page)->locale_changed (page);
}

void
gis_page_apply_begin (GisPage                *page,
                      GisPageApplyCallback callback,
                      gpointer                user_data)
{
  GisPageClass *klass;
  GisPagePrivate *priv = gis_page_get_instance_private (page);

  g_return_if_fail (GIS_IS_PAGE (page));
  g_return_if_fail (priv->applying == FALSE);

  klass = GIS_PAGE_GET_CLASS (page);

  priv->apply_cb = callback;
  priv->apply_data = user_data;
  priv->apply_cancel = g_cancellable_new ();
  priv->applying = TRUE;

  if (!klass->apply (page, priv->apply_cancel))
    {
      /* Shortcut case where we don't want apply, to avoid flicker */
      gis_page_apply_complete (page, TRUE);
    }

  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_APPLYING]);
}

void
gis_page_apply_complete (GisPage *page,
                         gboolean valid)
{
  GisPageApplyCallback callback;
  gpointer user_data;
  GisPagePrivate *priv = gis_page_get_instance_private (page);

  g_return_if_fail (GIS_IS_PAGE (page));
  g_return_if_fail (priv->applying == TRUE);

  callback = priv->apply_cb;
  priv->apply_cb = NULL;
  user_data = priv->apply_data;
  priv->apply_data = NULL;

  g_clear_object (&priv->apply_cancel);
  priv->applying = FALSE;
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_APPLYING]);

  if (callback)
    (callback) (page, valid, user_data);
}

gboolean
gis_page_get_applying (GisPage *page)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  return priv->applying;
}

void
gis_page_apply_cancel (GisPage *page)
{
  GisPagePrivate *priv = gis_page_get_instance_private (page);
  g_cancellable_cancel (priv->apply_cancel);
}

void
gis_page_save_data (GisPage *page)
{
  if (GIS_PAGE_GET_CLASS (page)->save_data)
    GIS_PAGE_GET_CLASS (page)->save_data (page);
}

void
gis_page_shown (GisPage *page)
{
  if (GIS_PAGE_GET_CLASS (page)->shown)
    GIS_PAGE_GET_CLASS (page)->shown (page);
}

gboolean
gis_page_skip (GisPage *page)
{
  if (GIS_PAGE_GET_CLASS (page)->skip)
    return GIS_PAGE_GET_CLASS (page)->skip (page);
  return TRUE;
}
