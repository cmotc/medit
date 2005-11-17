/*
 *   mooutils-misc.h
 *
 *   Copyright (C) 2004-2005 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#ifndef __MOO_UTILS_MISC_H__
#define __MOO_UTILS_MISC_H__

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS


gboolean    moo_save_file_utf8              (const char *name,
                                             const char *text,
                                             gssize      len,
                                             GError    **error);
int         moo_unlink                      (const char *filename);
int         moo_mkdir                       (const char *path);
gboolean    moo_rmdir                       (const char *path,
                                             gboolean    recursive);

gboolean    moo_open_url                    (const char *url);
gboolean    moo_open_email                  (const char *address,
                                             const char *subject,
                                             const char *body);


gboolean    moo_window_is_hidden            (GtkWindow  *window);
void        moo_window_present              (GtkWindow  *window);
GtkWindow  *moo_get_top_window              (GSList     *windows);
GtkWindow  *moo_get_toplevel_window         (void);

gboolean    moo_window_set_icon_from_stock  (GtkWindow  *window,
                                             const char *stock_id);


void        moo_log_window_show             (void);
void        moo_log_window_hide             (void);

void        moo_log_window_write            (const char *log_domain,
                                             GLogLevelFlags flags,
                                             const char *message);


void        moo_set_log_func_window         (gboolean        show);
void        moo_set_log_func_file           (const char     *log_file);
void        moo_set_log_func                (gboolean        show_log);
#ifdef __WIN32__
void        moo_show_fatal_error            (const char     *logdomain,
                                             const char     *logmsg);
#endif /* __WIN32__ */

void        moo_segfault                    (void);


G_END_DECLS

#endif /* __MOO_UTILS_MISC_H__ */
