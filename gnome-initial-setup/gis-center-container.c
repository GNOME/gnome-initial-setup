/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Red Hat
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
 *     Cosimo Cecchi <cosimoc@gnome.org>
 */

#include "gis-center-container.h"

struct _GisCenterContainerPrivate
{
  GtkWidget *left;
  GtkWidget *center;
  GtkWidget *right;
};

G_DEFINE_TYPE (GisCenterContainer, gis_center_container, GTK_TYPE_CONTAINER);

#define SPACING 6

static void
gis_center_container_init (GisCenterContainer *center)
{
  GisCenterContainerPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (center, GIS_TYPE_CENTER_CONTAINER, GisCenterContainerPrivate);
  center->priv = priv;

  gtk_widget_set_has_window (GTK_WIDGET (center), FALSE);
}

static void
gis_center_container_get_preferred_width (GtkWidget *widget,
                                          gint      *minimum_size,
                                          gint      *natural_size)
{
  GisCenterContainer *center = GIS_CENTER_CONTAINER (widget);
  GisCenterContainerPrivate *priv = center->priv;

  gint sum_min, sum_nat;
  gint child_min, child_nat;

  gtk_widget_get_preferred_width (priv->left, &child_min, &child_nat);
  sum_min = child_min;
  sum_nat = child_nat;

  gtk_widget_get_preferred_width (priv->center, &child_min, &child_nat);
  sum_min = sum_min + child_min;
  sum_nat = sum_nat + child_nat + SPACING;

  gtk_widget_get_preferred_width (priv->right, &child_min, &child_nat);
  sum_min = sum_min + child_min;
  sum_nat = sum_nat + child_nat + SPACING;

  if (minimum_size)
    *minimum_size = sum_min;
  if (natural_size)
    *natural_size = sum_nat;
}

static void
gis_center_container_get_preferred_height (GtkWidget *widget,
                                           gint      *minimum_size,
                                           gint      *natural_size)
{
  GisCenterContainer *center = GIS_CENTER_CONTAINER (widget);
  GisCenterContainerPrivate *priv = center->priv;
  gint max_min, max_nat;
  gint child_min, child_nat;

  gtk_widget_get_preferred_height (priv->left, &child_min, &child_nat);
  max_min = child_min;
  max_nat = child_nat;

  gtk_widget_get_preferred_height (priv->center, &child_min, &child_nat);
  max_min = MAX (max_min, child_min);
  max_nat = MAX (max_nat, child_nat);

  gtk_widget_get_preferred_height (priv->right, &child_min, &child_nat);
  max_min = MAX (max_min, child_min);
  max_nat = MAX (max_nat, child_nat);

  if (minimum_size)
    *minimum_size = max_min;
  if (natural_size)
    *natural_size = max_nat;
}

static void
gis_center_container_size_allocate (GtkWidget     *widget,
                                    GtkAllocation *allocation)
{
  GisCenterContainer *center = GIS_CENTER_CONTAINER (widget);
  GisCenterContainerPrivate *priv = center->priv;
  GtkAllocation child_allocation;
  gint max_side;

  gtk_widget_set_allocation (widget, allocation);

  child_allocation.y = allocation->y;
  child_allocation.height = allocation->height;

  /* XXX -- fix minimum allocations */
  {
    gint left_side, right_side;
    gtk_widget_get_preferred_width (priv->left, NULL, &left_side);
    gtk_widget_get_preferred_width (priv->right, NULL, &right_side);
    max_side = MAX (left_side, right_side);
  }

  child_allocation.width = max_side;

  child_allocation.x = allocation->x;
  gtk_widget_size_allocate (priv->left, &child_allocation);

  child_allocation.x = allocation->x + allocation->width - max_side;
  gtk_widget_size_allocate (priv->right, &child_allocation);

  child_allocation.x = allocation->x + max_side + SPACING;
  child_allocation.width = allocation->width - max_side * 2 - SPACING * 2;
  gtk_widget_size_allocate (priv->center, &child_allocation);
}

static GType
gis_center_container_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gis_center_container_forall (GtkContainer *container,
                             gboolean      include_internals,
                             GtkCallback   callback,
                             gpointer      callback_data)
{
  GisCenterContainer *center = GIS_CENTER_CONTAINER (container);
  GisCenterContainerPrivate *priv = center->priv;

  callback (priv->left, callback_data);
  callback (priv->center, callback_data);
  callback (priv->right, callback_data);
}

static void
gis_center_container_class_init (GisCenterContainerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  widget_class->size_allocate = gis_center_container_size_allocate;
  widget_class->get_preferred_width = gis_center_container_get_preferred_width;
  widget_class->get_preferred_height = gis_center_container_get_preferred_height;

  container_class->forall = gis_center_container_forall;
  container_class->child_type = gis_center_container_child_type;
  gtk_container_class_handle_border_width (container_class);

  g_type_class_add_private (object_class, sizeof (GisCenterContainerPrivate));
}

GtkWidget *
gis_center_container_new (GtkWidget *left,
                          GtkWidget *center,
                          GtkWidget *right)
{
  GisCenterContainer *container = g_object_new (GIS_TYPE_CENTER_CONTAINER, NULL);
  GisCenterContainerPrivate *priv = container->priv;

  priv->left = left;
  gtk_widget_set_parent (left, GTK_WIDGET (container));

  priv->center = center;
  gtk_widget_set_parent (center, GTK_WIDGET (container));

  priv->right = right;
  gtk_widget_set_parent (right, GTK_WIDGET (container));

  return GTK_WIDGET (container);
}

