/*
 *   mooapp-private.h
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

#ifndef MOO_APP_COMPILATION
#error "This file may not be used"
#endif

#ifndef __MOO_APP_PRIVATE_H__
#define __MOO_APP_PRIVATE_H__

#include <mooapp/mooapp.h>

G_BEGIN_DECLS


void             _moo_app_exec_cmd              (MooApp     *app,
                                                 char        cmd,
                                                 const char *data,
                                                 guint       len);
GtkWidget       *_moo_app_create_prefs_dialog   (MooApp     *app);


G_END_DECLS

#endif /* __MOO_APP_PRIVATE_H__ */