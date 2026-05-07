/* gweather-location-entry.c - Location-selecting text entry
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gis-location-entry.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <geocode-glib/geocode-glib.h>

/**
 * GisLocationEntry:
 *
 * A subclass of [class@Gtk.SearchEntry] that provides autocompletion on
 * [struct@GWeather.Location]s.
 *
 */

struct _GisLocationEntryPrivate {
    GtkWidget        *entry;
    GWeatherLocation *location;
    GWeatherLocation *top;
    gboolean          show_named_timezones;
    GCancellable     *cancellable;
    GtkTreeModel     *model;
    GtkTreeModel     *active_model;
    gboolean          active_model_is_geocode;
};

static void editable_iface_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GisLocationEntry, gis_location_entry, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GisLocationEntry)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, editable_iface_init));

typedef enum {
    PROP_TOP = 1,
    PROP_SHOW_NAMED_TIMEZONES,
    PROP_LOCATION,
} GisLocationEntryProps;

static GParamSpec *props[PROP_LOCATION + 1] = { NULL, };

static void set_property (GObject *object, guint prop_id,
                          const GValue *value, GParamSpec *pspec);
static void get_property (GObject *object, guint prop_id,
                          GValue *value, GParamSpec *pspec);

static void set_location_internal (GisLocationEntry      *entry,
                                   GtkTreeModel          *model,
                                   GtkTreeIter           *iter,
                                   GWeatherLocation      *loc);
static void
fill_location_entry_model (GtkListStore *store, GWeatherLocation *loc,
                           const char *parent_display_name,
                           const char *parent_sort_local_name,
                           const char *parent_compare_local_name,
                           const char *parent_compare_english_name,
                           gboolean show_named_timezones);

static gboolean on_entry_key_pressed (GtkEventControllerKey *controller,
                                      guint                  keyval,
                                      guint                  keycode,
                                      GdkModifierType        state,
                                      gpointer               user_data);

static void on_entry_focus_leave (GtkEventControllerFocus *controller,
                            gpointer                 user_data);

enum LOC
{
    LOC_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME = 0,
    LOC_GIS_LOCATION_ENTRY_COL_LOCATION,
    LOC_GIS_LOCATION_ENTRY_COL_LOCAL_SORT_NAME,
    LOC_GIS_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME,
    LOC_GIS_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME,
    LOC_GIS_LOCATION_ENTRY_NUM_COLUMNS
};

enum PLACE
{
    PLACE_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME = 0,
    PLACE_GIS_LOCATION_ENTRY_COL_PLACE,
    PLACE_GIS_LOCATION_ENTRY_COL_LOCAL_SORT_NAME,
    PLACE_GIS_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME
};

static gboolean model_row_matches (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   const char   *key);
static gboolean active_model_contains_exact_text (GisLocationEntry *entry,
                                                  const char       *text,
                                                  GtkTreeIter      *iter_out);
static void     entry_changed (GisLocationEntry *entry);
static void     trigger_geocode_search (GisLocationEntry *entry);
static void     restore_local_model (GisLocationEntry *entry);
static void     set_active_model (GisLocationEntry *entry,
                                  GtkTreeModel     *model,
                                  gboolean          is_geocode);

static GtkEditable*
gis_location_entry_get_delegate (GtkEditable *editable)
{
    GisLocationEntry *entry = GIS_LOCATION_ENTRY (editable);
    GisLocationEntryPrivate *priv = gis_location_entry_get_instance_private (entry);

    return GTK_EDITABLE (priv->entry);
}

static void
editable_iface_init (GtkEditableInterface *iface)
{
    iface->get_delegate = gis_location_entry_get_delegate;
}

