/*
 *   moofileview-impl.h
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

#include <moofileview/moofileview.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkmenu.h>

#ifndef __MOO_FILE_VIEW_IMPL_H__
#define __MOO_FILE_VIEW_IMPL_H__


typedef struct _MooFileViewPrivate  MooFileViewPrivate;

struct _MooFileView
{
    GtkVBox vbox;
    GtkWidget *toolbar;
    MooFileViewPrivate *priv;
};

struct _MooFileViewClass
{
    GtkVBoxClass vbox_class;

    gboolean    (*chdir)            (MooFileView    *fileview,
                                     const char     *dir,
                                     GError        **error);
    void        (*populate_popup)   (MooFileView    *fileview,
                                     GList          *selected,
                                     GtkMenu        *menu);
    void        (*activate)         (MooFileView    *fileview,
                                     const char     *path);

    gboolean    (*drop)             (MooFileView    *fileview,
                                     const char     *path,
                                     GtkWidget      *widget,
                                     GdkDragContext *context,
                                     int             x,
                                     int             y,
                                     guint           time);
    gboolean    (*drop_data_received) (MooFileView  *fileview,
                                     const char     *path,
                                     GtkWidget      *widget,
                                     GdkDragContext *context,
                                     int             x,
                                     int             y,
                                     GtkSelectionData *data,
                                     guint           info,
                                     guint           time);
};


void        _moo_file_view_select_name                  (MooFileView    *fileview,
                                                         const char     *name);

void        _moo_file_view_add_target                   (MooFileView    *fileview,
                                                         GdkAtom         target,
                                                         guint           flags,
                                                         guint           info);

void        _moo_file_view_drag_finish                  (MooFileView    *fileview,
                                                         GdkDragContext *context,
                                                         gboolean        success,
                                                         gboolean        delete_data,
                                                         guint           time);


/* returns list of absolute paths, must be freed together with content */
GList      *_moo_file_view_get_filenames                (MooFileView    *fileview);


#endif /* __MOO_FILE_VIEW_IMPL_H__ */