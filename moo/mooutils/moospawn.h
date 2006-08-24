/*
 *   moospawn.h
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

#ifndef __MOO_SPAWN_H__
#define __MOO_SPAWN_H__

#include <glib-object.h>

G_BEGIN_DECLS


#define MOO_TYPE_CMD              (_moo_cmd_get_type ())
#define MOO_CMD(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_CMD, MooCmd))
#define MOO_CMD_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_CMD, MooCmdClass))
#define MOO_IS_CMD(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_CMD))
#define MOO_IS_CMD_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_CMD))
#define MOO_CMD_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_CMD, MooCmdClass))


typedef struct _MooCmd         MooCmd;
typedef struct _MooCmdPrivate  MooCmdPrivate;
typedef struct _MooCmdClass    MooCmdClass;

/* XXX BUFFERED */
typedef enum {
    MOO_CMD_COLLECT_STDOUT      = 1 << 0,
    MOO_CMD_COLLECT_STDERR      = 1 << 1,
    MOO_CMD_STDOUT_TO_PARENT    = 1 << 2,
    MOO_CMD_STDERR_TO_PARENT    = 1 << 3,
    MOO_CMD_UTF8_OUTPUT         = 1 << 4,
    MOO_CMD_OPEN_CONSOLE        = 1 << 5
} MooCmdFlags;

struct _MooCmd
{
    GObject base;
    MooCmdPrivate *priv;
};

struct _MooCmdClass
{
    GObjectClass base_class;

    /* action signal */
    gboolean (*abort)       (MooCmd     *cmd);

    gboolean (*cmd_exit)    (MooCmd     *cmd,
                             int         status);
    gboolean (*stdout_text) (MooCmd     *cmd,
                             const char *text);
    gboolean (*stderr_text) (MooCmd     *cmd,
                             const char *text);
};


GType       _moo_cmd_get_type       (void) G_GNUC_CONST;

gboolean    _moo_unix_spawn_async   (char      **argv,
                                     GSpawnFlags g_flags,
                                     GError    **error);

MooCmd     *_moo_cmd_new            (const char *working_dir,
                                     char      **argv,
                                     char      **envp,
                                     GSpawnFlags flags,
                                     MooCmdFlags cmd_flags,
                                     GSpawnChildSetupFunc child_setup,
                                     gpointer    user_data,
                                     GError    **error);

void        _moo_cmd_abort          (MooCmd     *cmd);


G_END_DECLS

#endif /* __MOO_SPAWN_H__ */