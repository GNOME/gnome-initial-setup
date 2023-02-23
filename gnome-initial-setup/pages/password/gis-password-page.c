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

/* Password page {{{1 */

#define PAGE_ID "password"

#include "config.h"
#include "password-resources.h"
#include "gis-password-page.h"

#include "gis-keyring.h"

#include "pw-utils.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gis-page-header.h"

#define VALIDATION_TIMEOUT 600

struct _GisPasswordPagePrivate
{
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  GtkWidget *password_strength;
  GtkWidget *password_explanation;
  GtkWidget *confirm_explanation;
  GtkWidget *header;

  gboolean valid_confirm;
  gboolean valid_password;
  guint timeout_id;
  const gchar *username;
  gboolean parent_mode;
};
typedef struct _GisPasswordPagePrivate GisPasswordPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisPasswordPage, gis_password_page, GIS_TYPE_PAGE);

typedef enum
{
  PROP_PARENT_MODE = 1,
} GisPasswordPageProperty;

static GParamSpec *obj_props[PROP_PARENT_MODE + 1];

static void
clear_password_validation_error (GtkWidget *entry)
{
  gtk_widget_remove_css_class (entry, "error");
}

static void
set_password_validation_error (GtkWidget *entry)
{
  gtk_widget_add_css_class (entry, "error");
}

static void
update_header (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  g_autofree gchar *title = NULL;
  g_autofree gchar *subtitle = NULL;
  const gchar *icon_name;
  GdkPaintable *paintable;

#ifndef HAVE_PARENTAL_CONTROLS
  /* Don’t break UI compatibility if parental controls are disabled. */
  title = g_strdup (_("Set a Password"));
  subtitle = g_strdup (_("Be careful not to lose your password."));
  paintable = NULL;
  icon_name = "dialog-password-symbolic";
#else
  if (!priv->parent_mode)
    {
      /* Translators: The placeholder is for the user’s full name. */
      title = g_strdup_printf (_("Set a Password for %s"),
                               gis_driver_get_full_name (GIS_PAGE (page)->driver));
      subtitle = g_strdup (_("Be careful not to lose your password."));
      paintable = gis_driver_get_avatar (GIS_PAGE (page)->driver);
      icon_name = (paintable != NULL) ? NULL : "dialog-password-symbolic";
    }
  else
    {
      title = g_strdup (_("Set a Parent Password"));
      /* Translators: The placeholder is the full name of the child user on the system. */
      subtitle = g_strdup_printf (_("This password will control access to the parental controls for %s."),
                                  gis_driver_get_full_name (GIS_PAGE (page)->driver));
      icon_name = "org.freedesktop.MalcontentControl-symbolic";
      paintable = NULL;
    }
#endif

  /* Doesn’t make sense to set both. */
  g_assert (icon_name == NULL || paintable == NULL);

  g_object_set (G_OBJECT (priv->header),
                "title", title,
                "subtitle", subtitle,
                NULL);
  if (paintable != NULL)
    g_object_set (G_OBJECT (priv->header), "paintable", paintable, NULL);
  else if (icon_name != NULL)
    g_object_set (G_OBJECT (priv->header), "icon-name", icon_name, NULL);
}

static void
set_parent_mode (GisPasswordPage *page,
                 gboolean         parent_mode)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  g_return_if_fail (GIS_IS_PASSWORD_PAGE (page));

  if (priv->parent_mode == parent_mode)
    return;

  priv->parent_mode = parent_mode;
  g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_PARENT_MODE]);

  update_header (page);
}

static gboolean
page_validate (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  return priv->valid_confirm;
}

