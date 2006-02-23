/*
 *   mooappinput.h
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

#ifndef __MOO_APP_INPUT__
#define __MOO_APP_INPUT__

#ifdef __WIN32__
#include <windows.h>
#endif /* __WIN32__ */

#include <glib.h>

G_BEGIN_DECLS


typedef enum
{
    MOO_APP_CMD_ZERO = 0,
    MOO_APP_CMD_PYTHON_STRING,
    MOO_APP_CMD_PYTHON_FILE,
    MOO_APP_CMD_OPEN_FILE,
    MOO_APP_CMD_QUIT,
    MOO_APP_CMD_DIE,
    MOO_APP_CMD_PRESENT,
    MOO_APP_CMD_LAST
} MooAppCmdCode;


#if defined(WANT_MOO_APP_CMD_STRINGS) || defined(WANT_MOO_APP_CMD_CHARS)

#define CMD_ZERO            "\0"
#define CMD_PYTHON_STRING   "s"
#define CMD_PYTHON_FILE     "p"
#define CMD_OPEN_FILE       "f"
#define CMD_QUIT            "q"
#define CMD_DIE             "d"
#define CMD_PRESENT         "r"

#endif

#ifdef WANT_MOO_APP_CMD_CHARS

static const char *moo_app_cmd_chars =
    CMD_ZERO
    CMD_PYTHON_STRING
    CMD_PYTHON_FILE
    CMD_OPEN_FILE
    CMD_QUIT
    CMD_DIE
    CMD_PRESENT
;

#endif /* WANT_MOO_APP_CMD_CHARS */


typedef struct _MooAppInput MooAppInput;

struct _MooAppInput
{
    guint        ref_count;

    int          pipe;
    char        *pipe_basename;
    char        *pipe_name;
    GIOChannel  *io;
    guint        io_watch;
    GByteArray  *buffer;
    gboolean     ready;

#ifdef __WIN32__
    HANDLE       listener;
#endif /* __WIN32__ */
};


MooAppInput *moo_app_input_new          (const char     *pipe_basename);

MooAppInput *moo_app_input_ref          (MooAppInput    *ch);
void         moo_app_input_unref        (MooAppInput    *ch);

gboolean     moo_app_input_start        (MooAppInput    *ch);
void         moo_app_input_shutdown     (MooAppInput    *ch);
gboolean     moo_app_input_ready        (MooAppInput    *ch);

const char  *moo_app_input_get_name     (MooAppInput    *ch);

gboolean     _moo_app_input_send_msg    (const char     *pipe_basename,
                                         const char     *data,
                                         int             len);


G_END_DECLS

#endif /* __MOO_APP_INPUT__ */
