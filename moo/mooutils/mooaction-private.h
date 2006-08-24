/*
 *   mooaction-private.h
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

#ifndef __MOO_ACTION_PRIVATE_H__
#define __MOO_ACTION_PRIVATE_H__

#include <mooutils/mooaction.h>
#include <mooutils/mooclosure.h>
#include <gtk/gtkactiongroup.h>

G_BEGIN_DECLS


void        _moo_action_set_closure         (MooAction          *action,
                                             MooClosure         *closure);

typedef void (*MooToggleActionCallback)     (gpointer            data,
                                             gboolean            active);
void        _moo_toggle_action_set_callback (MooToggleAction    *action,
                                             MooToggleActionCallback callback,
                                             gpointer            data,
                                             gboolean            object);

void        _moo_sync_toggle_action         (GtkAction          *action,
                                             gpointer            master,
                                             const char         *prop,
                                             gboolean            invert);

gpointer    _moo_action_get_window          (gpointer            action);

/* defined in mooactionbase.c */
gboolean    _moo_action_get_dead            (gpointer            action);
gboolean    _moo_action_get_has_submenu     (gpointer            action);

const char *_moo_action_get_display_name    (gpointer            action);
GtkActionGroup *_moo_action_get_group       (gpointer            action);

void        _moo_action_set_no_accel        (gpointer            action,
                                             gboolean            no_accel);
gboolean    _moo_action_get_no_accel        (gpointer            action);
char       *_moo_action_make_accel_path     (gpointer            action);
void        _moo_action_set_accel_path      (gpointer            action,
                                             const char         *accel_path);
const char *_moo_action_get_accel_path      (gpointer            action);
const char *_moo_action_get_default_accel   (gpointer            action);
void        _moo_action_set_default_accel   (gpointer            action,
                                             const char         *accel);


G_END_DECLS

#endif /* __MOO_ACTION_PRIVATE_H__ */