static void
update_page_validation (GisPasswordPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static gboolean
gis_password_page_save_data (GisPage  *gis_page,
                             GError  **error)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (gis_page);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  ActUser *act_user;
  UmAccountMode account_mode;
  const gchar *password = NULL;

  g_assert (gis_page->driver != NULL);

  account_mode = gis_driver_get_account_mode (gis_page->driver);

  if (!priv->parent_mode)
    gis_driver_get_user_permissions (gis_page->driver, &act_user, &password);
  else
    gis_driver_get_parent_permissions (gis_page->driver, &act_user, &password);

  if (account_mode == UM_ENTERPRISE) {
    g_assert (!priv->parent_mode);

    if (password != NULL)
      gis_update_login_keyring_password (password);
    return TRUE;
  }

  password = gtk_editable_get_text (GTK_EDITABLE (priv->password_entry));

  if (strlen (password) == 0)
    act_user_set_password_mode (act_user, ACT_USER_PASSWORD_MODE_NONE);
  else
    act_user_set_password (act_user, password, "");

  if (!priv->parent_mode)
    gis_driver_set_user_permissions (gis_page->driver, act_user, password);
  else
    gis_driver_set_parent_permissions (gis_page->driver, act_user, password);

  if (!priv->parent_mode)
    gis_update_login_keyring_password (password);

  return TRUE;
}

static void
gis_password_page_shown (GisPage *gis_page)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (gis_page);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  gtk_widget_grab_focus (priv->password_entry);
}

static gboolean
validate (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  const gchar *password;
  const gchar *verify;
  gint strength_level;
  const gchar *hint;

  g_clear_handle_id (&priv->timeout_id, g_source_remove);

  password = gtk_editable_get_text (GTK_EDITABLE (priv->password_entry));
  verify = gtk_editable_get_text (GTK_EDITABLE (priv->confirm_entry));

  pw_strength (password, NULL, priv->username, &hint, &strength_level);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (priv->password_strength), strength_level);
  gtk_label_set_label (GTK_LABEL (priv->password_explanation), hint);

  gtk_label_set_label (GTK_LABEL (priv->confirm_explanation), "");
  priv->valid_confirm = FALSE;

  priv->valid_password = (strlen (password) && strength_level > 1);
  if (priv->valid_password)
    clear_password_validation_error (priv->password_entry);
  else
    set_password_validation_error (priv->password_entry);

  if (strlen (password) > 0 && strlen (verify) > 0) {
    priv->valid_confirm = (strcmp (password, verify) == 0);
    if (!priv->valid_confirm)
      gtk_label_set_label (GTK_LABEL (priv->confirm_explanation), _("The passwords do not match."));
    else
      clear_password_validation_error (priv->password_entry);
  }

  /*
   * We deliberately don’t validate that the parent password and main user
   * password are different. It’s more feasible that someone would usefully
   * want to set their system up that way, than it is that the parent and child
   * would accidentally choose the same password.
   */

  update_page_validation (page);

  return G_SOURCE_REMOVE;
}

static gboolean
on_focusout (GisPasswordPage *page)
{
  validate (page);

  return FALSE;
}

static void
password_changed (GtkWidget      *w,
                  GParamSpec     *pspec,
                  GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  clear_password_validation_error (w);
  clear_password_validation_error (priv->confirm_entry);

  priv->valid_password = FALSE;
  update_page_validation (page);

  if (priv->timeout_id != 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate, page);
}

static void
confirm_changed (GtkWidget      *w,
                 GParamSpec     *pspec,
                 GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  clear_password_validation_error (w);

  priv->valid_confirm = FALSE;
  update_page_validation (page);

  if (priv->timeout_id != 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate, page);
}

static void
username_changed (GObject *obj, GParamSpec *pspec, GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  priv->username = gis_driver_get_username (GIS_DRIVER (obj));

  if (priv->username)
    gtk_widget_show (GTK_WIDGET (page));
  else
    gtk_widget_hide (GTK_WIDGET (page));  

  clear_password_validation_error (priv->password_entry);
  clear_password_validation_error (priv->confirm_entry);

  validate (page);
}

static void
full_name_or_avatar_changed (GObject    *obj,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (user_data);

  update_header (page);
}

