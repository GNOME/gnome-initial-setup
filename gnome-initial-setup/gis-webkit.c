/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2015, 2024 Red Hat Inc.
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
 */

#include "config.h"
#include "gis-webkit.h"

#include <glib/gi18n.h>

#ifdef HAVE_WEBKITGTK
#include <webkit/webkit.h>
#endif

#ifdef HAVE_WEBKITGTK
static void
notify_progress_cb (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  GtkWidget *progress_bar = user_data;
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (object);
  gdouble progress;

  progress = webkit_web_view_get_estimated_load_progress (web_view);

  gtk_widget_set_visible (progress_bar, progress != 1.0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), progress);
}

static void
notify_title_cb (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  GtkWindow *dialog = user_data;
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (object);

  gtk_window_set_title (dialog, webkit_web_view_get_title (web_view));
}

gboolean
gis_activate_link (GtkLabel    *label,
                   const gchar *uri,
                   GtkWidget   *any_widget)
{
  GtkWidget *headerbar;
  GtkWidget *dialog;
  GtkWidget *overlay;
  GtkWidget *view;
  GtkWidget *progress_bar;

  headerbar = gtk_header_bar_new ();
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (headerbar), TRUE);

  dialog = g_object_new (GTK_TYPE_WINDOW,
                         "destroy-with-parent", TRUE,
                         "transient-for", gtk_widget_get_root (any_widget),
                         "titlebar", headerbar,
                         "title", "", /* use empty title until it can be filled, instead of briefly flashing the default title */
                         "modal", TRUE,
                         "default-width", 800,
                         "default-height", 600,
                         NULL);

  overlay = gtk_overlay_new ();
  gtk_window_set_child (GTK_WINDOW (dialog), overlay);

  progress_bar = gtk_progress_bar_new ();
  gtk_widget_add_css_class (progress_bar, "osd");
  gtk_widget_set_halign (progress_bar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (progress_bar, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), progress_bar);

  view = webkit_web_view_new ();
  gtk_widget_set_hexpand (view, TRUE);
  gtk_widget_set_vexpand (view, TRUE);
  g_signal_connect_object (view, "notify::estimated-load-progress",
                           G_CALLBACK (notify_progress_cb), progress_bar, 0);
  g_signal_connect_object (view, "notify::title",
                           G_CALLBACK (notify_title_cb), dialog, 0);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), view);

  gtk_window_present (GTK_WINDOW (dialog));

  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), uri);

  return TRUE;
}
#else
gboolean
gis_activate_link (GtkLabel    *label,
                   const gchar *uri,
                   GtkWidget   *any_widget)
{
  /* Fall back to default handler */
  return FALSE;
}
#endif
