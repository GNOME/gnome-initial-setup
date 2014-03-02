/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2014 Red Hat
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
 */

#include "config.h"

#include "gis-prompt.h"

#include <gcr/gcr.h>

/* This code is inspired by GcrMockPrompt - we never show any UI,
 * and just return TRUE for confirmations and "gis" for passwords.
 */

enum {
        PROP_0,

        PROP_TITLE,
        PROP_MESSAGE,
        PROP_DESCRIPTION,
        PROP_WARNING,
        PROP_PASSWORD_NEW,
        PROP_PASSWORD_STRENGTH,
        PROP_CHOICE_LABEL,
        PROP_CHOICE_CHOSEN,
        PROP_CALLER_WINDOW,
        PROP_CONTINUE_LABEL,
        PROP_CANCEL_LABEL
};

static void    gis_prompt_iface     (GcrPromptIface *iface);

G_DEFINE_TYPE_WITH_CODE (GisPrompt, gis_prompt, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_PROMPT, gis_prompt_iface))

static void
property_free (gpointer data)
{
        GParameter *param = data;
        g_value_unset (&param->value);
        g_free (param);
}

static void
blank_string_property (GHashTable *properties,
                       const gchar *property)
{
        GParameter *param;

        param = g_new0 (GParameter, 1);
        param->name = property;
        g_value_init (&param->value, G_TYPE_STRING);
        g_value_set_string (&param->value, "");
        g_hash_table_insert (properties, (gpointer)param->name, param);
}

static void
blank_boolean_property (GHashTable *properties,
                        const gchar *property)
{
        GParameter *param;

        param = g_new0 (GParameter, 1);
        param->name = property;
        g_value_init (&param->value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&param->value, FALSE);
        g_hash_table_insert (properties, (gpointer)param->name, param);
}

static void
blank_int_property (GHashTable *properties,
                    const gchar *property)
{
        GParameter *param;

        param = g_new0 (GParameter, 1);
        param->name = property;
        g_value_init (&param->value, G_TYPE_INT);
        g_value_set_int (&param->value, 0);
        g_hash_table_insert (properties, (gpointer)param->name, param);
}

static void
gis_prompt_init (GisPrompt *self)
{
        self->properties = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  NULL, property_free);

        blank_string_property (self->properties, "title");
        blank_string_property (self->properties, "message");
        blank_string_property (self->properties, "description");
        blank_string_property (self->properties, "warning");
        blank_string_property (self->properties, "choice-label");
        blank_string_property (self->properties, "caller-window");
        blank_string_property (self->properties, "continue-label");
        blank_string_property (self->properties, "cancel-label");

        blank_boolean_property (self->properties, "choice-chosen");
        blank_boolean_property (self->properties, "password-new");

        blank_int_property (self->properties, "password-strength");
}

