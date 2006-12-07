/*
 *   mooeditor-private.h
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

#ifndef MOOEDIT_COMPILATION
#error "This file may not be used directly"
#endif

#ifndef __MOO_EDITOR_PRIVATE_H__
#define __MOO_EDITOR_PRIVATE_H__

#include "mooedit/mooeditor.h"

G_BEGIN_DECLS


void             _moo_edit_window_insert_doc    (MooEditWindow  *window,
                                                 MooEdit        *doc,
                                                 int             position);
void             _moo_edit_window_remove_doc    (MooEditWindow  *window,
                                                 MooEdit        *doc);

void             _moo_editor_set_focused_doc    (MooEditor      *editor,
                                                 MooEdit        *doc);
void             _moo_editor_unset_focused_doc  (MooEditor      *editor,
                                                 MooEdit        *doc);

gpointer         _moo_editor_get_file_watch     (MooEditor      *editor);
void             _moo_editor_reload             (MooEditor      *editor,
                                                 MooEdit        *doc,
                                                 GError        **error);
gboolean         _moo_editor_save               (MooEditor      *editor,
                                                 MooEdit        *doc,
                                                 GError        **error);
gboolean         _moo_editor_save_as            (MooEditor      *editor,
                                                 MooEdit        *doc,
                                                 const char     *filename,
                                                 const char     *encoding,
                                                 GError        **error);
void             _moo_editor_post_message       (MooEditor      *editor,
                                                 GQuark          domain,
                                                 const char     *message);
void             _moo_editor_apply_prefs        (MooEditor      *editor);


G_END_DECLS

#endif /* __MOO_EDITOR_PRIVATE_H__ */