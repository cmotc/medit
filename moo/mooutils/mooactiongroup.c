/*
 *   mooactiongroup.c
 *
 *   Copyright (C) 2004-2006 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#include "mooutils/mooactiongroup.h"
#include "mooutils/moocompat.h"
#include <string.h>


struct _MooActionGroupPrivate
{
    GHashTable *actions; /* char* -> MooAction* */
    char       *name;
};


static void moo_action_group_class_init     (MooActionGroupClass    *klass);

static void moo_action_group_init           (MooActionGroup         *group);
static void moo_action_group_finalize       (GObject                *object);

static void moo_action_group_add_action_priv (MooActionGroup        *group,
                                              MooAction             *action);


enum {
    PROP_0
};

enum {
    LAST_SIGNAL
};


/* MOO_TYPE_ACTION_GROUP */
G_DEFINE_TYPE (MooActionGroup, moo_action_group, G_TYPE_OBJECT)


static void moo_action_group_class_init (MooActionGroupClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = moo_action_group_finalize;
}


static void moo_action_group_init (MooActionGroup *group)
{
    group->priv = g_new0 (MooActionGroupPrivate, 1);
    group->priv->actions =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free, g_object_unref);
    group->priv->name = NULL;
}


static void moo_action_group_finalize       (GObject      *object)
{
    MooActionGroup *group = MOO_ACTION_GROUP (object);
    g_hash_table_destroy (group->priv->actions);
    g_free (group->priv->name);
    g_free (group->priv);
    group->priv = NULL;
    G_OBJECT_CLASS (moo_action_group_parent_class)->finalize (object);
}


void
moo_action_group_add (MooActionGroup *group,
                      MooAction      *action)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group) && group->priv != NULL);
    g_return_if_fail (MOO_IS_ACTION (action));
    moo_action_group_add_action_priv (group, action);
}


MooActionGroup*
moo_action_group_new (const char *name)
{
    MooActionGroup *group;

    g_return_val_if_fail (name != NULL, NULL);
    group = g_object_new (MOO_TYPE_ACTION_GROUP, NULL);
    group->priv->name = g_strdup (name);

    return group;
}


MooAction*
moo_action_group_get_action (MooActionGroup *group,
                             const char     *action_id)
{
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group) && action_id != NULL, NULL);
    g_return_val_if_fail (group->priv != NULL, NULL);
    return g_hash_table_lookup (group->priv->actions, action_id);
}


typedef struct {
    MooActionGroupForeachFunc   func;
    gpointer                    data;
    gboolean                    stop;
    MooActionGroup             *group;
} ForeachData;

static void
foreach_func (G_GNUC_UNUSED const char *action_id,
              MooAction     *action,
              ForeachData   *data)
{
    if (!data->stop)
        data->stop = data->func (data->group, action, data->data);
}

void
moo_action_group_foreach (MooActionGroup             *group,
                          MooActionGroupForeachFunc   func,
                          gpointer                    data)
{
    ForeachData d = {func, data, FALSE, group};

    g_return_if_fail (MOO_IS_ACTION_GROUP (group) && group->priv != NULL);
    g_return_if_fail (func != NULL);

    g_hash_table_foreach (group->priv->actions,
                          (GHFunc) foreach_func, &d);
}


void
moo_action_group_set_name (MooActionGroup *group,
                           const char     *name)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group) && name != NULL);
    g_free (group->priv->name);
    group->priv->name = g_strdup (name);
}


const char*
moo_action_group_get_name (MooActionGroup *group)
{
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);
    return group->priv->name;
}


static void
moo_action_group_add_action_priv (MooActionGroup    *group,
                                  MooAction         *action)
{
    const char *id = moo_action_get_id (action);
    if (g_hash_table_lookup (group->priv->actions, id))
        g_warning ("action with id '%s' already exists in action group '%s'\n",
                   id, group->priv->name);

    g_hash_table_insert (group->priv->actions, g_strdup (id),
                         g_object_ref (action));
}


void
moo_action_group_remove_action (MooActionGroup *group,
                                const char     *action_id)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group));
    g_return_if_fail (action_id != NULL);

    g_hash_table_remove (group->priv->actions, action_id);
}


MooAction*
moo_action_group_add_action (MooActionGroup *group,
                             const char     *first_prop_name,
                             ...)
{
    MooAction *action;
    GObject *object;
    GType action_type = MOO_TYPE_ACTION;
    va_list var_args;

    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);

    va_start (var_args, first_prop_name);

    if (first_prop_name && (!strcmp (first_prop_name, "action-type::") ||
        !strcmp (first_prop_name, "action-type::")))
    {
        action_type = va_arg (var_args, GType);
        g_return_val_if_fail (g_type_is_a (action_type, MOO_TYPE_ACTION), NULL);
        first_prop_name = va_arg (var_args, char*);
    }

    object = g_object_new_valist (action_type, first_prop_name, var_args);

    va_end (var_args);

    g_return_val_if_fail (object != NULL, NULL);

    action = MOO_ACTION (object);
    moo_action_group_add (group, action);
    g_object_unref (action);

    return action;
}
