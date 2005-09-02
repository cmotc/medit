/*
 *   moorecentmgr.c
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

#define MOOEDIT_COMPILATION
#include "mooedit/mooeditprefs.h"
#include "mooedit/mooedit-private.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moomarshals.h"
#include <string.h>

#define PREFS_RECENT            "recent_files"
#define PREFS_ENTRY             "entry"
#define PREFS_SHOW_FULL_NAME    PREFS_RECENT "/show_full_name"
#define NUM_RECENT_FILES        5


typedef struct {
    MooEditFileInfo *info;
} RecentEntry;

typedef struct {
    GtkMenuItem *parent;
    GtkMenu     *menu;
    gpointer     data;
    GSList      *items;     /* GtkMenuItem*, synchronized with recent files list */
} RecentMenu;

struct _MooRecentMgrPrivate {
    GSList  *files;  /* RecentEntry* */
    GSList  *menus;  /* RecentMenu* */
    gboolean prefs_loaded;
    GtkTooltips *tooltips;
    gboolean display_full_name;
};


static void moo_recent_mgr_class_init    (MooRecentMgrClass    *klass);
static void moo_recent_mgr_init          (MooRecentMgr         *mgr);
static void moo_recent_mgr_finalize      (GObject                *object);

static RecentEntry  *recent_entry_new       (const MooEditFileInfo  *info);
static void          recent_entry_free      (RecentEntry            *entry);

static RecentEntry  *recent_list_find       (MooRecentMgr           *mgr,
                                             MooEditFileInfo        *info);
static void          recent_list_delete_tail(MooRecentMgr           *mgr);

static GtkWidget    *recent_menu_item_new   (MooRecentMgr           *mgr,
                                             const MooEditFileInfo  *info,
                                             gpointer                data);

static void          mgr_add_recent         (MooRecentMgr           *mgr,
                                             const MooEditFileInfo  *info);
static void          mgr_reorder_recent     (MooRecentMgr           *mgr,
                                             RecentEntry            *top);
static void          mgr_load_recent        (MooRecentMgr           *mgr);
static void          mgr_save_recent        (MooRecentMgr           *mgr);

static void          save_recent            (guint                   n,
                                             RecentEntry            *entry);
static RecentEntry  *load_recent            (guint                   n);

static void          menu_item_activated    (GtkMenuItem            *item,
                                             MooRecentMgr           *mgr);
static void          menu_destroyed         (GtkMenuItem            *parent,
                                             MooRecentMgr           *mgr);


/* MOO_TYPE_RECENT_MGR */
G_DEFINE_TYPE (MooRecentMgr, moo_recent_mgr, G_TYPE_OBJECT)

enum {
    OPEN_RECENT,
    NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

static void
moo_recent_mgr_class_init (MooRecentMgrClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = moo_recent_mgr_finalize;

    signals[OPEN_RECENT] =
            g_signal_new ("open-recent",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooRecentMgrClass, open_recent),
                          NULL, NULL,
                          _moo_marshal_VOID__BOXED_POINTER_OBJECT,
                          G_TYPE_NONE, 3,
                          MOO_TYPE_EDIT_FILE_INFO | G_SIGNAL_TYPE_STATIC_SCOPE,
                          G_TYPE_POINTER,
                          GTK_TYPE_MENU_ITEM);
}


static void
moo_recent_mgr_init (MooRecentMgr *mgr)
{
    mgr->priv = g_new0 (MooRecentMgrPrivate, 1);

    mgr->priv->tooltips = gtk_tooltips_new ();
    gtk_object_sink (GTK_OBJECT (g_object_ref (mgr->priv->tooltips)));
    mgr->priv->display_full_name = FALSE;
}