static void
gis_location_entry_init (GisLocationEntry *entry)
{
    GisLocationEntryPrivate *priv;
    GtkEventController *key;
    GtkEventController *focus;

    priv = entry->priv = gis_location_entry_get_instance_private (entry);

    priv->entry = gtk_search_entry_new ();
    gtk_editable_set_text (GTK_EDITABLE (priv->entry), "");
    gtk_widget_set_parent (priv->entry, GTK_WIDGET (entry));
    gtk_editable_init_delegate (GTK_EDITABLE (entry));

    gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (priv->entry), GTK_WIDGET (entry));
    gtk_widget_set_tooltip_text (priv->entry, _("Search cities"));

    g_signal_connect (entry, "changed",
                      G_CALLBACK (entry_changed), NULL);

    key = gtk_event_controller_key_new ();
    g_signal_connect (key, "key-pressed",
                      G_CALLBACK (on_entry_key_pressed), entry);
    gtk_widget_add_controller (priv->entry, key);

    focus = gtk_event_controller_focus_new ();
    g_signal_connect (focus, "leave",
                      G_CALLBACK (on_entry_focus_leave), entry);
    gtk_widget_add_controller (priv->entry, focus);
}

static void
finalize (GObject *object)
{
    GisLocationEntry *entry;
    GisLocationEntryPrivate *priv;

    entry = GIS_LOCATION_ENTRY (object);
    priv = entry->priv;

    g_clear_object (&priv->location);
    g_clear_object (&priv->top);
    g_clear_object (&priv->model);
    g_clear_object (&priv->active_model);

    G_OBJECT_CLASS (gis_location_entry_parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
    GisLocationEntry *entry;
    GisLocationEntryPrivate *priv;

    entry = GIS_LOCATION_ENTRY (object);
    priv = entry->priv;

    if (priv->cancellable) {
        g_cancellable_cancel (priv->cancellable);
        g_clear_object (&priv->cancellable);
    }

    gtk_editable_finish_delegate (GTK_EDITABLE (entry));
    g_clear_pointer (&priv->entry, gtk_widget_unparent);

    G_OBJECT_CLASS (gis_location_entry_parent_class)->dispose (object);
}

static int
tree_compare_local_name (GtkTreeModel *model,
                         GtkTreeIter  *a,
                         GtkTreeIter  *b,
                         gpointer      user_data)
{
    g_autofree gchar *name_a = NULL, *name_b = NULL;

    gtk_tree_model_get (model, a,
                        LOC_GIS_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, &name_a,
                        -1);
    gtk_tree_model_get (model, b,
                        LOC_GIS_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, &name_b,
                        -1);

    return g_utf8_collate (name_a, name_b);
}

static void
set_active_model (GisLocationEntry *entry,
                  GtkTreeModel     *model,
                  gboolean          is_geocode)
{
    g_set_object (&entry->priv->active_model, model);
    entry->priv->active_model_is_geocode = is_geocode;
}

static void
restore_local_model (GisLocationEntry *entry)
{
    set_active_model (entry, entry->priv->model, FALSE);
}

static void
constructed (GObject *object)
{
    GisLocationEntry *entry;
    GtkListStore *store = NULL;

    entry = GIS_LOCATION_ENTRY (object);

    if (!entry->priv->top)
        entry->priv->top = gweather_location_get_world ();

    store = gtk_list_store_new (5, G_TYPE_STRING, GWEATHER_TYPE_LOCATION, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                             tree_compare_local_name, NULL, NULL);
    fill_location_entry_model (store, entry->priv->top, NULL, NULL, NULL, NULL, entry->priv->show_named_timezones);

    entry->priv->model = GTK_TREE_MODEL (store);
    restore_local_model (entry);

    G_OBJECT_CLASS (gis_location_entry_parent_class)->constructed (object);
}

static gboolean
grab_focus (GtkWidget *self)
{
    GisLocationEntryPrivate *priv = gis_location_entry_get_instance_private (GIS_LOCATION_ENTRY (self));
    return gtk_widget_grab_focus (priv->entry);
}

static void
gis_location_entry_class_init (GisLocationEntryClass *location_entry_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (location_entry_class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (location_entry_class);

    object_class->constructed = constructed;
    object_class->finalize = finalize;
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;

    /* properties */
    props[PROP_TOP] = g_param_spec_object ("top",
                                           NULL, NULL,
                                           GWEATHER_TYPE_LOCATION,
                                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    props[PROP_SHOW_NAMED_TIMEZONES] = g_param_spec_boolean ("show-named-timezones",
                                                             NULL, NULL,
                                                             FALSE,
                                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    props[PROP_LOCATION] = g_param_spec_object ("location",
                                                NULL, NULL,
                                                GWEATHER_TYPE_LOCATION,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

    widget_class->grab_focus = grab_focus;
    gtk_editable_install_properties (object_class, PROP_LOCATION);

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
    GisLocationEntry *entry = GIS_LOCATION_ENTRY (object);

    if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
      return;

    switch ((GisLocationEntryProps) prop_id) {
    case PROP_TOP:
        entry->priv->top = g_value_dup_object (value);
        break;
    case PROP_SHOW_NAMED_TIMEZONES:
        entry->priv->show_named_timezones = g_value_get_boolean (value);
        break;
    case PROP_LOCATION:
        gis_location_entry_set_location (GIS_LOCATION_ENTRY (object),
                                         g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
    GisLocationEntry *entry = GIS_LOCATION_ENTRY (object);

    if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
      return;

    switch ((GisLocationEntryProps) prop_id) {
    case PROP_SHOW_NAMED_TIMEZONES:
        g_value_set_boolean (value, entry->priv->show_named_timezones);
        break;
    case PROP_LOCATION:
        g_value_set_object (value, entry->priv->location);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
entry_changed (GisLocationEntry *entry)
{
    const gchar *text;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean found_local_match = FALSE;

    if (entry->priv->cancellable) {
        g_cancellable_cancel (entry->priv->cancellable);
        g_clear_object (&entry->priv->cancellable);
    }

    restore_local_model (entry);

    text = gtk_editable_get_text (GTK_EDITABLE (entry));

    if (!text || *text == '\0') {
        set_location_internal (entry, NULL, NULL, NULL);
        return;
    }

    model = entry->priv->model;

    if (gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            if (model_row_matches (model, &iter, text)) {
                found_local_match = TRUE;
                break;
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }

    if (!found_local_match)
        trigger_geocode_search (entry);
}

static void
set_entry_text (GisLocationEntry *entry,
                const char       *text)
{
    GisLocationEntryPrivate *priv = entry->priv;

    if (g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (priv->entry)), text) != 0)
        gtk_editable_set_text (GTK_EDITABLE (priv->entry), text);
}

static void
set_location_internal (GisLocationEntry *entry,
                       GtkTreeModel     *model,
                       GtkTreeIter      *iter,
                       GWeatherLocation *loc)
{
    GisLocationEntryPrivate *priv;
    char *name;

    priv = entry->priv;

    g_clear_object (&priv->location);

    g_assert (iter == NULL || loc == NULL);

    g_signal_handlers_block_by_func (entry, entry_changed, NULL);

    if (iter) {
        gtk_tree_model_get (model, iter,
                            LOC_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME, &name,
                            LOC_GIS_LOCATION_ENTRY_COL_LOCATION, &priv->location,
                            -1);
        set_entry_text (entry, name);
        g_free (name);
    } else if (loc) {
        priv->location = g_object_ref (loc);
        set_entry_text (entry, gweather_location_get_name (loc));
    } else {
        priv->location = NULL;
        set_entry_text (entry, "");
    }

    g_signal_handlers_unblock_by_func (entry, entry_changed, NULL);

    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_LOCATION]);
}

/**
 * gis_location_entry_set_location:
 * @entry: a #GisLocationEntry
 * @loc: (allow-none): a #GWeatherLocation in @entry, or %NULL to
 * clear @entry
 *
 * Sets @entry's location to @loc, and updates the text of the
 * entry accordingly.
 * Note that if the database contains a location that compares
 * equal to @loc, that will be chosen in place of @loc.
 **/
void
gis_location_entry_set_location (GisLocationEntry *entry,
                                 GWeatherLocation *loc)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    g_autoptr(GWeatherLocation) cmploc = NULL;

    g_return_if_fail (GIS_IS_LOCATION_ENTRY (entry));

    model = entry->priv->model;

    if (loc == NULL) {
        set_location_internal (entry, model, NULL, NULL);
        return;
    }
    if (model != NULL && gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            gtk_tree_model_get (model, &iter,
                                LOC_GIS_LOCATION_ENTRY_COL_LOCATION, &cmploc,
                                -1);
            if (gweather_location_equal (loc, cmploc)) {
                set_location_internal (entry, model, &iter, NULL);
                return;
            }

            g_clear_object (&cmploc);
        } while (gtk_tree_model_iter_next (model, &iter));
    }

    set_location_internal (entry, model, NULL, loc);
}

/**
 * gis_location_entry_get_location:
 * @entry: a #GisLocationEntry
 *
 * Gets the location that was set by a previous call to
 * gis_location_entry_set_location() or was selected by the user.
 *
 * Return value: (transfer full) (allow-none): the selected location
 * (which you must unref when you are done with it), or %NULL if no
 * location is selected.
 **/
GWeatherLocation *
gis_location_entry_get_location (GisLocationEntry *entry)
{
    g_return_val_if_fail (GIS_IS_LOCATION_ENTRY (entry), NULL);

    if (entry->priv->location)
        return g_object_ref (entry->priv->location);
    else
        return NULL;
}

/**
 * gis_location_entry_set_city:
 * @entry: a #GisLocationEntry
 * @city_name: (allow-none): the city name, or %NULL
 * @code: the METAR station code
 *
 * Sets @entry's location to a city with the given @code, and given
 * @city_name, if non-%NULL. If there is no matching city, sets
 * @entry's location to %NULL.
 *
 * Return value: %TRUE if @entry's location could be set to a matching city,
 * %FALSE otherwise.
 **/
gboolean
gis_location_entry_set_city (GisLocationEntry *entry,
                             const char       *city_name,
                             const char       *code)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    const char *cmpcode;

    g_return_val_if_fail (GIS_IS_LOCATION_ENTRY (entry), FALSE);
    g_return_val_if_fail (code != NULL, FALSE);

    model = entry->priv->model;

    if (model == NULL)
        return FALSE;

    gtk_tree_model_get_iter_first (model, &iter);
    do {
        g_autoptr(GWeatherLocation) cmploc = NULL;

        gtk_tree_model_get (model, &iter,
                            LOC_GIS_LOCATION_ENTRY_COL_LOCATION, &cmploc,
                            -1);

        cmpcode = gweather_location_get_code (cmploc);
        if (!cmpcode || strcmp (cmpcode, code) != 0)
            continue;

        if (city_name) {
            g_autofree gchar *cmpname = gweather_location_get_city_name (cmploc);
            if (!cmpname || strcmp (cmpname, city_name) != 0)
                continue;
        }

        set_location_internal (entry, model, &iter, NULL);
        return TRUE;
    } while (gtk_tree_model_iter_next (model, &iter));

    set_location_internal (entry, model, NULL, NULL);
    return FALSE;
}

static void
fill_location_entry_model (GtkListStore *store, GWeatherLocation *loc,
                           const char *parent_display_name,
                           const char *parent_sort_local_name,
                           const char *parent_compare_local_name,
                           const char *parent_compare_english_name,
                           gboolean show_named_timezones)
{
    g_autoptr(GWeatherLocation) child = NULL;
    char *display_name, *local_sort_name, *local_compare_name, *english_compare_name;

    switch (gweather_location_get_level (loc)) {
    case GWEATHER_LOCATION_WORLD:
    case GWEATHER_LOCATION_REGION:
        /* Ignore these levels of hierarchy; just recurse, passing on
         * the names from the parent node.
         */
        while ((child = gweather_location_next_child (loc, child)))
            fill_location_entry_model (store, child,
                                       parent_display_name,
                                       parent_sort_local_name,
                                       parent_compare_local_name,
                                       parent_compare_english_name,
                                       show_named_timezones);
        break;

    case GWEATHER_LOCATION_COUNTRY:
        /* Recurse, initializing the names to the country name */
        while ((child = gweather_location_next_child (loc, child)))
            fill_location_entry_model (store, child,
                                       gweather_location_get_name (loc),
                                       gweather_location_get_sort_name (loc),
                                       gweather_location_get_sort_name (loc),
                                       gweather_location_get_english_sort_name (loc),
                                       show_named_timezones);
        break;

    case GWEATHER_LOCATION_ADM1:
        /* Recurse, adding the ADM1 name to the country name */
        /* Translators: this is the name of a location followed by a region, for example:
         * 'London, United Kingdom'
         * You shouldn't need to translate this string unless the language has a different comma.
         */
        display_name = g_strdup_printf (_("%s, %s"), gweather_location_get_name (loc), parent_display_name);
        local_sort_name = g_strdup_printf ("%s, %s", parent_sort_local_name, gweather_location_get_sort_name (loc));
        local_compare_name = g_strdup_printf ("%s, %s", gweather_location_get_sort_name (loc), parent_compare_local_name);
        english_compare_name = g_strdup_printf ("%s, %s", gweather_location_get_english_sort_name (loc), parent_compare_english_name);

        while ((child = gweather_location_next_child (loc, child)))
            fill_location_entry_model (store, child,
                                       display_name, local_sort_name, local_compare_name, english_compare_name,
                                       show_named_timezones);

        g_free (display_name);
        g_free (local_sort_name);
        g_free (local_compare_name);
        g_free (english_compare_name);
        break;

    case GWEATHER_LOCATION_CITY:
        if (gweather_location_get_timezone (loc) == NULL) {
            break;
        }

        /* Translators: this is the name of a location followed by a region, for example:
         * 'London, United Kingdom'
         * You shouldn't need to translate this string unless the language has a different comma.
         */
        display_name = g_strdup_printf (_("%s, %s"),
                                        gweather_location_get_name (loc), parent_display_name);
        local_sort_name = g_strdup_printf ("%s, %s",
                                           parent_sort_local_name, gweather_location_get_sort_name (loc));
        local_compare_name = g_strdup_printf ("%s, %s",
                                              gweather_location_get_sort_name (loc), parent_compare_local_name);
        english_compare_name = g_strdup_printf ("%s, %s",
                                                gweather_location_get_english_sort_name (loc), parent_compare_english_name);

        gtk_list_store_insert_with_values (store, NULL, -1,
                                           LOC_GIS_LOCATION_ENTRY_COL_LOCATION, loc,
                                           LOC_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME, display_name,
                                           LOC_GIS_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, local_sort_name,
                                           LOC_GIS_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, local_compare_name,
                                           LOC_GIS_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, english_compare_name,
                                           -1);

        g_free (display_name);
        g_free (local_sort_name);
        g_free (local_compare_name);
        g_free (english_compare_name);
        break;

    case GWEATHER_LOCATION_WEATHER_STATION:
        break;

    case GWEATHER_LOCATION_NAMED_TIMEZONE:
        if (show_named_timezones) {
            gtk_list_store_insert_with_values (store, NULL, -1,
                                               LOC_GIS_LOCATION_ENTRY_COL_LOCATION, loc,
                                               LOC_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME, gweather_location_get_name (loc),
                                               LOC_GIS_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, gweather_location_get_sort_name (loc),
                                               LOC_GIS_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, gweather_location_get_sort_name (loc),
                                               LOC_GIS_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, gweather_location_get_english_sort_name (loc),
                                               -1);
        }
        break;

    case GWEATHER_LOCATION_DETACHED:
        g_assert_not_reached ();
    }
}

static char *
find_word (const char *full_name, const char *word, int word_len,
           gboolean whole_word, gboolean is_first_word)
{
    char *p;

    if (word == NULL || *word == '\0')
        return NULL;

    p = (char *)full_name - 1;
    while ((p = strchr (p + 1, *word))) {
        if (strncmp (p, word, word_len) != 0)
            continue;

        if (p > (char *)full_name) {
            char *prev = g_utf8_prev_char (p);

            /* Make sure p points to the start of a word */
            if (g_unichar_isalpha (g_utf8_get_char (prev)))
                continue;

            /* If we're matching the first word of the key, it has to
             * match the first word of the location, city, state, or
             * country, or the abbreviation (in parenthesis).
             * Eg, it either matches the start of the string
             * (which we already know it doesn't at this point) or
             * it is preceded by the string ", " or "(" (which isn't actually
             * a perfect test. FIXME)
             */
            if (is_first_word) {
                if (prev == (char *)full_name ||
                    ((prev - 1 <= full_name && strncmp (prev - 1, ", ", 2) != 0)
                      && *prev != '('))
                    continue;
            }
        }

        if (whole_word && g_unichar_isalpha (g_utf8_get_char (p + word_len)))
            continue;

        return p;
    }
    return NULL;
}

static gboolean
match_compare_name (const char *key, const char *name)
{
    gboolean is_first_word = TRUE;
    size_t len;

    /* Ignore whitespace before the string */
    key += strspn (key, " ");

    /* All but the last word in KEY must match a full word from NAME,
     * in order (but possibly skipping some words from NAME).
     */
    len = strcspn (key, " ");
    while (key[len]) {
        name = find_word (name, key, len, TRUE, is_first_word);
        if (!name)
            return FALSE;

        key += len;
        while (*key && !g_unichar_isalpha (g_utf8_get_char (key)))
            key = g_utf8_next_char (key);
        while (*name && !g_unichar_isalpha (g_utf8_get_char (name)))
            name = g_utf8_next_char (name);

        len = strcspn (key, " ");
        is_first_word = FALSE;
    }

    /* The last word in KEY must match a prefix of a following word in NAME */
    if (len == 0) {
        return TRUE;
    } else {
        // if we get here, key[len] == 0, so...
        g_assert (len == strlen(key));
        return find_word (name, key, len, FALSE, is_first_word) != NULL;
    }
}

static gboolean
model_row_matches (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   const char   *key)
{
    char *local_compare_name = NULL;
    char *english_compare_name = NULL;
    gboolean match;

    gtk_tree_model_get (model, iter,
                        LOC_GIS_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, &local_compare_name,
                        LOC_GIS_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, &english_compare_name,
                        -1);

    match = match_compare_name (key, local_compare_name) ||
            match_compare_name (key, english_compare_name) ||
            g_ascii_strcasecmp (key, english_compare_name) == 0;

    g_free (local_compare_name);
    g_free (english_compare_name);

    return match;
}

static void
commit_selected_iter (GisLocationEntry *entry,
                      GtkTreeModel     *model,
                      GtkTreeIter      *iter)
{
    GisLocationEntryPrivate *priv = entry->priv;

    if (model != priv->model) {
        GeocodePlace *place;
        char *display_name;
        GeocodeLocation *loc;
        GWeatherLocation *location;
        GWeatherLocation *scope = NULL;
        const char *country_code;

        gtk_tree_model_get (model, iter,
                            PLACE_GIS_LOCATION_ENTRY_COL_PLACE, &place,
                            PLACE_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME, &display_name,
                            -1);

        country_code = geocode_place_get_country_code (place);
        if (country_code != NULL &&
            gweather_location_get_level (priv->top) == GWEATHER_LOCATION_WORLD)
            scope = gweather_location_find_by_country_code (priv->top, country_code);
        if (!scope)
            scope = priv->top;

        loc = geocode_place_get_location (place);
        location = gweather_location_new_detached (display_name,
                                                   NULL,
                                                   geocode_location_get_latitude (loc),
                                                   geocode_location_get_longitude (loc));

        set_location_internal (entry, model, NULL, location);

        g_object_unref (place);
        g_object_unref (location);
        g_free (display_name);
    } else {
        set_location_internal (entry, model, iter, NULL);
    }
}

static gboolean
active_model_contains_exact_text (GisLocationEntry *entry,
                                  const char       *text,
                                  GtkTreeIter      *iter_out)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    int display_col;

    model = entry->priv->active_model;
    if (model == NULL || text == NULL || *text == '\0')
        return FALSE;

    display_col = entry->priv->active_model_is_geocode
        ? PLACE_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME
        : LOC_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME;

    if (!gtk_tree_model_get_iter_first (model, &iter))
        return FALSE;

    do {
        g_autofree char *display_name = NULL;

        gtk_tree_model_get (model, &iter, display_col, &display_name, -1);

        if (display_name && g_strcmp0 (display_name, text) == 0) {
            if (iter_out != NULL)
                *iter_out = iter;
            return TRUE;
        }
    } while (gtk_tree_model_iter_next (model, &iter));

    return FALSE;
}

static gboolean
maybe_commit_completion (GisLocationEntry *entry)
{
    GtkTreeIter iter;
    const char *text;

    text = gtk_editable_get_text (GTK_EDITABLE (entry->priv->entry));

    if (!text || *text == '\0')
        return FALSE;

    if (!active_model_contains_exact_text (entry, text, &iter))
        return FALSE;

    commit_selected_iter (entry, entry->priv->active_model, &iter);
    return TRUE;
}

static gboolean
on_entry_key_pressed (GtkEventControllerKey *controller,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
                      gpointer               user_data)
{
    GisLocationEntry *entry = GIS_LOCATION_ENTRY (user_data);

    if (keyval != GDK_KEY_Return &&
        keyval != GDK_KEY_KP_Enter &&
        keyval != GDK_KEY_Tab &&
        keyval != GDK_KEY_ISO_Left_Tab)
        return FALSE;

    maybe_commit_completion (entry);

    return FALSE;
}

static void
on_entry_focus_leave (GtkEventControllerFocus *controller,
                gpointer                 user_data)
{
    GisLocationEntry *entry = GIS_LOCATION_ENTRY (user_data);

    maybe_commit_completion (entry);
}

static void
fill_store (gpointer data, gpointer user_data)
{
    GeocodePlace *place = GEOCODE_PLACE (data);
    GeocodeLocation *loc = geocode_place_get_location (place);
    const char *display_name;
    char *normalized;
    char *compare_name;

    display_name = geocode_location_get_description (loc);
    normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_ALL);
    compare_name = g_utf8_casefold (normalized, -1);

    g_debug ("Adding geocode match %s", display_name);

    gtk_list_store_insert_with_values (user_data, NULL, -1,
                                       PLACE_GIS_LOCATION_ENTRY_COL_PLACE, place,
                                       PLACE_GIS_LOCATION_ENTRY_COL_DISPLAY_NAME, display_name,
                                       PLACE_GIS_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, compare_name,
                                       PLACE_GIS_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, compare_name,
                                       -1);

    g_free (normalized);
    g_free (compare_name);
}

static void
_got_places (GObject      *source_object,
             GAsyncResult *result,
             gpointer      user_data)
{
    g_autolist(GeocodePlace) places = NULL;
    GisLocationEntry *self = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GtkListStore) store = NULL;

    places = geocode_forward_search_finish (GEOCODE_FORWARD (source_object), result, &error);
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_debug ("Geocode query cancelled");
        return;
    }

    if (error != NULL) {
        g_warning ("Geocode query failed: %s", error->message);
        restore_local_model (self);
        g_clear_object (&self->priv->cancellable);
        return;
    }

    self = GIS_LOCATION_ENTRY (user_data);

    if (places == NULL) {
        g_debug ("No geocode results, restoring default model");
        restore_local_model (self);
    } else {
        store = gtk_list_store_new (4,
                                    G_TYPE_STRING,
                                    GEOCODE_TYPE_PLACE,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                                 tree_compare_local_name, NULL, NULL);
        g_list_foreach (places, fill_store, store);
        set_active_model (self, GTK_TREE_MODEL (store), TRUE);
    }

    g_clear_object (&self->priv->cancellable);
}

