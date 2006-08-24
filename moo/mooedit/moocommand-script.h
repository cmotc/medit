/*
 *   moocommand-script.h
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

#ifndef __MOO_COMMAND_SCRIPT_H__
#define __MOO_COMMAND_SCRIPT_H__

#include <mooedit/moocommand.h>

G_BEGIN_DECLS


#define MOO_TYPE_COMMAND_SCRIPT                    (moo_command_script_get_type ())
#define MOO_COMMAND_SCRIPT(object)                 (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_COMMAND_SCRIPT, MooCommandScript))
#define MOO_COMMAND_SCRIPT_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_COMMAND_SCRIPT, MooCommandScriptClass))
#define MOO_IS_COMMAND_SCRIPT(object)              (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_COMMAND_SCRIPT))
#define MOO_IS_COMMAND_SCRIPT_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_COMMAND_SCRIPT))
#define MOO_COMMAND_SCRIPT_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_COMMAND_SCRIPT, MooCommandScriptClass))

typedef struct _MooCommandScript        MooCommandScript;
typedef struct _MooCommandScriptPrivate MooCommandScriptPrivate;
typedef struct _MooCommandScriptClass   MooCommandScriptClass;

struct _MooCommandScript {
    MooCommand base;
    MooCommandScriptPrivate *priv;
};

struct _MooCommandScriptClass {
    MooCommandClass base_class;
};


GType       moo_command_script_get_type (void) G_GNUC_CONST;

MooCommand *moo_command_script_new      (const char         *script,
                                         MooCommandOptions   options);


G_END_DECLS

#endif /* __MOO_COMMAND_SCRIPT_H__ */