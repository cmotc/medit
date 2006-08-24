/*
 *   mooactionbase.h
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

#ifndef __MOO_ACTION_BASE_H__
#define __MOO_ACTION_BASE_H__

#include <glib-object.h>

G_BEGIN_DECLS


#define MOO_TYPE_ACTION_BASE                (moo_action_base_get_type ())
#define MOO_ACTION_BASE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOO_TYPE_ACTION_BASE, MooActionBase))
#define MOO_IS_ACTION_BASE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOO_TYPE_ACTION_BASE))
#define MOO_ACTION_BASE_GET_CLASS(inst)     (G_TYPE_INSTANCE_GET_INTERFACE ((inst), MOO_TYPE_ACTION_BASE, MooActionBaseClass))

typedef struct _MooActionBase       MooActionBase;
typedef struct _MooActionBaseClass  MooActionBaseClass;

struct _MooActionBaseClass {
    GTypeInterface parent;
};


GType   moo_action_base_get_type    (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __MOO_ACTION_BASE_H__ */