static void
moo_recent_mgr_finalize (GObject *object)
{
    GSList *l;
    MooRecentMgr *mgr = MOO_RECENT_MGR (object);

    for (l = mgr->priv->files; l != NULL; l = l->next)
        recent_entry_free (l->data);
    g_slist_free (mgr->priv->files);
    mgr->priv->files = NULL;
    g_object_unref (mgr->priv->tooltips);
    mgr->priv->tooltips = NULL;

    for (l = mgr->priv->menus; l != NULL; l = l->next)
    {
        RecentMenu *menu = l->data;
        GSList *i;

        g_signal_handlers_disconnect_by_func (menu->parent,
                                              (gpointer) menu_destroyed,
                                              mgr);

        for (i = menu->items; i != NULL; i = i->next)
        {
            if (GTK_IS_MENU_ITEM (i->data))
                g_signal_handlers_disconnect_by_func (i->data,
                    (gpointer) menu_item_activated, mgr);
        }

        g_slist_free (menu->items);
        menu->items = NULL;

        g_free (menu);
    }

    g_free (mgr->priv);

    G_OBJECT_CLASS (moo_recent_mgr_parent_class)->finalize (object);
}


MooRecentMgr*
moo_recent_mgr_new (void)
{
    return MOO_RECENT_MGR (g_object_new (MOO_TYPE_RECENT_MGR, NULL));
}


typedef gboolean (*ListFoundFunc) (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   gpointer      data);

typedef struct {
    GtkTreeIter     *iter;
    ListFoundFunc    func;
    gpointer         user_data;
    gboolean         found;
} ListStoreFindData;


void
moo_recent_mgr_add_recent (MooRecentMgr    *mgr,
                           MooEditFileInfo *info)
{
    RecentEntry *entry;

    g_return_if_fail (MOO_IS_RECENT_MGR (mgr));
    g_return_if_fail (info != NULL);
    g_return_if_fail (info->filename != NULL);

    mgr_load_recent (mgr);

    entry = recent_list_find (mgr, info);

    if (!entry)
        mgr_add_recent (mgr, info);
    else
        mgr_reorder_recent (mgr, entry);

    mgr_save_recent (mgr);
}


static void
mgr_add_recent (MooRecentMgr          *mgr,
                const MooEditFileInfo *info)
{
    GSList *l;
    RecentEntry *entry = recent_entry_new (info);
    gboolean delete_last = FALSE;

    if (g_slist_length (mgr->priv->files) == NUM_RECENT_FILES)
        delete_last = TRUE;

    mgr->priv->files =
            g_slist_prepend (mgr->priv->files, entry);

    if (delete_last)
        recent_list_delete_tail (mgr);

    for (l = mgr->priv->menus; l != NULL; l = l->next)
    {
        RecentMenu *menu;
        GtkWidget *item;

        menu = l->data;

        item = recent_menu_item_new (mgr, info, menu->data);
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu->menu), item);
        menu->items = g_slist_prepend (menu->items, item);

        if (delete_last)
        {
            GSList *tail;
            tail = g_slist_last (menu->items);
            g_signal_handlers_disconnect_by_func (tail->data,
                    (gpointer) menu_item_activated, mgr);
            gtk_container_remove (GTK_CONTAINER (menu->menu), tail->data);
            menu->items = g_slist_delete_link (menu->items, tail);
        }
    }
}


static void
mgr_reorder_recent (MooRecentMgr *mgr,
                    RecentEntry  *top)
{
    int index;
    GSList *l;

    index = g_slist_index (mgr->priv->files, top);
    g_return_if_fail (index >= 0);

    if (index == 0)
        return;

    mgr->priv->files =
            g_slist_delete_link (mgr->priv->files,
                                 g_slist_nth (mgr->priv->files, index));
    mgr->priv->files =
            g_slist_prepend (mgr->priv->files, top);

    for (l = mgr->priv->menus; l != NULL; l = l->next)
    {
        RecentMenu *menu;
        GtkWidget *item;

        menu = l->data;
        item = g_slist_nth_data (menu->items, index);
        g_assert (item != NULL);

        g_object_ref (item);

        gtk_container_remove (GTK_CONTAINER (menu->menu), item);
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu->menu), item);

        g_object_unref (item);

        menu->items = g_slist_delete_link (menu->items,
                                           g_slist_nth (menu->items, index));
        menu->items = g_slist_prepend (menu->items, item);
    }
}


