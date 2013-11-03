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

/* Account page {{{1 */

#define PAGE_ID "account"

#include "config.h"
#include "account-resources.h"
#include "gis-account-page.h"
#include "gis-account-page-local.h"
#include "gis-account-page-enterprise.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

typedef enum {
  UM_LOCAL,
  UM_ENTERPRISE,
  NUM_MODES,
} UmAccountMode;

struct _GisAccountPagePrivate
{
  GtkWidget *page_local;
  GtkWidget *page_enterprise;

  GtkWidget *page_toggle;
  GtkWidget *notebook;

  UmAccountMode mode;

  gboolean has_enterprise;
};
typedef struct _GisAccountPagePrivate GisAccountPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisAccountPage, gis_account_page, GIS_TYPE_PAGE);

static void
enterprise_apply_complete (GisPage  *dummy,
                           gboolean  valid,
                           gpointer  user_data)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (user_data);
  gis_page_apply_complete (GIS_PAGE (page), valid);
}

static gboolean
page_validate (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  switch (priv->mode) {
  case UM_LOCAL:
    return gis_account_page_local_validate (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local));
  case UM_ENTERPRISE:
    return gis_account_page_enterprise_validate (GIS_ACCOUNT_PAGE_ENTERPRISE (priv->page_enterprise));
  default:
    g_assert_not_reached ();
  }
}

static void
update_page_validation (GisAccountPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
on_validation_changed (gpointer        page_area,
                       GisAccountPage *page)
{
  update_page_validation (page);
}

static void
set_mode (GisAccountPage *page,
          UmAccountMode   mode)
{
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  if (priv->mode == mode)
    return;

  priv->mode = mode;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), (mode == UM_LOCAL) ? 0 : 1);

  update_page_validation (page);
}

static void
on_enterprise_visible_changed (GObject    *object,
                               GParamSpec *param,
                               gpointer    user_data)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (user_data);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);
  gboolean has_enterprise;

  has_enterprise = gtk_widget_get_visible (priv->page_enterprise);

  if (priv->has_enterprise == has_enterprise)
    return;

  priv->has_enterprise = has_enterprise;

  if (!has_enterprise)
    set_mode (page, UM_LOCAL);

  gtk_widget_set_visible (priv->page_toggle, has_enterprise);
}

static void
toggle_mode (GtkToggleButton *button,
             gpointer         user_data)
{
  set_mode (GIS_ACCOUNT_PAGE (user_data),
            gtk_toggle_button_get_active (button) ? UM_ENTERPRISE : UM_LOCAL);
}

static gboolean
gis_account_page_apply (GisPage *gis_page,
                        GCancellable *cancellable)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  switch (priv->mode) {
  case UM_LOCAL:
    return FALSE;
  case UM_ENTERPRISE:
    return gis_account_page_enterprise_apply (GIS_ACCOUNT_PAGE_ENTERPRISE (priv->page_enterprise), cancellable,
                                              enterprise_apply_complete, page);
    return TRUE;
  default:
    g_assert_not_reached ();
  }
}

static void
gis_account_page_save_data (GisPage *gis_page)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  switch (priv->mode) {
  case UM_LOCAL:
    gis_account_page_local_create_user (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local));
    break;
  case UM_ENTERPRISE:
    break;
  default:
    g_assert_not_reached ();
  }
}

static void
on_local_user_created (GtkWidget      *page_local,
                       ActUser        *user,
                       char           *password,
                       GisAccountPage *page)
{
  const gchar *language;

  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
gis_account_page_constructed (GObject *object)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (object);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_account_page_parent_class)->constructed (object);

  g_signal_connect (priv->page_local, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (priv->page_local, "user-created",
                    G_CALLBACK (on_local_user_created), page);

  g_signal_connect (priv->page_enterprise, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (priv->page_enterprise, "notify::visible",
                    G_CALLBACK (on_enterprise_visible_changed), page);

  update_page_validation (page);

  priv->has_enterprise = FALSE;

  g_signal_connect (priv->page_toggle, "toggled", G_CALLBACK (toggle_mode), page);
  g_object_bind_property (page, "applying", priv->page_toggle, "sensitive", G_BINDING_INVERT_BOOLEAN);

  /* force a refresh by setting to an invalid value */
  priv->mode = NUM_MODES;
  set_mode (page, UM_LOCAL);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_account_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Login"));
}

static void
gis_account_page_class_init (GisAccountPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, page_local);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, page_enterprise);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, page_toggle);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, notebook);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_account_page_locale_changed;
  page_class->apply = gis_account_page_apply;
  page_class->save_data = gis_account_page_save_data;
  object_class->constructed = gis_account_page_constructed;
}

static void
gis_account_page_init (GisAccountPage *page)
{
  g_resources_register (account_get_resource ());
  g_type_ensure (GIS_TYPE_ACCOUNT_PAGE_LOCAL);
  g_type_ensure (GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE);

  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_prepare_account_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_ACCOUNT_PAGE,
                                     "driver", driver,
                                     NULL));
}
