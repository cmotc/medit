/*
 *   mooactioncollection.h
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

#ifndef __MOO_ACTION_COLLECTION_H__
#define __MOO_ACTION_COLLECTION_H__

#include <gtk/gtkactiongroup.h>

G_BEGIN_DECLS


#define MOO_TYPE_ACTION_COLLECTION              (moo_action_collection_get_type ())
#define MOO_ACTION_COLLECTION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_ACTION_COLLECTION, MooActionCollection))
#define MOO_ACTION_COLLECTION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_ACTION_COLLECTION, MooActionCollectionClass))
#define MOO_IS_ACTION_COLLECTION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_ACTION_COLLECTION))
#define MOO_IS_ACTION_COLLECTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_ACTION_COLLECTION))
#define MOO_ACTION_COLLECTION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_ACTION_COLLECTION, MooActionCollectionClass))

typedef struct _MooActionCollection        MooActionCollection;
typedef struct _MooActionCollectionPrivate MooActionCollectionPrivate;
typedef struct _MooActionCollectionClass   MooActionCollectionClass;

struct _MooActionCollection {
    GObject base;
    MooActionCollectionPrivate *priv;
};

struct _MooActionCollectionClass {
    GObjectClass base_class;
};


GType                moo_action_collection_get_type (void) G_GNUC_CONST;

MooActionCollection *moo_action_collection_new              (const char             *name,
                                                             const char             *display_name);

const char          *moo_action_collection_get_name         (MooActionCollection    *coll);

void                 moo_action_collection_set_display_name (MooActionCollection    *coll,
                                                             const char             *name);
const char          *moo_action_collection_get_display_name (MooActionCollection    *coll);

GtkActionGroup      *moo_action_collection_add_group        (MooActionCollection    *coll,
                                                             const char             *name,
                                                             const char             *display_name);
GtkActionGroup      *moo_action_collection_get_group        (MooActionCollection    *coll,
                                                             const char             *name);
void                 moo_action_collection_remove_group     (MooActionCollection    *coll,
                                                             GtkActionGroup         *group);
const GSList        *moo_action_collection_get_groups       (MooActionCollection    *coll);

GtkAction           *moo_action_collection_get_action       (MooActionCollection    *coll,
                                                             const char             *name);
void                 moo_action_collection_remove_action    (MooActionCollection    *coll,
                                                             GtkAction              *action);
GList               *moo_action_collection_list_actions     (MooActionCollection    *coll);

void                 _moo_action_collection_set_window      (MooActionCollection    *coll,
                                                             gpointer                window);
gpointer             _moo_action_collection_get_window      (MooActionCollection    *coll);


G_END_DECLS

#endif /* __MOO_ACTION_COLLECTION_H__ */