static void
mgr_save_recent (MooRecentMgr *mgr)
{
    guint i, num;

    num = g_slist_length (mgr->priv->files);

    for (i = 0; i < num; ++i)
        save_recent (i, g_slist_nth_data (mgr->priv->files, i));
    for (; i < NUM_RECENT_FILES; ++i)
        save_recent (i, NULL);
}


static void
mgr_load_recent (MooRecentMgr *mgr)
{
    guint i;

    if (mgr->priv->prefs_loaded)
        return;

    mgr->priv->prefs_loaded = TRUE;

    mgr->priv->display_full_name =
            moo_prefs_get_bool (moo_edit_setting (PREFS_SHOW_FULL_NAME));

    for (i = 0; i < NUM_RECENT_FILES; ++i)
    {
        RecentEntry *entry = load_recent (i);
        if (entry)
            mgr->priv->files =
                    g_slist_append (mgr->priv->files, entry);
    }
}


static gboolean
file_cmp (RecentEntry     *entry,
          MooEditFileInfo *info)
{
    return strcmp (entry->info->filename, info->filename);
}

static RecentEntry*
recent_list_find (MooRecentMgr    *mgr,
                  MooEditFileInfo *info)
{
    GSList *l = g_slist_find_custom (mgr->priv->files,
                                     info, (GCompareFunc) file_cmp);
    if (l)
        return l->data;
    else
        return NULL;
}


static void
recent_list_delete_tail (MooRecentMgr *mgr)
{
    GSList *l = g_slist_last (mgr->priv->files);
    g_return_if_fail (l != NULL);
    recent_entry_free (l->data);
    g_slist_delete_link (mgr->priv->files, l);
}


static GtkWidget*
recent_menu_item_new (MooRecentMgr          *mgr,
                      const MooEditFileInfo *info,
                      gpointer               data)
{
    GtkWidget *item;

    if (mgr->priv->display_full_name)
    {
        char *display_name = _moo_edit_filename_to_utf8 (info->filename);
        item = gtk_menu_item_new_with_label (display_name);
        g_free (display_name);
    }
    else
    {
        char *basename = g_path_get_basename (info->filename);
        char *display_basename = _moo_edit_filename_to_utf8 (basename);
        char *display_fullname = _moo_edit_filename_to_utf8 (info->filename);

        item = gtk_menu_item_new_with_label (display_basename);
        gtk_tooltips_set_tip (mgr->priv->tooltips, item,
                              display_fullname, NULL);

        g_free (display_basename);
        g_free (display_fullname);
        g_free (basename);
    }

    gtk_widget_show (item);

    g_signal_connect (item, "activate",
                      G_CALLBACK (menu_item_activated), mgr);
    g_object_set_data_full (G_OBJECT (item), "moo-edit-file-mgr-recent-file",
                            moo_edit_file_info_copy (info),
                            (GDestroyNotify) moo_edit_file_info_free);
    g_object_set_data (G_OBJECT (item),
                       "moo-edit-file-mgr-recent-file-data", data);
    return item;
}


static void
menu_item_activated (GtkMenuItem  *item,
                     MooRecentMgr *mgr)
{
    MooEditFileInfo *info = g_object_get_data (G_OBJECT (item),
                                               "moo-edit-file-mgr-recent-file");
    gpointer data = g_object_get_data (G_OBJECT (item),
                                       "moo-edit-file-mgr-recent-file-data");
    g_return_if_fail (info != NULL);
    g_signal_emit (mgr, signals[OPEN_RECENT], 0, info, data, item);
}


