/*
 *   mooedit-fileops.h
 *
 *   Copyright (C) 2004-2008 by Yevgen Muntyan <muntyan@tamu.edu>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with medit.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MOOEDIT_COMPILATION
#error "This file may not be included"
#endif

#ifndef MOO_EDIT_FILE_OPS_H
#define MOO_EDIT_FILE_OPS_H

#include "mooedit/mooedit.h"

G_BEGIN_DECLS


const char *_moo_get_default_encodings (void);

typedef enum {
    MOO_EDIT_SAVE_BACKUP = 1 << 0
} MooEditSaveFlags;

#define MOO_EDIT_FILE_ERROR (_moo_edit_file_error_quark ())
#define MOO_EDIT_FILE_ERROR_ENCODING 0

GQuark           _moo_edit_file_error_quark     (void) G_GNUC_CONST;

gboolean         _moo_edit_load_file            (MooEdit        *edit,
                                                 const char     *file,
                                                 const char     *encoding,
                                                 GError        **error);
gboolean         _moo_edit_reload_file          (MooEdit        *edit,
                                                 const char     *encoding,
                                                 GError        **error);
gboolean         _moo_edit_save_file            (MooEdit        *edit,
                                                 const char     *file,
                                                 const char     *encoding,
                                                 MooEditSaveFlags flags,
                                                 GError        **error);
gboolean         _moo_edit_save_file_copy       (MooEdit        *edit,
                                                 const char     *file,
                                                 const char     *encoding,
                                                 GError        **error);


G_END_DECLS

#endif /* MOO_EDIT_FILE_OPS_H */