static void
trigger_geocode_search (GisLocationEntry *entry)
{
    const gchar *key = gtk_editable_get_text (GTK_EDITABLE (entry->priv->entry));
    GeocodeForward *forward;

    if (key == NULL || *key == '\0')
        return;

    if (entry->priv->cancellable) {
        g_cancellable_cancel (entry->priv->cancellable);
        g_clear_object (&entry->priv->cancellable);
    }

    entry->priv->cancellable = g_cancellable_new ();

    g_debug ("Starting geocode query for %s", key);
    forward = geocode_forward_new_for_string (key);
    geocode_forward_search_async (forward, entry->priv->cancellable, _got_places, entry);
    g_object_unref (forward);
}

/**
 * gis_location_entry_new:
 * @top: the top-level location for the entry.
 *
 * Creates a new #GisLocationEntry.
 *
 * @top will normally be the location returned from
 * gweather_location_get_world(), but you can create an entry that
 * only accepts a smaller set of locations if you want.
 *
 * Return value: the new #GisLocationEntry
 **/
GtkWidget *
gis_location_entry_new (GWeatherLocation *top)
{
    return g_object_new (GIS_TYPE_LOCATION_ENTRY,
                         "top", top,
                         NULL);
}

GtkWidget *
gis_location_entry_get_entry (GisLocationEntry *self)
{
    return self->priv->entry;
}