static RecentEntry*
recent_entry_new (const MooEditFileInfo  *info)
{
    RecentEntry *entry = g_new0 (RecentEntry, 1);
    entry->info = moo_edit_file_info_copy (info);
    return entry;
}


static void
recent_entry_free (RecentEntry        *entry)
{
    if (entry)
    {
        moo_edit_file_info_free (entry->info);
        g_free (entry);
    }
}


#define RECENT_PREFIX MOO_EDIT_PREFS_PREFIX "/" PREFS_RECENT "/" PREFS_ENTRY
#define RECENT_FILENAME RECENT_PREFIX "%d/filename"
#define RECENT_ENCODING RECENT_PREFIX "%d/encoding"


static void
save_recent (guint        n,
             RecentEntry *entry)
{
    char *key;
    const char *filename, *encoding;

    filename = entry ? entry->info->filename : NULL;
    encoding = entry ? entry->info->encoding : NULL;

    key = g_strdup_printf (RECENT_FILENAME, n);
    moo_prefs_set_string (key, filename);
    g_free (key);

    key = g_strdup_printf (RECENT_ENCODING, n);
    moo_prefs_set_string (key, encoding);
    g_free (key);
}


static RecentEntry*
load_recent (guint n)
{
    RecentEntry *entry = NULL;
    char *filename, *encoding;
    char *key;

    key = g_strdup_printf (RECENT_FILENAME, n);
    filename = g_strdup (moo_prefs_get_string (key));
    g_free (key);

    key = g_strdup_printf (RECENT_ENCODING, n);
    encoding = g_strdup (moo_prefs_get_string (key));
    g_free (key);

    if (filename)
    {
        MooEditFileInfo *info = moo_edit_file_info_new (filename, encoding);
        entry = recent_entry_new (info);
        moo_edit_file_info_free (info);
    }

    g_free (filename);
    g_free (encoding);

    return entry;
}

#undef RECENT_PREFIX
#undef RECENT_FILENAME
#undef RECENT_ENCODING


GtkMenuItem*
moo_recent_mgr_create_menu (MooRecentMgr *mgr,
                            gpointer      data)
{
    RecentMenu *menu;
    GSList *l;

    mgr_load_recent (mgr);

    menu = g_new0 (RecentMenu, 1);
    menu->data = data;

    menu->parent = GTK_MENU_ITEM (gtk_image_menu_item_new_with_label ("Open Recent"));
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu->parent),
                                   gtk_image_new_from_stock (GTK_STOCK_OPEN,
                                           GTK_ICON_SIZE_MENU));

    menu->menu = GTK_MENU (gtk_menu_new ());
    gtk_menu_item_set_submenu (menu->parent, GTK_WIDGET (menu->menu));

    gtk_widget_show (GTK_WIDGET (menu->parent));
    gtk_widget_show (GTK_WIDGET (menu->menu));

    g_signal_connect (menu->parent, "destroy",
                      G_CALLBACK (menu_destroyed), mgr);

    for (l = mgr->priv->files; l != NULL; l = l->next)
    {
        RecentEntry *entry = l->data;
        GtkWidget *item = recent_menu_item_new (mgr, entry->info, data);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu), item);
        menu->items = g_slist_append (menu->items, item);
    }

    mgr->priv->menus = g_slist_prepend (mgr->priv->menus, menu);
    return menu->parent;
}


static gboolean
menu_cmp (RecentMenu  *menu,
          GtkMenuItem *parent)
{
    return menu->parent != parent;
}

static void
menu_destroyed (GtkMenuItem  *parent,
                MooRecentMgr *mgr)
{
    RecentMenu *menu;
    GSList *l = g_slist_find_custom (mgr->priv->menus,
                                     parent,
                                     (GCompareFunc) menu_cmp);

    g_return_if_fail (l != NULL);

    menu = l->data;

    g_slist_free (menu->items);
    g_free (menu);

    mgr->priv->menus =
            g_slist_remove (mgr->priv->menus, menu);
}
