/*
 *   moolangmgr-private.h
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
#error "This file may not be used"
#endif

#ifndef MOO_LANG_MGR_PRIVATE_H
#define MOO_LANG_MGR_PRIVATE_H

#include "mooedit/moolangmgr.h"
#include "mooedit/mooeditconfig.h"
#include "mooedit/gtksourceview/gtksourceview-api.h"

G_BEGIN_DECLS


#define MOO_LANG_MGR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_LANG_MGR, MooLangMgrClass))
#define MOO_IS_LANG_MGR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_LANG_MGR))
#define MOO_LANG_MGR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_LANG_MGR, MooLangMgrClass))

typedef struct _MooLangMgrClass MooLangMgrClass;

struct _MooLangMgr {
    GtkSourceLanguageManager base;

    GtkSourceStyleSchemeManager *style_mgr;

    GHashTable *langs;
    GHashTable *config;
    GHashTable *schemes;

    MooTextStyleScheme *active_scheme;

    gboolean got_langs;
    gboolean got_schemes;
    gboolean modified;
};

struct _MooLangMgrClass
{
    GtkSourceLanguageManagerClass base_class;
};


G_END_DECLS

#endif /* MOO_LANG_MGR_PRIVATE_H */
