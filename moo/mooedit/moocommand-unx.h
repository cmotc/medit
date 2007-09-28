/*
 *   moocommand-unx.h
 *
 *   Copyright (C) 2004-2007 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License version 2.1 as published by the Free Software Foundation.
 *
 *   See COPYING file that comes with this distribution.
 */

#ifndef MOO_COMMAND_UNX_H
#define MOO_COMMAND_UNX_H

#include <mooedit/moocommand.h>

G_BEGIN_DECLS


#define MOO_TYPE_COMMAND_UNX            (_moo_command_unx_get_type ())
#define MOO_COMMAND_UNX(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_COMMAND_UNX, MooCommandUnx))
#define MOO_COMMAND_UNX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_COMMAND_UNX, MooCommandUnxClass))
#define MOO_IS_COMMAND_UNX(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_COMMAND_UNX))
#define MOO_IS_COMMAND_UNX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_COMMAND_UNX))
#define MOO_COMMAND_UNX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_COMMAND_UNX, MooCommandUnxClass))

typedef struct _MooCommandUnx        MooCommandUnx;
typedef struct _MooCommandUnxPrivate MooCommandUnxPrivate;
typedef struct _MooCommandUnxClass   MooCommandUnxClass;

struct _MooCommandUnx {
    MooCommand base;
    MooCommandUnxPrivate *priv;
};

struct _MooCommandUnxClass {
    MooCommandClass base_class;
};


GType _moo_command_unx_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* MOO_COMMAND_UNX_H */