static void
gis_prompt_set_property (GObject *obj,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
        GisPrompt *self = GIS_PROMPT (obj);
        GParameter *param;

        switch (prop_id) {
        case PROP_TITLE:
        case PROP_MESSAGE:
        case PROP_DESCRIPTION:
        case PROP_WARNING:
        case PROP_PASSWORD_NEW:
        case PROP_CHOICE_LABEL:
        case PROP_CHOICE_CHOSEN:
        case PROP_CALLER_WINDOW:
        case PROP_CONTINUE_LABEL:
        case PROP_CANCEL_LABEL:
                param = g_new0 (GParameter, 1);
                param->name = pspec->name;
                g_value_init (&param->value, pspec->value_type);
                g_value_copy (value, &param->value);
                g_hash_table_replace (self->properties, (gpointer)param->name, param);
                g_object_notify (G_OBJECT (self), param->name);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

static void
gis_prompt_get_property (GObject *obj,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
        GisPrompt *self = GIS_PROMPT (obj);
        GParameter *param;

        switch (prop_id) {
        case PROP_TITLE:
        case PROP_MESSAGE:
        case PROP_DESCRIPTION:
        case PROP_WARNING:
        case PROP_PASSWORD_NEW:
        case PROP_PASSWORD_STRENGTH:
        case PROP_CHOICE_LABEL:
        case PROP_CHOICE_CHOSEN:
        case PROP_CALLER_WINDOW:
        case PROP_CONTINUE_LABEL:
        case PROP_CANCEL_LABEL:
                param = g_hash_table_lookup (self->properties, pspec->name);
                g_return_if_fail (param != NULL);
                g_value_copy (&param->value, value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

static void
gis_prompt_finalize (GObject *obj)
{
        GisPrompt *self = GIS_PROMPT (obj);

        g_hash_table_destroy (self->properties);

        G_OBJECT_CLASS (gis_prompt_parent_class)->finalize (obj);
}

static void
gis_prompt_class_init (GisPromptClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->get_property = gis_prompt_get_property;
        gobject_class->set_property = gis_prompt_set_property;
        gobject_class->finalize = gis_prompt_finalize;

        g_object_class_override_property (gobject_class, PROP_TITLE, "title");
        g_object_class_override_property (gobject_class, PROP_MESSAGE, "message");
        g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
        g_object_class_override_property (gobject_class, PROP_WARNING, "warning");
        g_object_class_override_property (gobject_class, PROP_CALLER_WINDOW, "caller-window");
        g_object_class_override_property (gobject_class, PROP_CHOICE_LABEL, "choice-label");
        g_object_class_override_property (gobject_class, PROP_CHOICE_CHOSEN, "choice-chosen");
        g_object_class_override_property (gobject_class, PROP_PASSWORD_NEW, "password-new");
        g_object_class_override_property (gobject_class, PROP_PASSWORD_STRENGTH, "password-strength");
        g_object_class_override_property (gobject_class, PROP_CONTINUE_LABEL, "continue-label");
        g_object_class_override_property (gobject_class, PROP_CANCEL_LABEL, "cancel-label");
}

static gboolean
on_timeout_complete (gpointer data)
{
        GSimpleAsyncResult *res = data;
        g_simple_async_result_complete (res);
        return FALSE;
}

static void
destroy_unref_source (gpointer source)
{
        if (!g_source_is_destroyed (source))
                g_source_destroy (source);
        g_source_unref (source);
}

static void
gis_prompt_confirm_async (GcrPrompt *prompt,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
        GisPrompt *self = GIS_PROMPT (prompt);
        GSourceFunc complete_func = on_timeout_complete;
        GSimpleAsyncResult *res;
        GSource *source;
        guint delay_msec;

        res = g_simple_async_result_new (G_OBJECT (prompt), callback, user_data,
                                         gis_prompt_confirm_async);
        g_simple_async_result_set_op_res_gboolean (res, TRUE);

        source = g_idle_source_new ();
        g_source_set_callback (source, on_timeout_complete, g_object_ref (res), g_object_unref);
        g_source_attach (source, NULL);
        g_object_set_data_full (G_OBJECT (self), "delay-source", source, destroy_unref_source);

        g_object_unref (res);
}

static GcrPromptReply
gis_prompt_confirm_finish (GcrPrompt *prompt,
                           GAsyncResult *result,
                           GError **error)
{
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
                              gis_prompt_confirm_async), GCR_PROMPT_REPLY_CANCEL);

        return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (result)) ?
                       GCR_PROMPT_REPLY_CONTINUE : GCR_PROMPT_REPLY_CANCEL;
}


static void
gis_prompt_password_async (GcrPrompt *prompt,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
        GisPrompt *self = GIS_PROMPT (prompt);
        GSourceFunc complete_func = on_timeout_complete;
        GSimpleAsyncResult *res;
        GSource *source;
        guint delay_msec;

        res = g_simple_async_result_new (G_OBJECT (prompt), callback, user_data,
                                         gis_prompt_password_async);

        g_simple_async_result_set_op_res_gpointer (res, "gis", NULL);

        source = g_idle_source_new ();
        g_source_set_callback (source, on_timeout_complete, g_object_ref (res), g_object_unref);
        g_source_attach (source, NULL);
        g_object_set_data_full (G_OBJECT (self), "delay-source", source, destroy_unref_source);

        g_object_unref (res);
}

static const gchar *
gis_prompt_password_finish (GcrPrompt *prompt,
                            GAsyncResult *result,
                            GError **error)
{
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (prompt),
                              gis_prompt_password_async), NULL);

        return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
}

static void
gis_prompt_iface (GcrPromptIface *iface)
{
        iface->prompt_confirm_async = gis_prompt_confirm_async;
        iface->prompt_confirm_finish = gis_prompt_confirm_finish;
        iface->prompt_password_async = gis_prompt_password_async;
        iface->prompt_password_finish = gis_prompt_password_finish;
}