static void
confirm (GisPasswordPage *page)
{
  if (page_validate (page))
    gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static void
track_focus_out (GisPasswordPage *page,
                 GtkWidget       *widget)
{
  GtkEventController *focus_controller;

  focus_controller = gtk_event_controller_focus_new ();
  gtk_widget_add_controller (widget, focus_controller);

  g_signal_connect_swapped (focus_controller, "leave", G_CALLBACK (on_focusout), page);
}

static void
gis_password_page_constructed (GObject *object)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (object);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_password_page_parent_class)->constructed (object);

  g_signal_connect (priv->password_entry, "notify::text",
                    G_CALLBACK (password_changed), page);
  g_signal_connect_swapped (priv->password_entry, "activate",
                            G_CALLBACK (confirm), page);
  track_focus_out (page, priv->password_entry);

  g_signal_connect (priv->confirm_entry, "notify::text",
                    G_CALLBACK (confirm_changed), page);
  g_signal_connect_swapped (priv->confirm_entry, "activate",
                            G_CALLBACK (confirm), page);
  track_focus_out (page, priv->confirm_entry);

  g_signal_connect (GIS_PAGE (page)->driver, "notify::username",
                    G_CALLBACK (username_changed), page);
  g_signal_connect (GIS_PAGE (page)->driver, "notify::full-name",
                    G_CALLBACK (full_name_or_avatar_changed), page);
  g_signal_connect (GIS_PAGE (page)->driver, "notify::avatar",
                    G_CALLBACK (full_name_or_avatar_changed), page);

  validate (page);
  update_header (page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_password_page_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (object);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  switch ((GisPasswordPageProperty) prop_id)
    {
    case PROP_PARENT_MODE:
      g_value_set_boolean (value, priv->parent_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_password_page_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (object);

  switch ((GisPasswordPageProperty) prop_id)
    {
    case PROP_PARENT_MODE:
      set_parent_mode (page, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_password_page_dispose (GObject *object)
{
  if (GIS_PAGE (object)->driver) {
    g_signal_handlers_disconnect_by_func (GIS_PAGE (object)->driver,
                                          username_changed, object);
    g_signal_handlers_disconnect_by_func (GIS_PAGE (object)->driver,
                                          full_name_or_avatar_changed, object);
  }

  G_OBJECT_CLASS (gis_password_page_parent_class)->dispose (object);
}

static void
gis_password_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Password"));
}

static void
gis_password_page_class_init (GisPasswordPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-password-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, confirm_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_strength);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_explanation);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, confirm_explanation);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, header);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_password_page_locale_changed;
  page_class->save_data = gis_password_page_save_data;
  page_class->shown = gis_password_page_shown;

  object_class->constructed = gis_password_page_constructed;
  object_class->get_property = gis_password_page_get_property;
  object_class->set_property = gis_password_page_set_property;
  object_class->dispose = gis_password_page_dispose;

  /**
   * GisPasswordPage:parent-mode:
   *
   * If %FALSE (the default), this page will collect a password for the main
   * user account. If %TRUE, it will collect a password for controlling access
   * to parental controls — this will affect where the password is stored, and
   * the appearance of the page.
   *
   * Since: 3.36
   */
  obj_props[PROP_PARENT_MODE] =
    g_param_spec_boolean ("parent-mode", "Parent Mode",
                          "Whether to collect a password for the main user account or a parent account.",
                          FALSE,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (obj_props), obj_props);

  gis_add_style_from_resource ("/org/gnome/initial-setup/gis-password-page.css");
}

static void
gis_password_page_init (GisPasswordPage *page)
{
  g_type_ensure (GIS_TYPE_PAGE_HEADER);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_password_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_PASSWORD_PAGE,
                       "driver", driver,
                       NULL);
}

GisPage *
gis_prepare_parent_password_page (GisDriver *driver)
{
  /* Skip prompting for the parent password if parental controls aren’t enabled. */
  if (!gis_driver_get_parental_controls_enabled (driver))
    return NULL;

  return g_object_new (GIS_TYPE_PASSWORD_PAGE,
                       "driver", driver,
                       "parent-mode", TRUE,
                       NULL);
}
