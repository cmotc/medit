/*
 *   moofileview.c
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

#ifdef GTK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#include <gtk/gtktoolbar.h>
#define GTK_DISABLE_DEPRECATED
#endif

#define MOO_FILE_VIEW_COMPILATION
#include "moofileview/moofileview-dialogs.h"
#include "moofileview/moobookmarkmgr.h"
#include "moofileview/moofilesystem.h"
#include "moofileview/moofoldermodel.h"
#include "moofileview/moofileentry.h"
#include "moofileview/mooiconview.h"
#include "moofileview/moofileview-aux.h"
#include "moofileview/moofileview-private.h"
#include "moofileview/moofileview-ui.h"
#include "moofileview/mootreeview.h"
#include "moofileview/moobookmarkview.h"
#include "moofileview/moofileview-tools.h"
#include "mooutils/mooutils-gobject.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-misc.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moofiltermgr.h"
#include "mooutils/moouixml.h"
#include "mooutils/moospawn.h"
#include "mooutils/moostock.h"
#include "mooutils/mooactionfactory.h"
#include "mooutils/mooaction-private.h"
#include "mooutils/moomarshals.h"
#include "mooutils/mooi18n.h"
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>


#ifndef __WIN32__
#define TYPEAHEAD_CASE_SENSITIVE_DEFAULT    FALSE
#define SORT_CASE_SENSITIVE_DEFAULT         MOO_FOLDER_MODEL_SORT_CASE_SENSITIVE_DEFAULT
#define COMPLETION_CASE_SENSITIVE_DEFAULT   TRUE
#else /* __WIN32__ */
#define TYPEAHEAD_CASE_SENSITIVE_DEFAULT    FALSE
#define SORT_CASE_SENSITIVE_DEFAULT         FALSE
#define COMPLETION_CASE_SENSITIVE_DEFAULT   FALSE
#endif /* __WIN32__ */


enum {
    TREEVIEW_PAGE = 0,
    ICONVIEW_PAGE = 1,
    BOOKMARK_PAGE = 2
};


enum {
    TARGET_URI_LIST = 1,
    TARGET_TEXT = 2
};

static GdkAtom moo_file_view_clipboard;

static GtkTargetEntry source_targets[] = {
    {(char*) "text/uri-list", 0, TARGET_URI_LIST}
};

static GtkTargetEntry dest_targets[] = {
    {(char*) "text/uri-list", 0, TARGET_URI_LIST}
};


typedef struct _History History;
typedef struct _Typeahead Typeahead;
typedef struct _Clipboard Clipboard;

struct _Clipboard {
    MooFolder *folder;
    GList *files;
    gboolean cut;
};

struct _MooFileViewPrivate {
    GtkTreeModel    *model;
    GtkTreeModel    *filter_model;
    MooFolder       *current_dir;
    MooFileSystem   *file_system;

    Clipboard       *clipboard;

    guint            select_file_idle;
    char            *select_file;

    GtkIconSize      icon_size;
    GtkNotebook     *notebook;
    MooFileViewType  view_type;
    MooFileViewType  file_view_type;

    MooTreeView     *view;
    GtkTreeView     *treeview;
    GtkTreeViewColumn *tree_name_column;
    MooIconView     *iconview;
    MooBookmarkView *bkview;

    GtkMenu         *bookmarks_menu;
    MooBookmarkMgr  *bookmark_mgr;

    char            *home_dir;

    gboolean         show_hidden_files;
    gboolean         show_two_dots;
    History         *history;
    GString         *temp_visible;  /* temporary visible name, for interactive search */

    MooFilterMgr    *filter_mgr;
    GtkToggleButton *filter_button;
    MooCombo        *filter_combo;
    GtkEntry        *filter_entry;
    GtkFileFilter   *current_filter;
    gboolean         use_current_filter;

    GtkEntry        *entry;
    int              entry_state;   /* it can be one of three: nothing, typeahead, or completion,
                                       depending on text entered into the entry */
    Typeahead       *typeahead;
    gboolean         typeahead_case_sensitive;
    gboolean         sort_case_sensitive;
    gboolean         completion_case_sensitive;

    MooActionCollection *actions;
    MooUIXML        *ui_xml;
    gboolean         has_selection;

    GtkTargetList   *targets;
    GSList          *drag_dest_widgets;

    struct {
        GtkWidget *button;
        gboolean highlight;
        GtkTreeRowReference *row;
        guint timeout;
        int x;
        int y;
        gboolean blink_clear;
        guint n_blinks;
    } drop_to;
};


static void         moo_file_view_finalize      (GObject        *object);
static void         moo_file_view_set_property  (GObject        *object,
                                                 guint           prop_id,
                                                 const GValue   *value,
                                                 GParamSpec     *pspec);
static void         moo_file_view_get_property  (GObject        *object,
                                                 guint           prop_id,
                                                 GValue         *value,
                                                 GParamSpec     *pspec);

static void         moo_file_view_hide          (GtkWidget      *widget);
static gboolean     moo_file_view_key_press     (MooFileView    *fileview,
                                                 GtkWidget      *widget,
                                                 GdkEventKey    *event);
static gboolean     moo_file_view_popup_menu    (GtkWidget      *widget);
static void         moo_file_view_unrealize     (GtkWidget      *widget);

static void         moo_file_view_set_filter_mgr    (MooFileView    *fileview,
                                                     MooFilterMgr   *mgr);
static void         moo_file_view_set_bookmark_mgr  (MooFileView    *fileview,
                                                     MooBookmarkMgr *mgr);

static void         moo_file_view_set_current_dir (MooFileView  *fileview,
                                                 MooFolder      *folder);
static gboolean     moo_file_view_chdir_real    (MooFileView    *fileview,
                                                 const char     *dir,
                                                 GError        **error);
static void         moo_file_view_reload        (MooFileView    *fileview);

static void         moo_file_view_go_up     (MooFileView    *fileview);
static void         moo_file_view_go_home   (MooFileView    *fileview);
static void         moo_file_view_go_back   (MooFileView    *fileview);
static void         moo_file_view_go_forward(MooFileView    *fileview);
static void         toggle_show_hidden      (MooFileView    *fileview);

static void         bookmark_activate       (MooBookmarkMgr *mgr,
                                             MooBookmark    *bookmark,
                                             MooFileView    *activated,
                                             MooFileView    *fileview);
static void         bookmark_activated      (MooFileView    *fileview,
                                             MooBookmark    *bookmark);

static void         history_init            (MooFileView    *fileview);
static void         history_free            (MooFileView    *fileview);
static void         history_add             (MooFileView    *fileview,
                                             const char     *dirname);
static void         history_clear           (MooFileView    *fileview);

static gboolean     filter_visible_func     (GtkTreeModel       *model,
                                             GtkTreeIter        *iter,
                                             MooFileView        *fileview);

static gboolean     moo_file_view_check_visible (MooFileView        *fileview,
                                                 MooFile            *file,
                                                 gboolean            ignore_hidden,
                                                 gboolean            ignore_two_dots);

static void icon_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
static void name_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
#ifdef USE_SIZE_AND_STUFF
static void date_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
static void size_data_func  (GObject            *column_or_iconview,
                             GtkCellRenderer    *cell,
                             GtkTreeModel       *model,
                             GtkTreeIter        *iter,
                             MooFileView        *fileview);
#endif

static void     init_gui                    (MooFileView    *fileview);
static void     focus_to_file_view          (MooFileView    *fileview);
static void     focus_to_filter_entry       (MooFileView    *fileview);
static GtkWidget *create_toolbar            (MooFileView    *fileview);
static GtkWidget *create_notebook           (MooFileView    *fileview);

static GtkWidget *create_filter_combo       (MooFileView    *fileview);
static void     init_filter_combo           (MooFileView    *fileview);
static void     filter_button_toggled       (MooFileView    *fileview);
static void     filter_combo_changed        (MooFileView    *fileview);
static void     filter_entry_activate       (MooFileView    *fileview);
static void     fileview_set_filter         (MooFileView    *fileview,
                                             GtkFileFilter  *filter);
static void     fileview_set_use_filter     (MooFileView    *fileview,
                                             gboolean        use,
                                             gboolean        block_signals);

static GtkWidget *create_treeview           (MooFileView    *fileview);
static GtkWidget *create_iconview           (MooFileView    *fileview);
static GtkWidget *create_bookmark_view      (MooFileView    *fileview);

static GtkWidget *get_view_widget           (MooFileView    *fileview);
static void     file_view_move_selection    (MooFileView    *fileview,
                                             GtkTreeIter    *filter_iter);
static GList   *file_view_get_selected_files(MooFileView    *fileview);

static void     path_entry_init             (MooFileView    *fileview);
static void     path_entry_deinit           (MooFileView    *fileview);
static void     path_entry_set_text         (MooFileView    *fileview,
                                             const char     *text);
static void     stop_path_entry             (MooFileView    *fileview,
                                             gboolean        focus_file_list);
static void     path_entry_delete_to_cursor (MooFileView *fileview);
static void     file_view_activate_filename (MooFileView *fileview,
                                             const char     *display_name);

static void     file_added                  (MooFileView    *fileview);

static void     file_view_delete_selected   (MooFileView    *fileview);
static void     file_view_create_folder     (MooFileView    *fileview);
static void     file_view_properties_dialog (MooFileView    *fileview);

static void     view_files                  (MooFileView    *fileview);
static void     view_bookmarks              (MooFileView    *fileview);
static void     add_bookmark                (MooFileView    *fileview);
static void     edit_bookmarks              (MooFileView    *fileview);

/* Dnd */
static void     icon_drag_begin             (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             MooIconView    *iconview);
static void     icon_drag_data_get          (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             GtkSelectionData *data,
                                             guint           info,
                                             guint           time,
                                             MooIconView    *iconview);
static void     icon_drag_end               (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             MooIconView    *iconview);

static void     drag_data_received          (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             GtkSelectionData *data,
                                             guint           info,
                                             guint           time,
                                             MooIconView    *view);
static gboolean drag_drop                   (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time,
                                             MooIconView    *view);
static void     drag_leave                  (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             guint           time,
                                             GtkWidget      *view);
static gboolean drag_motion                 (GtkWidget      *view,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time,
                                             MooFileView    *fileview);

static gboolean moo_file_view_drop          (MooFileView    *fileview,
                                             const char     *path,
                                             GtkWidget      *widget,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time);
static gboolean moo_file_view_drop_data_received
                                            (MooFileView    *fileview,
                                             const char     *path,
                                             GtkWidget      *widget,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             GtkSelectionData *data,
                                             guint           info,
                                             guint           time);
static void     cancel_drop_open            (MooFileView    *fileview);

static void     button_drag_leave           (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             guint           time,
                                             GtkWidget      *button);
static gboolean button_drag_motion          (MooFileView    *fileview,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time,
                                             GtkWidget      *button);
static void     sync_dest_targets           (MooFileView    *fileview);

static void     moo_file_view_drop_uris     (MooFileView    *fileview,
                                             char          **uris,
                                             const char     *destdir,
                                             GtkWidget      *widget,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time);
static gboolean moo_file_view_drop_text     (MooFileView    *fileview,
                                             const char     *text,
                                             const char     *destdir,
                                             GtkWidget      *widget,
                                             GdkDragContext *context,
                                             int             x,
                                             int             y,
                                             guint           time,
                                             gboolean       *delete);


static void     file_list_selection_changed (MooFileView    *file_view,
                                             MooTreeView    *view);
static gboolean file_list_button_press      (MooFileView    *fileview,
                                             GtkWidget      *widget,
                                             GdkEventButton *event,
                                             MooTreeView    *view);
static void     file_list_row_activated     (MooFileView    *fileview,
                                             GtkTreePath    *filter_path);

static Clipboard *clipboard_new             (MooFolder      *folder,
                                             GList          *files,
                                             gboolean        cut);
static void     clipboard_free              (Clipboard      *cb);
static void     file_view_clear_clipboard   (MooFileView    *fileview);

static void     copy_files                  (MooFileView    *fileview,
                                             GList          *filenames,
                                             const char     *destdir);
static void     move_files                  (MooFileView    *fileview,
                                             GList          *filenames,
                                             const char     *destdir);
static void     link_files                  (MooFileView    *fileview,
                                             GList          *filenames,
                                             const char     *destdir);

static void     file_view_cut_clipboard     (MooFileView    *fileview);
static void     file_view_copy_clipboard    (MooFileView    *fileview);
static void     file_view_paste_clipboard   (MooFileView    *fileview);


/* MOO_TYPE_FILE_VIEW */
G_DEFINE_TYPE (MooFileView, moo_file_view, GTK_TYPE_VBOX)

enum {
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_HOME_DIRECTORY,
    PROP_FILTER_MGR,
    PROP_BOOKMARK_MGR,
    PROP_SORT_CASE_SENSITIVE,
    PROP_TYPEAHEAD_CASE_SENSITIVE,
    PROP_COMPLETION_CASE_SENSITIVE,
    PROP_SHOW_HIDDEN_FILES,
    PROP_SHOW_PARENT_FOLDER,
    /* Aux properties */
    PROP_HAS_SELECTION,
    PROP_CAN_GO_BACK,
    PROP_CAN_GO_FORWARD
};

enum {
    CHDIR,
    ACTIVATE,
    POPULATE_POPUP,
    GO_UP,
    GO_BACK,
    GO_FORWARD,
    GO_HOME,
    FOCUS_TO_FILTER_ENTRY,
    FOCUS_TO_FILE_VIEW,
    TOGGLE_SHOW_HIDDEN,
    DELETE_TO_CURSOR,
    PROPERTIES_DIALOG,
    DELETE_SELECTED,
    CREATE_FOLDER,
    DROP,
    DROP_DATA_RECEIVED,
    CUT_CLIPBOARD,
    COPY_CLIPBOARD,
    PASTE_CLIPBOARD,
    RELOAD,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
moo_file_view_class_init (MooFileViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkBindingSet *binding_set;

    moo_file_view_clipboard = gdk_atom_intern ("MOO_FILE_VIEW_CLIPBOARD", FALSE);

    gobject_class->finalize = moo_file_view_finalize;
    gobject_class->set_property = moo_file_view_set_property;
    gobject_class->get_property = moo_file_view_get_property;

    widget_class->hide = moo_file_view_hide;
    widget_class->popup_menu = moo_file_view_popup_menu;
    widget_class->unrealize = moo_file_view_unrealize;

    klass->chdir = moo_file_view_chdir_real;
    klass->drop = moo_file_view_drop;
    klass->drop_data_received = moo_file_view_drop_data_received;

    g_object_class_install_property (gobject_class,
                                     PROP_HAS_SELECTION,
                                     g_param_spec_boolean ("has-selection",
                                             "has-selection", "has-selection",
                                             FALSE, G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_CAN_GO_BACK,
                                     g_param_spec_boolean ("can-go-back",
                                             "can-go-back", "can-go-back",
                                             FALSE, G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_CAN_GO_FORWARD,
                                     g_param_spec_boolean ("can-go-forward",
                                             "can-go-forward", "can-go-forward",
                                             FALSE, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_CURRENT_DIRECTORY,
                                     g_param_spec_string ("current-directory",
                                             "current-directory",
                                             "current-directory",
                                             NULL,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_HOME_DIRECTORY,
                                     g_param_spec_string ("home-directory",
                                             "home-directory",
                                             "home-directory",
#ifndef __WIN32__
                                             g_get_home_dir (),
#else
                                             "C:\\",
#endif
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_FILTER_MGR,
                                     g_param_spec_object ("filter-mgr",
                                             "filter-mgr",
                                             "filter-mgr",
                                             MOO_TYPE_FILTER_MGR,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_BOOKMARK_MGR,
                                     g_param_spec_object ("bookmark-mgr",
                                             "bookmark-mgr",
                                             "bookmark-mgr",
                                             MOO_TYPE_BOOKMARK_MGR,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TYPEAHEAD_CASE_SENSITIVE,
                                     g_param_spec_boolean ("typeahead-case-sensitive",
                                             "typeahead-case-sensitive",
                                             "typeahead-case-sensitive",
                                             TYPEAHEAD_CASE_SENSITIVE_DEFAULT,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SORT_CASE_SENSITIVE,
                                     g_param_spec_boolean ("sort-case-sensitive",
                                             "sort-case-sensitive",
                                             "sort-case-sensitive",
                                             SORT_CASE_SENSITIVE_DEFAULT,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_COMPLETION_CASE_SENSITIVE,
                                     g_param_spec_boolean ("completion-case-sensitive",
                                             "completion-case-sensitive",
                                             "completion-case-sensitive",
                                             COMPLETION_CASE_SENSITIVE_DEFAULT,
                                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_HIDDEN_FILES,
                                     g_param_spec_boolean ("show-hidden-files",
                                             "show-hidden-files",
                                             "show-hidden-files",
                                             FALSE,
                                             G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_PARENT_FOLDER,
                                     g_param_spec_boolean ("show-parent-folder",
                                             "show-parent-folder",
                                             "show-parent-folder",
                                             FALSE,
                                             G_PARAM_READWRITE));

    signals[CHDIR] =
            g_signal_new ("chdir",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                          G_STRUCT_OFFSET (MooFileViewClass, chdir),
                          NULL, NULL,
                          _moo_marshal_BOOLEAN__STRING_POINTER,
                          G_TYPE_BOOLEAN, 2,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                          G_TYPE_POINTER);

    signals[ACTIVATE] =
            g_signal_new ("activate",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                          G_STRUCT_OFFSET (MooFileViewClass, activate),
                          NULL, NULL,
                          _moo_marshal_VOID__STRING,
                          G_TYPE_NONE, 1,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

    signals[POPULATE_POPUP] =
            g_signal_new ("populate-popup",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_FIRST,
                          G_STRUCT_OFFSET (MooFileViewClass, populate_popup),
                          NULL, NULL,
                          _moo_marshal_VOID__POINTER_OBJECT,
                          G_TYPE_NONE, 2,
                          G_TYPE_POINTER,
                          GTK_TYPE_MENU);

    signals[GO_UP] =
            _moo_signal_new_cb("go-up",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_up),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[GO_FORWARD] =
            _moo_signal_new_cb("go-forward",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_forward),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[GO_BACK] =
            _moo_signal_new_cb("go-back",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_back),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[GO_HOME] =
            _moo_signal_new_cb("go-home",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_go_home),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[FOCUS_TO_FILTER_ENTRY] =
            _moo_signal_new_cb("focus-to-filter-entry",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (focus_to_filter_entry),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[FOCUS_TO_FILE_VIEW] =
            _moo_signal_new_cb("focus-to-file-view",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (focus_to_file_view),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[TOGGLE_SHOW_HIDDEN] =
            _moo_signal_new_cb("toggle-show-hidden",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (toggle_show_hidden),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[DELETE_TO_CURSOR] =
            _moo_signal_new_cb("delete-to-cursor",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (path_entry_delete_to_cursor),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[DELETE_SELECTED] =
            _moo_signal_new_cb("delete-selected",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_delete_selected),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[CREATE_FOLDER] =
            _moo_signal_new_cb("create-folder",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_create_folder),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[PROPERTIES_DIALOG] =
             _moo_signal_new_cb("properties-dialog",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_properties_dialog),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[RELOAD] =
            _moo_signal_new_cb("reload",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (moo_file_view_reload),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[DROP] =
            g_signal_new ("drop",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooFileViewClass, drop),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOLEAN__STRING_OBJECT_OBJECT_INT_INT_UINT,
                          G_TYPE_BOOLEAN, 6,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                          GTK_TYPE_WIDGET,
                          GDK_TYPE_DRAG_CONTEXT,
                          G_TYPE_INT,
                          G_TYPE_INT,
                          G_TYPE_UINT);

    signals[DROP_DATA_RECEIVED] =
            g_signal_new ("drop-data-received",
                          G_OBJECT_CLASS_TYPE (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (MooFileViewClass, drop_data_received),
                          g_signal_accumulator_true_handled, NULL,
                          _moo_marshal_BOOLEAN__STRING_OBJECT_OBJECT_INT_INT_POINTER_UINT_UINT,
                          G_TYPE_BOOLEAN, 8,
                          G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                          GTK_TYPE_WIDGET,
                          GDK_TYPE_DRAG_CONTEXT,
                          G_TYPE_INT,
                          G_TYPE_INT,
                          G_TYPE_POINTER,
                          G_TYPE_UINT,
                          G_TYPE_UINT);

    signals[CUT_CLIPBOARD] =
            _moo_signal_new_cb("cut-clipboard",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_cut_clipboard),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[COPY_CLIPBOARD] =
            _moo_signal_new_cb("copy-clipboard",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_copy_clipboard),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    signals[PASTE_CLIPBOARD] =
            _moo_signal_new_cb("paste-clipboard",
                               G_OBJECT_CLASS_TYPE (klass),
                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                               G_CALLBACK (file_view_paste_clipboard),
                               NULL, NULL,
                               _moo_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    binding_set = gtk_binding_set_by_class (klass);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_u, GDK_CONTROL_MASK,
                                  "delete-to-cursor", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Return, GDK_MOD1_MASK,
                                  "properties-dialog", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Delete, GDK_MOD1_MASK,
                                  "delete-selected", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Up, GDK_MOD1_MASK,
                                  "go-up", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Up, GDK_MOD1_MASK,
                                  "go-up", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Left, GDK_MOD1_MASK,
                                  "go-back", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Left, GDK_MOD1_MASK,
                                  "go-back", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Right, GDK_MOD1_MASK,
                                  "go-forward", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Right, GDK_MOD1_MASK,
                                  "go-forward", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Home, GDK_MOD1_MASK,
                                  "go-home", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KP_Home, GDK_MOD1_MASK,
                                  "go-home", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_f, GDK_MOD1_MASK | GDK_SHIFT_MASK,
                                  "focus-to-filter-entry", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_b, GDK_MOD1_MASK | GDK_SHIFT_MASK,
                                  "focus-to-file-view", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_h, GDK_MOD1_MASK | GDK_SHIFT_MASK,
                                  "toggle-show-hidden", 0);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_c, GDK_CONTROL_MASK,
                                  "copy-clipboard", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Insert, GDK_CONTROL_MASK,
                                  "copy-clipboard", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_x, GDK_CONTROL_MASK,
                                  "cut-clipboard", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Delete, GDK_SHIFT_MASK,
                                  "cut-clipboard", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_v, GDK_CONTROL_MASK,
                                  "paste-clipboard", 0);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_Insert, GDK_SHIFT_MASK,
                                  "paste-clipboard", 0);
}


static void
moo_file_view_init (MooFileView *fileview)
{
    fileview->priv = g_new0 (MooFileViewPrivate, 1);
    fileview->priv->show_hidden_files = FALSE;
    fileview->priv->file_view_type = fileview->priv->view_type = MOO_FILE_VIEW_ICON;
    fileview->priv->use_current_filter = FALSE;
    fileview->priv->icon_size = GTK_ICON_SIZE_MENU;

    fileview->priv->typeahead_case_sensitive = TYPEAHEAD_CASE_SENSITIVE_DEFAULT;
    fileview->priv->sort_case_sensitive = SORT_CASE_SENSITIVE_DEFAULT;
    fileview->priv->completion_case_sensitive = COMPLETION_CASE_SENSITIVE_DEFAULT;

    history_init (fileview);

    fileview->priv->model = g_object_new (MOO_TYPE_FOLDER_MODEL,
                                          "sort-case-sensitive",
                                          fileview->priv->sort_case_sensitive,
                                          NULL);
    g_signal_connect_swapped (fileview->priv->model, "row-inserted",
                              G_CALLBACK (file_added), fileview);
    fileview->priv->filter_model =
            _moo_folder_filter_new (MOO_FOLDER_MODEL (fileview->priv->model));
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (fileview->priv->filter_model),
                                            (GtkTreeModelFilterVisibleFunc) filter_visible_func,
                                            fileview, NULL);

    fileview->priv->file_system = _moo_file_system_create ();

    fileview->priv->targets = gtk_target_list_new (dest_targets, G_N_ELEMENTS (dest_targets));
    gtk_target_list_add_text_targets (fileview->priv->targets, TARGET_TEXT);

    init_gui (fileview);
    path_entry_init (fileview);

    _moo_file_view_tools_load (fileview);
}


static void
moo_file_view_finalize (GObject *object)
{
    MooFileView *fileview = MOO_FILE_VIEW (object);

    path_entry_deinit (fileview);

    if (fileview->priv->select_file_idle)
        g_source_remove (fileview->priv->select_file_idle);
    g_free (fileview->priv->select_file);

    g_object_unref (fileview->priv->model);
    g_object_unref (fileview->priv->filter_model);
    history_free (fileview);

    if (fileview->priv->bookmark_mgr)
    {
        g_signal_handlers_disconnect_by_func (fileview->priv->bookmark_mgr,
                                              (gpointer) bookmark_activate,
                                              fileview);
        _moo_bookmark_mgr_remove_user (fileview->priv->bookmark_mgr,
                                       fileview);
        g_object_unref (fileview->priv->bookmark_mgr);
    }

    if (fileview->priv->filter_mgr)
        g_object_unref (fileview->priv->filter_mgr);
    if (fileview->priv->current_filter)
        g_object_unref (fileview->priv->current_filter);

    if (fileview->priv->temp_visible)
        g_string_free (fileview->priv->temp_visible, TRUE);

    if (fileview->priv->current_dir)
        g_object_unref (fileview->priv->current_dir);
    g_object_unref (fileview->priv->file_system);

    g_free (fileview->priv->home_dir);

    g_object_unref (fileview->priv->actions);
    g_object_unref (fileview->priv->ui_xml);

    gtk_target_list_unref (fileview->priv->targets);
    g_slist_free (fileview->priv->drag_dest_widgets);

    g_free (fileview->priv);
    fileview->priv = NULL;

    G_OBJECT_CLASS (moo_file_view_parent_class)->finalize (object);
}


static void
moo_file_view_hide (GtkWidget *widget)
{
    cancel_drop_open (MOO_FILE_VIEW (widget));
    GTK_WIDGET_CLASS(moo_file_view_parent_class)->hide (widget);
}


GtkWidget *
_moo_file_view_new (void)
{
    return GTK_WIDGET (g_object_new (MOO_TYPE_FILE_VIEW, NULL));
}


static void
moo_file_view_set_current_dir (MooFileView  *fileview,
                               MooFolder    *folder)
{
    GtkTreeIter filter_iter;
    char *path;

    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));
    g_return_if_fail (!folder || MOO_IS_FOLDER (folder));

    if (folder == fileview->priv->current_dir)
        return;

    cancel_drop_open (fileview);

    if (fileview->priv->temp_visible)
    {
        g_string_free (fileview->priv->temp_visible, TRUE);
        fileview->priv->temp_visible = NULL;
    }

    if (!folder)
    {
        if (fileview->priv->current_dir)
        {
            g_object_unref (fileview->priv->current_dir);
            fileview->priv->current_dir = NULL;
            _moo_folder_model_set_folder (MOO_FOLDER_MODEL (fileview->priv->model),
                                          NULL);
        }

        history_clear (fileview);
        path_entry_set_text (fileview, "");
        g_object_set (MOO_FILE_ENTRY (fileview->priv->entry),
                      "current-dir", NULL, NULL);
        g_object_notify (G_OBJECT (fileview), "current-directory");
        return;
    }

    if (fileview->priv->current_dir)
        g_object_unref (fileview->priv->current_dir);

    fileview->priv->current_dir = g_object_ref (folder);
    _moo_folder_model_set_folder (MOO_FOLDER_MODEL (fileview->priv->model),
                                  folder);

    if (gtk_tree_model_get_iter_first (fileview->priv->filter_model, &filter_iter))
        file_view_move_selection (fileview, &filter_iter);

    if (gtk_widget_is_focus (GTK_WIDGET (fileview->priv->entry)))
        focus_to_file_view (fileview);

    path = g_filename_display_name (_moo_folder_get_path (folder));
    path_entry_set_text (fileview, path);
    g_free (path);

    g_object_set (MOO_FILE_ENTRY(fileview->priv->entry)->completion,
                  "current-dir", _moo_folder_get_path (folder), NULL);

    history_add (fileview, _moo_folder_get_path (folder));
    g_object_notify (G_OBJECT (fileview), "current-directory");
}


static gboolean
moo_file_view_chdir_real (MooFileView    *fileview,
                          const char     *new_dir,
                          GError        **error)
{
    char *real_new_dir;
    MooFolder *folder;

    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), FALSE);

    if (!new_dir)
    {
        moo_file_view_set_current_dir (fileview, NULL);
        return TRUE;
    }

    if (g_path_is_absolute (new_dir) || !fileview->priv->current_dir)
    {
        real_new_dir = g_strdup (new_dir);
    }
    else
    {
        real_new_dir = g_build_filename (_moo_folder_get_path (fileview->priv->current_dir),
                                         new_dir, NULL);
    }

    folder = _moo_file_system_get_folder (fileview->priv->file_system,
                                          real_new_dir,
                                          MOO_FILE_ALL_FLAGS,
                                          error);
    g_free (real_new_dir);

    if (!folder)
        return FALSE;

    view_files (fileview);

    moo_file_view_set_current_dir (fileview, folder);
    g_object_unref (folder);
    return TRUE;
}


static void
moo_file_view_reload (MooFileView *fileview)
{
    if (fileview->priv->current_dir)
        _moo_folder_reload (fileview->priv->current_dir);
}


static void
init_actions (MooFileView *fileview)
{
    GtkAction *action;
    GtkActionGroup *group;

    fileview->priv->actions = moo_action_collection_new ("File Selector", _("File Selector"));
    fileview->priv->ui_xml = moo_ui_xml_new ();
    moo_ui_xml_add_ui_from_string (fileview->priv->ui_xml,
                                   MOO_FILE_VIEW_UI, -1);

    group = moo_action_collection_get_group (fileview->priv->actions, NULL);

    moo_action_group_add_action (group, "GoUp",
                                 "label", "Parent Folder",
                                 "tooltip", "Parent Folder",
                                 "stock-id", GTK_STOCK_GO_UP,
                                 "accel", "<alt>Up",
                                 "force-accel-label", TRUE,
                                 "closure-object", fileview,
                                 "closure-signal", "go-up",
                                 NULL);

    action = moo_action_group_add_action (group, "GoBack",
                                          "label", "Go Back",
                                          "tooltip", "Go Back",
                                          "stock-id", GTK_STOCK_GO_BACK,
                                          "accel", "<alt>Left",
                                          "force-accel-label", TRUE,
                                          "closure-object", fileview,
                                          "closure-signal", "go-back",
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "can-go-back", FALSE);

    action = moo_action_group_add_action (group, "GoForward",
                                          "label", "Go Forward",
                                          "tooltip", "Go Forward",
                                          "stock-id", GTK_STOCK_GO_FORWARD,
                                          "accel", "<alt>Right",
                                          "force-accel-label", TRUE,
                                          "closure-object", fileview,
                                          "closure-signal", "go-forward",
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "can-go-forward", FALSE);

    moo_action_group_add_action (group, "GoHome",
                                 "label", "Home Folder",
                                 "tooltip", "Home Folder",
                                 "stock-id", GTK_STOCK_HOME,
                                 "accel", "<alt>Home",
                                 "force-accel-label", TRUE,
                                 "closure-object", fileview,
                                 "closure-signal", "go-home",
                                 NULL);

    moo_action_group_add_action (group, "NewFolder",
                                 "label", "New Folder...",
                                 "tooltip", "New Folder...",
                                 "stock-id", GTK_STOCK_DIRECTORY,
                                 "closure-object", fileview,
                                 "closure-callback", file_view_create_folder,
                                 NULL);

    action = moo_action_group_add_action (group, "Delete",
                                          "label", "Delete...",
                                          "tooltip", "Delete...",
                                          "stock-id", GTK_STOCK_DELETE,
                                          "accel", "<alt>Delete",
                                          "force-accel-label", TRUE,
                                          "closure-object", fileview,
                                          "closure-callback", file_view_delete_selected,
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "has-selection", FALSE);

    action = moo_action_group_add_action (group, "ShowHiddenFiles",
                                          "action-type::", MOO_TYPE_TOGGLE_ACTION,
                                          "label", "Show Hidden Files",
                                          "tooltip", "Show Hidden Files",
                                          "accel", "<alt><shift>H",
                                          "force-accel-label", TRUE,
                                          NULL);
    _moo_sync_toggle_action (action, fileview, "show-hidden-files", FALSE);

    action = moo_action_group_add_action (group, "ShowParentFolder",
                                          "action-type::", MOO_TYPE_TOGGLE_ACTION,
                                          "label", "Show Parent Folder",
                                          "tooltip", "Show Parent Folder",
                                          NULL);
    _moo_sync_toggle_action (action, fileview, "show-parent-folder", FALSE);

    action = moo_action_group_add_action (group, "CaseSensitiveSort",
                                          "action-type::", MOO_TYPE_TOGGLE_ACTION,
                                          "label", "Case Sensitive Sort",
                                          "tooltip", "Case Sensitive Sort",
                                          NULL);
    _moo_sync_toggle_action (action, fileview, "sort-case-sensitive", FALSE);

    action = moo_action_group_add_action (group, "Properties",
                                          "label", "Properties",
                                          "tooltip", "Properties",
                                          "stock-id", GTK_STOCK_PROPERTIES,
                                          "accel", "<alt>Return",
                                          "force-accel-label", TRUE,
                                          "closure-object", fileview,
                                          "closure-callback", file_view_properties_dialog,
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "has-selection", FALSE);

    moo_action_group_add_action (group, "BookmarksMenu",
                                 "label", "Bookmarks",
                                 "tooltip", "Bookmarks",
                                 "stock-id", GTK_STOCK_ABOUT,
                                 "closure-object", fileview,
                                 "closure-callback", view_bookmarks,
                                 "has-submenu", TRUE,
                                 NULL);

    moo_action_group_add_action (group, "AddBookmark",
                                 "label", "Add Bookmark",
                                 "tooltip", "Add Bookmark",
                                 "stock-id", GTK_STOCK_ADD,
                                 "closure-object", fileview,
                                 "closure-callback", add_bookmark,
                                 NULL);

    moo_action_group_add_action (group, "EditBookmarks",
                                 "label", "Edit Bookmarks...",
                                 "tooltip", "Edit Bookmarks...",
                                 "stock-id", GTK_STOCK_EDIT,
                                 "closure-object", fileview,
                                 "closure-callback", edit_bookmarks,
                                 NULL);

    action = moo_action_group_add_action (group, "Cut",
                                          "label", "Cut",
                                          "tooltip", "Cut",
                                          "stock-id", GTK_STOCK_CUT,
                                          "accel", "<control>X",
                                          "force-accel-label", TRUE,
                                          "closure-object", fileview,
                                          "closure-callback", file_view_cut_clipboard,
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "has-selection", FALSE);

    action = moo_action_group_add_action (group, "Copy",
                                          "label", "Copy",
                                          "tooltip", "Copy",
                                          "stock-id", GTK_STOCK_COPY,
                                          "force-accel-label", TRUE,
                                          "accel", "<control>C",
                                          "closure-object", fileview,
                                          "closure-callback", file_view_copy_clipboard,
                                          NULL);
    moo_bind_bool_property (action, "sensitive", fileview, "has-selection", FALSE);

    action = moo_action_group_add_action (group, "Paste",
                                          "label", "Paste",
                                          "tooltip", "Paste",
                                          "stock-id", GTK_STOCK_PASTE,
                                          "accel", "<control>V",
                                          "force-accel-label", TRUE,
                                          "closure-object", fileview,
                                          "closure-callback", file_view_paste_clipboard,
                                          NULL);

#ifdef __WIN32__
    moo_action_group_add_action (group, "Reload",
                                 "label", "Reload",
                                 "tooltip", "Reload",
                                 "stock-id", GTK_STOCK_REFRESH,
                                 "closure-object", fileview,
                                 "closure-signal", "reload",
                                 NULL);
#endif
}


MooUIXML*
moo_file_view_get_ui_xml (MooFileView *fileview)
{
    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), NULL);
    return fileview->priv->ui_xml;
}


MooActionCollection *
moo_file_view_get_actions (MooFileView *fileview)
{
    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), NULL);
    return fileview->priv->actions;
}


static void
init_gui (MooFileView *fileview)
{
    GtkBox *box;
    GtkWidget *toolbar, *notebook, *filter_combo, *entry;

    init_actions (fileview);

    box = GTK_BOX (fileview);
    toolbar = create_toolbar (fileview);

    if (toolbar)
    {
        gtk_widget_show (toolbar);
        gtk_box_pack_start (box, toolbar, FALSE, FALSE, 0);
    }
    else
    {
        g_critical ("%s: oops", G_STRLOC);
    }

    entry = _moo_file_entry_new ();
    g_object_set_data (G_OBJECT (entry), "moo-file-view", fileview);
    gtk_widget_show (entry);
    gtk_box_pack_start (box, entry, FALSE, FALSE, 0);
    fileview->priv->entry = GTK_ENTRY (entry);

    notebook = create_notebook (fileview);
    gtk_widget_show (notebook);
    gtk_box_pack_start (box, notebook, TRUE, TRUE, 0);
    fileview->priv->notebook = GTK_NOTEBOOK (notebook);

    filter_combo = create_filter_combo (fileview);
    gtk_widget_show (filter_combo);
    gtk_box_pack_start (box, filter_combo, FALSE, FALSE, 0);

    switch (fileview->priv->view_type)
    {
        case MOO_FILE_VIEW_ICON:
            fileview->priv->file_view_type = MOO_FILE_VIEW_ICON;
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                           ICONVIEW_PAGE);
            _moo_tree_view_set_active (fileview->priv->view,
                                       GTK_WIDGET (fileview->priv->iconview));
            break;

        case MOO_FILE_VIEW_LIST:
            fileview->priv->file_view_type = MOO_FILE_VIEW_LIST;
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                           TREEVIEW_PAGE);
            _moo_tree_view_set_active (fileview->priv->view,
                                       GTK_WIDGET (fileview->priv->treeview));

        case MOO_FILE_VIEW_BOOKMARK:
            fileview->priv->file_view_type = MOO_FILE_VIEW_ICON;
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                           BOOKMARK_PAGE);
    }

    focus_to_file_view (fileview);
}


static void
focus_to_file_view (MooFileView *fileview)
{
    gtk_widget_grab_focus (get_view_widget (fileview));
}


static void
focus_to_filter_entry (MooFileView *fileview)
{
    gtk_widget_grab_focus (GTK_WIDGET(fileview->priv->filter_entry));
}


void
_moo_file_view_set_view_type (MooFileView    *fileview,
                              MooFileViewType type)
{
    if (fileview->priv->view_type == type)
        return;

    cancel_drop_open (fileview);

    switch (type)
    {
        case MOO_FILE_VIEW_LIST:
            fileview->priv->file_view_type = MOO_FILE_VIEW_LIST;
            _moo_tree_view_set_active (fileview->priv->view,
                                       GTK_WIDGET (fileview->priv->treeview));
            gtk_notebook_set_current_page (fileview->priv->notebook,
                                           TREEVIEW_PAGE);
            break;

        case MOO_FILE_VIEW_ICON:
            fileview->priv->file_view_type = MOO_FILE_VIEW_ICON;
            _moo_tree_view_set_active (fileview->priv->view,
                                       GTK_WIDGET (fileview->priv->iconview));
            gtk_notebook_set_current_page (fileview->priv->notebook,
                                           ICONVIEW_PAGE);
            break;

        case MOO_FILE_VIEW_BOOKMARK:
            fileview->priv->file_view_type = fileview->priv->view_type;
            gtk_notebook_set_current_page (fileview->priv->notebook,
                                           BOOKMARK_PAGE);
            break;
    }

    fileview->priv->view_type = type;
}


static void
setup_button_drag_dest (MooFileView *fileview,
                        const char  *xml_path,
                        const char  *sig_name)
{
    GtkWidget *button;

    button = moo_ui_xml_get_widget (fileview->priv->ui_xml,
                                    fileview->toolbar, xml_path);

    g_return_if_fail (button != NULL);

    g_object_set_data_full (G_OBJECT (button), "moo-fileview-signal",
                            g_strdup (sig_name), g_free);

    gtk_drag_dest_set (button, 0, NULL, 0,
                       GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
    gtk_drag_dest_set_target_list (button, fileview->priv->targets);
    fileview->priv->drag_dest_widgets =
            g_slist_prepend (fileview->priv->drag_dest_widgets, button);

    g_signal_connect_swapped (button, "drag-motion",
                              G_CALLBACK (button_drag_motion), fileview);
    g_signal_connect_swapped (button, "drag-leave",
                              G_CALLBACK (button_drag_leave), fileview);
}

static GtkWidget*
create_toolbar (MooFileView *fileview)
{
    GtkToolbar *toolbar;

    toolbar = moo_ui_xml_create_widget (fileview->priv->ui_xml,
                                        MOO_UI_TOOLBAR,
                                        "MooFileView/Toolbar",
                                        fileview->priv->actions,
                                        NULL);
    g_return_val_if_fail (toolbar != NULL, NULL);
    fileview->toolbar = GTK_WIDGET (toolbar);

    gtk_toolbar_set_tooltips (toolbar, TRUE);
    gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_MENU);

    setup_button_drag_dest (fileview, "MooFileView/Toolbar/GoUp", "go-up");
    setup_button_drag_dest (fileview, "MooFileView/Toolbar/GoBack", "go-back");
    setup_button_drag_dest (fileview, "MooFileView/Toolbar/GoForward", "go-forward");
    setup_button_drag_dest (fileview, "MooFileView/Toolbar/GoHome", "go-home");

    return fileview->toolbar;
}


static void
create_tree_view_proxy (MooFileView *fileview)
{
    fileview->priv->view = _moo_tree_view_new (fileview->priv->filter_model);

    g_signal_connect_swapped (fileview->priv->view, "key-press-event",
                              G_CALLBACK (moo_file_view_key_press),
                              fileview);
    g_signal_connect_swapped (fileview->priv->view, "selection-changed",
                              G_CALLBACK (file_list_selection_changed), fileview);
    g_signal_connect_swapped (fileview->priv->view, "button-press-event",
                              G_CALLBACK (file_list_button_press), fileview);
    g_signal_connect_swapped (fileview->priv->view, "row-activated",
                              G_CALLBACK (file_list_row_activated), fileview);
}


static GtkWidget *
create_notebook (MooFileView *fileview)
{
    GtkWidget *notebook, *swin, *treeview, *iconview, *bkview;

    notebook = gtk_notebook_new ();
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

    create_tree_view_proxy (fileview);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (swin);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), swin, NULL);
    treeview = create_treeview (fileview);
    gtk_widget_show (treeview);
    gtk_container_add (GTK_CONTAINER (swin), treeview);
    fileview->priv->treeview = GTK_TREE_VIEW (treeview);
    /* gtk+ #313719 */
    g_signal_connect_swapped (treeview, "popup-menu",
                              G_CALLBACK (moo_file_view_popup_menu),
                              fileview);
    _moo_tree_view_add (fileview->priv->view, treeview);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (swin);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_ALWAYS,
                                    GTK_POLICY_AUTOMATIC);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), swin, NULL);
    iconview = create_iconview (fileview);
    gtk_widget_show (iconview);
    gtk_container_add (GTK_CONTAINER (swin), iconview);
    fileview->priv->iconview = MOO_ICON_VIEW (iconview);
    _moo_tree_view_add (fileview->priv->view, iconview);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (swin);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), swin, NULL);
    bkview = create_bookmark_view (fileview);
    gtk_widget_show (bkview);
    gtk_container_add (GTK_CONTAINER (swin), bkview);
    fileview->priv->bkview = MOO_BOOKMARK_VIEW (bkview);

    return notebook;
}


static GtkWidget   *create_filter_combo (G_GNUC_UNUSED MooFileView *fileview)
{
    GtkWidget *hbox, *button, *combo;

    hbox = gtk_hbox_new (FALSE, 0);

    button = gtk_toggle_button_new_with_label ("Filter");
    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

    combo = moo_combo_new ();
    gtk_widget_show (combo);
    gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);

    fileview->priv->filter_button = GTK_TOGGLE_BUTTON (button);
    fileview->priv->filter_combo = MOO_COMBO (combo);
    fileview->priv->filter_entry = GTK_ENTRY (MOO_COMBO(combo)->entry);

    g_signal_connect_swapped (button, "toggled",
                              G_CALLBACK (filter_button_toggled),
                              fileview);
    g_signal_connect_data (combo, "changed",
                           G_CALLBACK (filter_combo_changed),
                           fileview, NULL,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
    g_signal_connect_swapped (MOO_COMBO(combo)->entry, "activate",
                              G_CALLBACK (filter_entry_activate),
                              fileview);

    return hbox;
}


static GtkWidget   *create_treeview     (MooFileView    *fileview)
{
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    GtkCellRenderer *cell;

    treeview = gtk_tree_view_new ();

#if 0
    gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview),
                                            GDK_CONTROL_MASK,
                                            source_targets,
                                            G_N_ELEMENTS (source_targets),
                                            GDK_ACTION_COPY | GDK_ACTION_MOVE |
                                                    GDK_ACTION_LINK);
#endif

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, "Name");
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    fileview->priv->tree_name_column = column;

    cell = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) icon_data_func,
                                             fileview, NULL);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) name_data_func,
                                             fileview, NULL);

#ifdef USE_SIZE_AND_STUFF
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, "Size");
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) size_data_func,
                                             fileview, NULL);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, "Date");
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) date_data_func,
                                             fileview, NULL);
#endif

    return treeview;
}


static void
file_list_selection_changed (MooFileView *fileview,
                             MooTreeView *view)
{
    gboolean has_selection;

    if (fileview->priv->select_file_idle)
        g_source_remove (fileview->priv->select_file_idle);
    fileview->priv->select_file_idle = 0;
    g_free (fileview->priv->select_file);
    fileview->priv->select_file = NULL;

    has_selection = !_moo_tree_view_selection_is_empty (view);

    if (fileview->priv->has_selection != has_selection)
    {
        fileview->priv->has_selection = has_selection;
        g_object_notify (G_OBJECT (fileview), "has-selection");
    }
}


static GtkWidget*
create_iconview (MooFileView *fileview)
{
    GtkWidget *iconview;
    GtkCellRenderer *cell;

    iconview = _moo_icon_view_new (NULL);
    _moo_icon_view_set_selection_mode (MOO_ICON_VIEW (iconview),
                                       GTK_SELECTION_MULTIPLE);

    _moo_icon_view_set_cell_data_func (MOO_ICON_VIEW (iconview),
                                       MOO_ICON_VIEW_CELL_PIXBUF,
                                       (MooIconCellDataFunc) icon_data_func,
                                       fileview, NULL);
    _moo_icon_view_set_cell_data_func (MOO_ICON_VIEW (iconview),
                                       MOO_ICON_VIEW_CELL_TEXT,
                                       (MooIconCellDataFunc) name_data_func,
                                       fileview, NULL);

    cell = _moo_icon_view_get_cell (MOO_ICON_VIEW (iconview),
                                    MOO_ICON_VIEW_CELL_TEXT);
    g_object_set (cell, "xalign", 0.0, NULL);
    cell = _moo_icon_view_get_cell (MOO_ICON_VIEW (iconview),
                                    MOO_ICON_VIEW_CELL_PIXBUF);
    g_object_set (cell, "xpad", 1, "ypad", 1, NULL);

    _moo_icon_view_enable_drag_source (MOO_ICON_VIEW (iconview),
                                       GDK_BUTTON1_MASK,
                                       source_targets,
                                       G_N_ELEMENTS (source_targets),
                                       GDK_ACTION_ASK | GDK_ACTION_COPY |
                                              GDK_ACTION_MOVE | GDK_ACTION_LINK);
    _moo_icon_view_enable_drag_dest (MOO_ICON_VIEW (iconview), NULL, 0,
                                     GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
    _moo_icon_view_set_dest_targets (MOO_ICON_VIEW (iconview),
                                     fileview->priv->targets);

    g_signal_connect_swapped (iconview, "drag-begin",
                              G_CALLBACK (icon_drag_begin),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-data-get",
                              G_CALLBACK (icon_drag_data_get),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-end",
                              G_CALLBACK (icon_drag_end),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-data-received",
                              G_CALLBACK (drag_data_received),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-drop",
                              G_CALLBACK (drag_drop),
                              fileview);
    g_signal_connect_swapped (iconview, "drag-leave",
                              G_CALLBACK (drag_leave),
                              fileview);
    g_signal_connect_after (iconview, "drag-motion",
                            G_CALLBACK (drag_motion),
                            fileview);

    return iconview;
}


static GtkWidget *
create_bookmark_view (MooFileView *fileview)
{
    GtkWidget *bkview;

    bkview = _moo_bookmark_view_new (NULL);

//     gtk_tree_view_enable_drag_source (GTK_TREE_VIEW (bkview),
//                                       GDK_BUTTON1_MASK,
//                                       source_targets,
//                                       G_N_ELEMENTS (source_targets),
//                                       GDK_ACTION_ASK | GDK_ACTION_COPY |
//                                               GDK_ACTION_MOVE | GDK_ACTION_LINK);
    gtk_drag_dest_set (bkview, 0, NULL, 0,
                       GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
    gtk_drag_dest_set_target_list (bkview, fileview->priv->targets);

//     g_signal_connect_swapped (bkview, "drag-data-received",
//                               G_CALLBACK (drag_data_received),
//                               fileview);
//     g_signal_connect_swapped (bkview, "drag-drop",
//                               G_CALLBACK (drag_drop),
//                               fileview);
    g_signal_connect_swapped (bkview, "drag-leave",
                              G_CALLBACK (drag_leave),
                              fileview);
    g_signal_connect_after (bkview, "drag-motion",
                            G_CALLBACK (drag_motion),
                            fileview);

    g_signal_connect_swapped (bkview, "bookmark-activated",
                              G_CALLBACK (bookmark_activated), fileview);

    return bkview;
}


static gboolean
moo_file_view_check_visible (MooFileView *fileview,
                             MooFile     *file,
                             gboolean     ignore_hidden,
                             gboolean     ignore_two_dots)
{
    if (!file)
        return FALSE;

    if (!strcmp (_moo_file_name (file), ".."))
    {
        if (!ignore_two_dots)
            return fileview->priv->show_two_dots;
        else
            return FALSE;
    }

    if (!ignore_hidden && MOO_FILE_IS_HIDDEN (file))
        return fileview->priv->show_hidden_files;

    if (MOO_FILE_IS_DIR (file))
        return TRUE;

    if (fileview->priv->current_filter && fileview->priv->use_current_filter)
    {
        GtkFileFilterInfo filter_info;
        GtkFileFilter *filter = fileview->priv->current_filter;

        filter_info.contains = gtk_file_filter_get_needed (filter);
        filter_info.filename = _moo_file_name (file);
        filter_info.uri = NULL;
        filter_info.display_name = _moo_file_display_name (file);
        filter_info.mime_type = _moo_file_get_mime_type (file);

        return gtk_file_filter_filter (fileview->priv->current_filter,
                                       &filter_info);
    }

    return TRUE;
}


static gboolean
filter_visible_func (GtkTreeModel *model,
                     GtkTreeIter  *iter,
                     MooFileView  *fileview)
{
    MooFile *file;
    gboolean visible = TRUE;

    gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

    if (file && fileview->priv->temp_visible &&
        !strncmp (_moo_file_name (file),
                  fileview->priv->temp_visible->str,
                  fileview->priv->temp_visible->len))
    {
        visible = moo_file_view_check_visible (fileview, file, TRUE, TRUE);
    }
    else
    {
        visible = moo_file_view_check_visible (fileview, file, FALSE, FALSE);
    }

    _moo_file_unref (file);
    return visible;
}


static void
icon_data_func (G_GNUC_UNUSED GObject *column_or_iconview,
                GtkCellRenderer    *cell,
                GtkTreeModel       *model,
                GtkTreeIter        *iter,
                MooFileView        *fileview)
{
    MooFile *file = NULL;
    GdkPixbuf *pixbuf = NULL;

    gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

    if (file)
        pixbuf = _moo_file_get_icon (file, GTK_WIDGET (fileview),
                                     fileview->priv->icon_size);

    g_object_set (cell, "pixbuf", pixbuf, NULL);
    _moo_file_unref (file);
}


static void
name_data_func (G_GNUC_UNUSED GObject *column_or_iconview,
                GtkCellRenderer    *cell,
                GtkTreeModel       *model,
                GtkTreeIter        *iter,
                G_GNUC_UNUSED MooFileView *fileview)
{
    MooFile *file = NULL;
    const char *name = NULL;

    gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

    if (file)
        name = _moo_file_display_name (file);

    g_object_set (cell, "text", name, NULL);
    _moo_file_unref (file);
}


#ifdef USE_SIZE_AND_STUFF
static void date_data_func  (G_GNUC_UNUSED GObject            *column_or_iconview,
                             G_GNUC_UNUSED GtkCellRenderer    *cell,
                             G_GNUC_UNUSED GtkTreeModel       *model,
                             G_GNUC_UNUSED GtkTreeIter        *iter,
                             G_GNUC_UNUSED MooFileView *fileview)
{
}

static void size_data_func  (G_GNUC_UNUSED GObject            *column_or_iconview,
                             G_GNUC_UNUSED GtkCellRenderer    *cell,
                             G_GNUC_UNUSED GtkTreeModel       *model,
                             G_GNUC_UNUSED GtkTreeIter        *iter,
                             G_GNUC_UNUSED MooFileView *fileview)
{
}
#endif


gboolean
moo_file_view_chdir (MooFileView    *fileview,
                     const char     *dir,
                     GError        **error)
{
    gboolean result;

    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), FALSE);

    g_signal_emit (fileview, signals[CHDIR], 0, dir, error, &result);

    return result;
}


/* TODO */
static void
moo_file_view_go_up (MooFileView *fileview)
{
    MooFolder *parent = NULL;

    if (fileview->priv->entry_state)
        stop_path_entry (fileview, TRUE);

    if (fileview->priv->view_type == MOO_FILE_VIEW_BOOKMARK)
    {
        view_files (fileview);
        return;
    }

    if (fileview->priv->current_dir)
        parent = _moo_folder_get_parent (fileview->priv->current_dir,
                                         MOO_FILE_HAS_ICON);
    if (!parent)
        parent = _moo_file_system_get_root_folder (fileview->priv->file_system,
                                                   MOO_FILE_HAS_ICON);

    g_return_if_fail (parent != NULL);

    if (parent != fileview->priv->current_dir)
    {
        const char *path = _moo_folder_get_path (parent);
        char *name = g_path_get_basename (_moo_folder_get_path (fileview->priv->current_dir));

        if (moo_file_view_chdir (fileview, path, NULL))
            _moo_file_view_select_name (fileview, name);

        g_free (name);
    }

    g_object_unref (parent);
}


static void
moo_file_view_go_home (MooFileView *fileview)
{
    GError *error = NULL;

    if (fileview->priv->entry_state)
        stop_path_entry (fileview, TRUE);

    if (!moo_file_view_chdir (fileview, fileview->priv->home_dir, &error))
    {
        g_warning ("%s: could not go home", G_STRLOC);

        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }
}


static void
file_list_row_activated (MooFileView    *fileview,
                         GtkTreePath    *filter_path)
{
    MooFile *file = NULL;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter (fileview->priv->filter_model,
                                  &iter, filter_path))
    {
        g_return_if_reached ();
    }

    gtk_tree_model_get (fileview->priv->filter_model,
                        &iter, COLUMN_FILE, &file, -1);
    g_return_if_fail (file != NULL);

    if (!strcmp (_moo_file_name (file), ".."))
    {
        g_signal_emit_by_name (fileview, "go-up");
    }
    else if (MOO_FILE_IS_DIR (file))
    {
        GError *error = NULL;

        if (!moo_file_view_chdir (fileview, _moo_file_name (file), &error))
        {
            g_warning ("%s: could not go into '%s'",
                       G_STRLOC, _moo_file_name (file));

            if (error)
            {
                g_warning ("%s: %s", G_STRLOC, error->message);
                g_error_free (error);
            }
        }
    }
    else
    {
        char *path;
        g_assert (fileview->priv->current_dir != NULL);
        path = g_build_filename (_moo_folder_get_path (fileview->priv->current_dir),
                                 _moo_file_name (file), NULL);
        g_signal_emit (fileview, signals[ACTIVATE], 0, path);
        g_free (path);
    }

    _moo_file_unref (file);
}


struct _History {
    GSList *back;
    GSList *fwd;
    char *current;
    guint block;
};


static void
history_init (MooFileView *fileview)
{
    fileview->priv->history = g_new0 (History, 1);
}


static void
history_clear (MooFileView *fileview)
{
    g_slist_foreach (fileview->priv->history->back, (GFunc) g_free, NULL);
    g_slist_foreach (fileview->priv->history->fwd, (GFunc) g_free, NULL);
    g_slist_free (fileview->priv->history->back);
    g_slist_free (fileview->priv->history->fwd);
    fileview->priv->history->back = NULL;
    fileview->priv->history->fwd = NULL;
    g_free (fileview->priv->history->current);
    fileview->priv->history->current = NULL;
}


static void
history_free (MooFileView *fileview)
{
    history_clear (fileview);
    g_free (fileview->priv->history);
    fileview->priv->history = NULL;
}


static const char*
history_get (MooFileView *fileview,
             int          where)
{
    History *hist = fileview->priv->history;

    g_assert (where == 1 || where == -1);

    if (where == 1)
    {
        if (!hist->fwd)
            return NULL;
        else
            return hist->fwd->data;
    }
    else
    {
        if (!hist->back)
            return NULL;
        else
            return hist->back->data;
    }
}


static void
history_go (MooFileView *fileview,
            int          where)
{
    History *hist = fileview->priv->history;
    char *tmp;
    GSList **pop_from, **push_to;
    gboolean could_go_forward, could_go_back;
    gboolean can_go_forward, can_go_back;

    g_assert (where == 1 || where == -1);
    g_assert (hist->current != NULL);

    could_go_forward = hist->fwd != NULL;
    could_go_back = hist->back != NULL;

    if (where == 1)
    {
        if (!hist->fwd)
            return;
        pop_from = &hist->fwd;
        push_to = &hist->back;
    }
    else
    {
        if (!hist->back)
            return;
        pop_from = &hist->back;
        push_to = &hist->fwd;
    }

    tmp = (*pop_from)->data;
    *pop_from = g_slist_delete_link (*pop_from, *pop_from);
    *push_to = g_slist_prepend (*push_to, hist->current);
    hist->current = tmp;

    can_go_forward = hist->fwd != NULL;
    can_go_back = hist->back != NULL;

    g_object_freeze_notify (G_OBJECT (fileview));
    if (could_go_forward != can_go_forward)
        g_object_notify (G_OBJECT (fileview), "can-go-forward");
    if (could_go_back != can_go_back)
        g_object_notify (G_OBJECT (fileview), "can-go-back");
    g_object_thaw_notify (G_OBJECT (fileview));
}


static void
history_add (MooFileView    *fileview,
             const char     *dirname)
{
    History *hist = fileview->priv->history;
    gboolean could_go_forward = FALSE, could_go_back, can_go_back;

    g_return_if_fail (dirname != NULL);

    if (hist->block)
        return;

    /* XXX strcmp */
    if (hist->current && !strcmp (dirname, hist->current))
        return;

    if (hist->fwd)
    {
        could_go_forward = TRUE;
        g_slist_foreach (hist->fwd, (GFunc) g_free, NULL);
        g_slist_free (hist->fwd);
        hist->fwd = NULL;
    }

    could_go_back = hist->back != NULL;

    if (hist->current)
    {
        hist->back = g_slist_prepend (hist->back, hist->current);
        hist->current = g_strdup (dirname);
    }
    else
    {
        hist->current = g_strdup (dirname);
    }

    can_go_back = hist->back != NULL;

    g_object_freeze_notify (G_OBJECT (fileview));
    if (could_go_back != can_go_back)
        g_object_notify (G_OBJECT (fileview), "can-go-back");
    if (could_go_forward)
        g_object_notify (G_OBJECT (fileview), "can-go-forward");
    g_object_thaw_notify (G_OBJECT (fileview));
}


static void
moo_file_view_go (MooFileView *fileview,
                  int          where)
{
    const char *dir;
    GError *error = NULL;

    g_assert (where == 1 || where == -1);

    if (fileview->priv->entry_state)
        stop_path_entry (fileview, TRUE);

    if (fileview->priv->view_type == MOO_FILE_VIEW_BOOKMARK)
    {
        view_files (fileview);
        return;
    }

    dir = history_get (fileview, where);

    if (dir)
    {
        fileview->priv->history->block++;

        if (!moo_file_view_chdir (fileview, dir, &error))
        {
            g_warning ("%s: could not go into '%s'",
                       G_STRLOC, dir);

            if (error)
            {
                g_warning ("%s: %s", G_STRLOC, error->message);
                g_error_free (error);
            }
        }
        else
        {
            history_go (fileview, where);
        }

        fileview->priv->history->block--;
    }
}


static void
moo_file_view_go_back (MooFileView *fileview)
{
    moo_file_view_go (fileview, -1);
}


static void
moo_file_view_go_forward (MooFileView *fileview)
{
    moo_file_view_go (fileview, 1);
}


static void
moo_file_view_set_property (GObject        *object,
                            guint           prop_id,
                            const GValue   *value,
                            GParamSpec     *pspec)
{
    MooFileView *fileview = MOO_FILE_VIEW (object);

    switch (prop_id)
    {
        case PROP_CURRENT_DIRECTORY:
            moo_file_view_chdir (fileview, g_value_get_string (value), NULL);
            break;

        case PROP_HOME_DIRECTORY:
            g_free (fileview->priv->home_dir);
            fileview->priv->home_dir = g_strdup (g_value_get_string (value));
            g_object_notify (object, "home-directory");
            break;

        case PROP_FILTER_MGR:
            moo_file_view_set_filter_mgr (fileview, g_value_get_object (value));
            break;

        case PROP_BOOKMARK_MGR:
            moo_file_view_set_bookmark_mgr (fileview, g_value_get_object (value));
            break;

        case PROP_SORT_CASE_SENSITIVE:
            _moo_file_view_set_sort_case_sensitive (fileview, g_value_get_boolean (value));
            break;

        case PROP_TYPEAHEAD_CASE_SENSITIVE:
            _moo_file_view_set_typeahead_case_sensitive (fileview, g_value_get_boolean (value));
            break;

        case PROP_COMPLETION_CASE_SENSITIVE:
            g_object_set (MOO_FILE_ENTRY(fileview->priv->entry)->completion,
                          "case-sensitive", g_value_get_boolean (value), NULL);
            g_object_notify (object, "completion-case-sensitive");
            break;

        case PROP_SHOW_HIDDEN_FILES:
            _moo_file_view_set_show_hidden (fileview, g_value_get_boolean (value));
            break;

        case PROP_SHOW_PARENT_FOLDER:
            _moo_file_view_set_show_parent (fileview, g_value_get_boolean (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


static void
moo_file_view_get_property (GObject        *object,
                            guint           prop_id,
                            GValue         *value,
                            GParamSpec     *pspec)
{
    MooFileView *fileview = MOO_FILE_VIEW (object);
    gboolean val;

    switch (prop_id)
    {
        case PROP_CURRENT_DIRECTORY:
            if (fileview->priv->current_dir)
                g_value_set_string (value, _moo_folder_get_path (fileview->priv->current_dir));
            else
                g_value_set_string (value, NULL);
            break;

        case PROP_HOME_DIRECTORY:
            g_value_set_string (value, fileview->priv->home_dir);
            break;

        case PROP_FILTER_MGR:
            g_value_set_object (value, fileview->priv->filter_mgr);
            break;

        case PROP_BOOKMARK_MGR:
            g_value_set_object (value, fileview->priv->bookmark_mgr);
            break;

        case PROP_SORT_CASE_SENSITIVE:
            g_value_set_boolean (value, fileview->priv->sort_case_sensitive);
            break;

        case PROP_TYPEAHEAD_CASE_SENSITIVE:
            g_value_set_boolean (value, fileview->priv->typeahead_case_sensitive);
            break;

        case PROP_COMPLETION_CASE_SENSITIVE:
            g_object_get (MOO_FILE_ENTRY(fileview->priv->entry)->completion,
                          "case-sensitive", &val, NULL);
            g_value_set_boolean (value, val);
            break;

        case PROP_HAS_SELECTION:
            if (fileview->priv->current_dir)
                g_value_set_boolean (value,
                    !_moo_tree_view_selection_is_empty (fileview->priv->view));
            else
                g_value_set_boolean (value, FALSE);
            break;

        case PROP_CAN_GO_BACK:
            g_value_set_boolean (value, history_get (fileview, -1) != NULL);
            break;

        case PROP_CAN_GO_FORWARD:
            g_value_set_boolean (value, history_get (fileview, 1) != NULL);
            break;

        case PROP_SHOW_HIDDEN_FILES:
            g_value_set_boolean (value, fileview->priv->show_hidden_files);
            break;

        case PROP_SHOW_PARENT_FOLDER:
            g_value_set_boolean (value, fileview->priv->show_two_dots);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


/*****************************************************************************/
/* Clipboard
 */

enum {
    CB_TARGET_CLIPBOARD = 1,
    CB_TARGET_URI_LIST = 2
};

static GtkTargetEntry clipboard_targets[] = {
    {(char*) "MOO_FILE_VIEW_CLIPBOARD", GTK_TARGET_SAME_APP, CB_TARGET_CLIPBOARD},
    {(char*) "text/uri-list", 0, CB_TARGET_URI_LIST},
};

static void
moo_file_view_unrealize (GtkWidget *widget)
{
    file_view_clear_clipboard (MOO_FILE_VIEW (widget));
    GTK_WIDGET_CLASS(moo_file_view_parent_class)->unrealize (widget);
}


static Clipboard *
clipboard_new (MooFolder *folder,
               GList     *files,
               gboolean   cut)
{
    Clipboard *cb;

    g_return_val_if_fail (MOO_IS_FOLDER (folder), NULL);

    cb = g_new0 (Clipboard, 1);
    cb->folder = g_object_ref (folder);
    cb->files = g_list_copy (files);
    g_list_foreach (cb->files, (GFunc) _moo_file_ref, NULL);
    cb->cut = cut != 0;

    return cb;
}


static void
clipboard_free (Clipboard *cb)
{
    if (cb)
    {
        g_object_unref (cb->folder);
        g_list_foreach (cb->files, (GFunc) _moo_file_unref, NULL);
        g_list_free (cb->files);
        g_free (cb);
    }
}


static void
file_view_clear_clipboard (MooFileView *fileview)
{
    if (fileview->priv->clipboard)
    {
        GtkClipboard *cb = gtk_widget_get_clipboard (GTK_WIDGET (fileview),
                                                     GDK_SELECTION_CLIPBOARD);

        if (gtk_clipboard_get_owner (cb) == G_OBJECT (fileview))
            gtk_clipboard_clear (cb);

        if (fileview->priv->clipboard)
        {
            clipboard_free (fileview->priv->clipboard);
            fileview->priv->clipboard = NULL;
        }
    }
}


static void
clear_clipboard_cb (G_GNUC_UNUSED GtkClipboard *clipboard,
                    MooFileView  *fileview)
{
    clipboard_free (fileview->priv->clipboard);
    fileview->priv->clipboard = NULL;
}


static void
set_selection_data_from_clipboard (GtkSelectionData *data,
                                   Clipboard        *cb)
{
    GList *l;
    char **uris;
    int n_uris, i;

    n_uris = g_list_length (cb->files);
    g_return_if_fail (n_uris > 0);

    uris = g_new0 (char*, n_uris + 1);

    for (i = 0, l = cb->files; l != NULL; l = l->next, ++i)
        uris[i] = _moo_folder_get_file_uri (cb->folder, l->data);

    gtk_selection_data_set_uris (data, uris);

    g_strfreev (uris);
}


static void
get_clipboard_cb (G_GNUC_UNUSED GtkClipboard *clipboard,
                  GtkSelectionData  *selection_data,
                  guint              info,
                  MooFileView       *fileview)
{
    g_return_if_fail (fileview->priv->clipboard != NULL);

    switch (info)
    {
        case CB_TARGET_CLIPBOARD:
            _moo_selection_data_set_pointer (selection_data,
                                             moo_file_view_clipboard,
                                             fileview);
            break;

        case CB_TARGET_URI_LIST:
            set_selection_data_from_clipboard (selection_data,
                                               fileview->priv->clipboard);
            break;

        default:
            g_return_if_reached ();
    }
}


static void
file_view_cut_or_copy_clipboard (MooFileView *fileview,
                                 gboolean     cut)
{
    GList *files;
    MooFolder *folder;
    gboolean result;
    GtkClipboard *clipboard;

    g_return_if_fail (GTK_WIDGET_REALIZED (fileview));

    folder = fileview->priv->current_dir;
    files = file_view_get_selected_files (fileview);

    if (!folder || !files)
        return;

    if (fileview->priv->clipboard)
    {
        clipboard_free (fileview->priv->clipboard);
        fileview->priv->clipboard = NULL;
    }

    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (fileview),
                                          GDK_SELECTION_CLIPBOARD);
    result = gtk_clipboard_set_with_owner (clipboard, clipboard_targets,
                                           G_N_ELEMENTS (clipboard_targets),
                                           (GtkClipboardGetFunc) get_clipboard_cb,
                                           (GtkClipboardClearFunc) clear_clipboard_cb,
                                           G_OBJECT (fileview));

    if (result)
        fileview->priv->clipboard = clipboard_new (folder, files, cut);

    g_list_foreach (files, (GFunc) _moo_file_unref, NULL);
    g_list_free (files);
}


static void
file_view_cut_clipboard (MooFileView *fileview)
{
    file_view_cut_or_copy_clipboard (fileview, TRUE);
}


static void
file_view_copy_clipboard (MooFileView *fileview)
{
    file_view_cut_or_copy_clipboard (fileview, FALSE);
}


static void
file_view_paste_clipboard (MooFileView *fileview)
{
    GtkClipboard *clipboard;
    GtkSelectionData *data = NULL;

    g_return_if_fail (fileview->priv->current_dir != NULL);

    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (fileview),
                                          GDK_SELECTION_CLIPBOARD);

    data = gtk_clipboard_wait_for_contents (clipboard, moo_file_view_clipboard);

    if (data)
    {
        Clipboard *cb;
        MooFileView *remote;
        GList *filenames, *l;
        const char *destdir;

        remote = _moo_selection_data_get_pointer (data, moo_file_view_clipboard);
        g_return_if_fail (remote != NULL);

        cb = remote->priv->clipboard;

        if (cb->folder == fileview->priv->current_dir)
            goto out;

        for (filenames = NULL, l = cb->files; l != NULL; l = l->next)
        {
            MooFile *file = l->data;
            char *path = _moo_folder_get_file_path (cb->folder, file);

            if (path)
                filenames = g_list_prepend (filenames, path);
        }

        filenames = g_list_reverse (filenames);
        destdir = _moo_folder_get_path (fileview->priv->current_dir);

        if (cb->cut)
        {
            remote->priv->clipboard = NULL;
            gtk_clipboard_clear (clipboard);

            move_files (fileview, filenames, destdir);

            clipboard_free (cb);
        }
        else
        {
            copy_files (fileview, filenames, destdir);
        }

        g_list_foreach (filenames, (GFunc) g_free, NULL);
        g_list_free (filenames);

        goto out;
    }

    data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern ("text/uri-list", FALSE));

    if (data)
    {
        char **uris = gtk_selection_data_get_uris (data);
        GList *filenames;
        char **u;
        const char *destdir;

        for (u = uris, filenames = NULL; u && *u; u++)
        {
            char *name = g_filename_from_uri (*u, NULL, NULL);

            if (name)
                filenames = g_list_prepend (filenames, name);
        }

        filenames = g_list_reverse (filenames);
        destdir = _moo_folder_get_path (fileview->priv->current_dir);

        if (filenames)
            copy_files (fileview, filenames, destdir);

        g_strfreev (uris);
        g_list_foreach (filenames, (GFunc) g_free, NULL);
        g_list_free (filenames);

        goto out;
    }

out:
    if (data)
        gtk_selection_data_free (data);
}


/*****************************************************************************/
/* Sorting and stuff
 */

void
_moo_file_view_set_sort_case_sensitive (MooFileView    *fileview,
                                        gboolean        case_sensitive)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    if (case_sensitive != fileview->priv->sort_case_sensitive)
    {
        fileview->priv->sort_case_sensitive = case_sensitive;
        g_object_set (fileview->priv->model,
                      "sort-case-sensitive",
                      case_sensitive, NULL);
        g_object_notify (G_OBJECT (fileview), "sort-case-sensitive");
    }
}


/*****************************************************************************/
/* Filters
 */

static void
moo_file_view_set_filter_mgr (MooFileView    *fileview,
                              MooFilterMgr   *mgr)
{
    if (!mgr)
    {
        mgr = moo_filter_mgr_new ();
        moo_file_view_set_filter_mgr (fileview, mgr);
        g_object_unref (mgr);
        g_object_notify (G_OBJECT (fileview), "filter-mgr");
        return;
    }

    if (mgr == fileview->priv->filter_mgr)
        return;

    if (fileview->priv->filter_mgr)
        g_object_unref (fileview->priv->filter_mgr);
    fileview->priv->filter_mgr = g_object_ref (mgr);

    init_filter_combo (fileview);
    g_object_notify (G_OBJECT (fileview), "filter-mgr");
}


static void
block_filter_signals (MooFileView *fileview)
{
    g_signal_handlers_block_by_func (fileview->priv->filter_combo,
                                     (gpointer) filter_button_toggled,
                                     fileview);
    g_signal_handlers_block_by_func (fileview->priv->filter_combo,
                                     (gpointer) filter_combo_changed,
                                     fileview);
    g_signal_handlers_block_by_func (fileview->priv->filter_combo,
                                     (gpointer) filter_entry_activate,
                                     fileview);
}

static void
unblock_filter_signals (MooFileView *fileview)
{
    g_signal_handlers_unblock_by_func (fileview->priv->filter_combo,
                                       (gpointer) filter_button_toggled,
                                       fileview);
    g_signal_handlers_unblock_by_func (fileview->priv->filter_combo,
                                       (gpointer) filter_combo_changed,
                                       fileview);
    g_signal_handlers_unblock_by_func (fileview->priv->filter_combo,
                                       (gpointer) filter_entry_activate,
                                       fileview);
}


static void
init_filter_combo (MooFileView *fileview)
{
    MooFilterMgr *mgr = fileview->priv->filter_mgr;
    GtkFileFilter *filter;

    block_filter_signals (fileview);

    moo_filter_mgr_init_filter_combo (mgr, fileview->priv->filter_combo,
                                      "MooFileView");
    if (fileview->priv->current_filter)
        g_object_unref (fileview->priv->current_filter);
    fileview->priv->current_filter = NULL;
    fileview->priv->use_current_filter = FALSE;
    gtk_toggle_button_set_active (fileview->priv->filter_button, FALSE);
    gtk_entry_set_text (fileview->priv->filter_entry, "");

    unblock_filter_signals (fileview);

    filter = moo_filter_mgr_get_last_filter (mgr, "MooFileView");

    if (filter)
        fileview_set_filter (fileview, filter);
}


static void
filter_button_toggled (MooFileView *fileview)
{
    gboolean active =
            gtk_toggle_button_get_active (fileview->priv->filter_button);

    if (active == fileview->priv->use_current_filter)
        return;

    /* TODO check entry content */
    fileview_set_use_filter (fileview, active, TRUE);
    focus_to_file_view (fileview);
}


static void
filter_combo_changed (MooFileView *fileview)
{
    GtkTreeIter iter;
    GtkFileFilter *filter;
    MooFilterMgr *mgr = fileview->priv->filter_mgr;
    MooCombo *combo = fileview->priv->filter_combo;

    if (!moo_combo_get_active_iter (combo, &iter))
        return;

    filter = moo_filter_mgr_get_filter (mgr, &iter, "MooFileView");
    g_return_if_fail (filter != NULL);
    moo_filter_mgr_set_last_filter (mgr, &iter, "MooFileView");

    fileview_set_filter (fileview, filter);
    focus_to_file_view (fileview);
}


static void
filter_entry_activate (MooFileView *fileview)
{
    const char *text;
    GtkFileFilter *filter;
    MooFilterMgr *mgr = fileview->priv->filter_mgr;

    text = gtk_entry_get_text (fileview->priv->filter_entry);

    if (text && text[0])
        filter = moo_filter_mgr_new_user_filter (mgr, text, "MooFileView");
    else
        filter = NULL;

    fileview_set_filter (fileview, filter);
    focus_to_file_view (fileview);
}


static void
fileview_set_filter (MooFileView    *fileview,
                     GtkFileFilter  *filter)
{
    GtkFileFilter *null_filter;

    if (filter && filter == fileview->priv->current_filter)
    {
        fileview_set_use_filter (fileview, TRUE, TRUE);
        return;
    }

    null_filter = moo_filter_mgr_get_null_filter (fileview->priv->filter_mgr);
    g_return_if_fail (null_filter != NULL);

    if (filter == null_filter)
        return fileview_set_filter (fileview, NULL);

    if (filter && (gtk_file_filter_get_needed (filter) & GTK_FILE_FILTER_URI))
    {
        g_warning ("%s: The filter set wants uri, but i do not know "
                   "how to work with uri's. Ignoring", G_STRLOC);
        return;
    }

    block_filter_signals (fileview);

    if (fileview->priv->current_filter)
        g_object_unref (fileview->priv->current_filter);
    fileview->priv->current_filter = filter;

    if (filter)
    {
        const char *name;
        gtk_object_sink (g_object_ref (filter));
        name = gtk_file_filter_get_name (filter);
        gtk_entry_set_text (fileview->priv->filter_entry, name);
        fileview_set_use_filter (fileview, TRUE, FALSE);
    }
    else
    {
        gtk_entry_set_text (fileview->priv->filter_entry, "");
        fileview_set_use_filter (fileview, FALSE, FALSE);
    }

    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
            (fileview->priv->filter_model));

    unblock_filter_signals (fileview);
}


static void
fileview_set_use_filter (MooFileView    *fileview,
                         gboolean        use,
                         gboolean        block_signals)
{
    if (block_signals)
        block_filter_signals (fileview);

    gtk_toggle_button_set_active (fileview->priv->filter_button, use);

    if (fileview->priv->use_current_filter != use)
    {
        if (fileview->priv->current_filter)
        {
            fileview->priv->use_current_filter = use;
            if (block_signals)
                gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                        (fileview->priv->filter_model));
        }
        else
        {
            fileview->priv->use_current_filter = FALSE;
            gtk_toggle_button_set_active (fileview->priv->filter_button, FALSE);
        }
    }

    if (block_signals)
        unblock_filter_signals (fileview);
}


/*****************************************************************************/
/* File manager functionality
 */

GList *
_moo_file_view_get_filenames (MooFileView *fileview)
{
    GList *files, *names = NULL;
    MooFolder *folder;

    g_return_val_if_fail (MOO_IS_FILE_VIEW (fileview), NULL);

    folder = fileview->priv->current_dir;
    g_return_val_if_fail (folder != NULL, NULL);

    files = file_view_get_selected_files (fileview);

    while (files)
    {
        names = g_list_prepend (names, _moo_folder_get_file_path (folder, files->data));
        _moo_file_unref (files->data);
        files = g_list_delete_link (files, files);
    }

    return g_list_reverse (names);
}


GList *
_moo_file_view_get_files (MooFileView *fileview)
{
    return file_view_get_selected_files (fileview);
}


/* path in the filter model; result must be unrefed */
static MooFile *
file_view_get_file_at_path (MooFileView *fileview,
                            GtkTreePath *path)
{
    GtkTreeIter iter;
    MooFile *file = NULL;

    g_return_val_if_fail (path != NULL, NULL);

    gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, path);
    gtk_tree_model_get (fileview->priv->filter_model, &iter, COLUMN_FILE, &file, -1);

    return file;
}


static void
prepend_file (GtkTreeModel       *model,
              G_GNUC_UNUSED GtkTreePath *path,
              GtkTreeIter        *iter,
              gpointer            user_data)
{
    GList **list = user_data;
    MooFile *file = NULL;

    gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

    if (file)
        *list = g_list_prepend (*list, file);
}

/* returned files must be unrefed and list freed */
static GList *
file_view_get_selected_files (MooFileView *fileview)
{
    GList *files = NULL;
    _moo_tree_view_selected_foreach (fileview->priv->view,
                                     prepend_file, &files);
    return files;
}


static void
file_view_delete_selected (MooFileView *fileview)
{
    GError *error = NULL;
    GList *files, *l;
    gboolean one;
    char *message, *path;
    GtkWidget *dialog;
    int response;

    if (!fileview->priv->current_dir)
        return;

    files = file_view_get_selected_files (fileview);

    if (!files)
        return;

    one = (files->next == NULL);

    if (one)
    {
        if (MOO_FILE_IS_DIR (files->data) && !MOO_FILE_IS_LINK (files->data))
            message = g_strdup_printf ("Delete folder %s and all its content?",
                                       _moo_file_display_name (files->data));
        else
            message = g_strdup_printf ("Delete file %s?",
                                       _moo_file_display_name (files->data));
    }
    else
    {
        message = g_strdup ("Delete selected files?");
    }

    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_NONE,
                                     "%s", message);
    moo_window_set_parent (dialog, GTK_WIDGET (fileview));

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_DELETE, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    if (response == GTK_RESPONSE_OK)
    {
        for (l = files; l != NULL; l = l->next)
        {
            path = g_build_filename (_moo_folder_get_path (fileview->priv->current_dir),
                                     _moo_file_name (l->data), NULL);

            if (!_moo_file_system_delete_file (fileview->priv->file_system, path, TRUE, &error))
            {
                dialog = gtk_message_dialog_new (NULL,
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_NONE,
                                                 "Could not delete %s '%s'",
                                                 MOO_FILE_IS_DIR (l->data) ? "folder" : "file",
                                                 path);

                moo_window_set_parent (dialog, GTK_WIDGET (fileview));

                if (error)
                {
                    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                              "%s", error->message);
                    g_error_free (error);
                }

                gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
                gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
            }

            g_free (path);
        }
    }

    g_list_foreach (files, (GFunc) _moo_file_unref, NULL);
    g_list_free (files);
}


static void
file_view_create_folder (MooFileView *fileview)
{
    GError *error = NULL;
    char *path, *name;

    if (!fileview->priv->current_dir)
        return;

    name = _moo_file_view_create_folder_dialog (GTK_WIDGET (fileview),
                                                fileview->priv->current_dir);

    if (!name || !name[0])
    {
        g_free (name);
        return;
    }

    path = _moo_file_system_make_path (fileview->priv->file_system,
                                       _moo_folder_get_path (fileview->priv->current_dir),
                                       name, &error);

    if (!path)
    {
        g_message ("%s: could not make path for '%s'", G_STRLOC, name);

        if (error)
        {
            g_message ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }

        goto out;
    }

    if (!_moo_file_system_create_folder (fileview->priv->file_system, path, &error))
    {
        g_message ("%s: could not create folder '%s'", G_STRLOC, name);

        if (error)
        {
            g_message ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }

        goto out;
    }

    _moo_file_view_select_display_name (fileview, name);

out:
    g_free (path);
    g_free (name);
}


/* XXX */
static void
file_view_properties_dialog (MooFileView *fileview)
{
    GtkWidget *dialog;
    GList *files;

    if (!fileview->priv->current_dir)
        return;

    files = file_view_get_selected_files (fileview);

    if (!files)
    {
        g_print ("no files\n");
        return;
    }

    if (files->next)
    {
        g_print ("many files\n");
        g_list_foreach (files, (GFunc) _moo_file_unref, NULL);
        g_list_free (files);
        return;
    }

    dialog = g_object_get_data (G_OBJECT (fileview),
                                "moo-file-view-properties-dialog");

    if (!dialog)
    {
        dialog = _moo_file_props_dialog_new (GTK_WIDGET (fileview));
        gtk_object_sink (g_object_ref (dialog));
        g_object_set_data_full (G_OBJECT (fileview),
                                "moo-file-view-properties-dialog",
                                dialog, g_object_unref);
        g_signal_connect (dialog, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }

    _moo_file_props_dialog_set_file (MOO_FILE_PROPS_DIALOG (dialog),
                                     files->data, fileview->priv->current_dir);
    gtk_window_present (GTK_WINDOW (dialog));

    g_list_foreach (files, (GFunc) _moo_file_unref, NULL);
    g_list_free (files);
}


/*****************************************************************************/
/* Popup menu
 */

static gboolean
really_destroy_menu (GtkWidget *menu)
{
    g_object_unref (menu);
    return FALSE;
}

static void
destroy_menu (GtkWidget *menu)
{
    g_idle_add ((GSourceFunc) really_destroy_menu, menu);
}

/* TODO */
static void
menu_position_func (G_GNUC_UNUSED GtkMenu *menu,
                    gint       *x,
                    gint       *y,
                    gboolean   *push_in,
                    gpointer    user_data)
{
    GdkWindow *window;

    struct {
        MooFileView *fileview;
        GList *rows;
    } *data = user_data;

    window = GTK_WIDGET(data->fileview)->window;
    gdk_window_get_origin (window, x, y);

    *push_in = TRUE;
}

static void
do_popup (MooFileView    *fileview,
          GdkEventButton *event,
          GList          *selected)
{
    GtkWidget *menu;
    GList *files = NULL, *l;
    struct {
        MooFileView *fileview;
        GList *rows;
    } position_data = {fileview, selected};

    for (l = selected; l != NULL; l = l->next)
    {
        GtkTreeIter iter;
        MooFile *file = NULL;
        gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, l->data);
        gtk_tree_model_get (fileview->priv->filter_model, &iter,
                            COLUMN_FILE, &file, -1);
        if (file)
            files = g_list_prepend (files, file);
    }

    menu = moo_ui_xml_create_widget (fileview->priv->ui_xml,
                                     MOO_UI_MENU, "MooFileView/Menu",
                                     fileview->priv->actions,
                                     NULL);
    gtk_object_sink (g_object_ref (menu));
    g_signal_connect (menu, "deactivate", G_CALLBACK (destroy_menu), NULL);

    _moo_file_view_tools_check (fileview);
    g_signal_emit (fileview, signals[POPULATE_POPUP], 0, files, menu);

    if (event)
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                        event->button, event->time);
    else
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                        menu_position_func,
                        &position_data, 0,
                        gtk_get_current_event_time ());

    g_list_foreach (files, (GFunc) _moo_file_unref, NULL);
    g_list_free (files);
}


static gboolean
moo_file_view_popup_menu (GtkWidget *widget)
{
    GList *selected;
    MooFileView *fileview = MOO_FILE_VIEW (widget);

    selected = _moo_tree_view_get_selected_rows (fileview->priv->view);
    do_popup (fileview, NULL, selected);
    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);

    return TRUE;
}


static gboolean
file_list_button_press (MooFileView    *fileview,
                        G_GNUC_UNUSED GtkWidget *widget,
                        GdkEventButton *event,
                        MooTreeView    *view)
{
    GtkTreePath *filter_path = NULL;
    GList *selected;

    if (event->button != 3)
        return FALSE;

    if (_moo_tree_view_get_path_at_pos (view, event->x, event->y, &filter_path))
    {
        if (!_moo_tree_view_path_is_selected (view, filter_path))
        {
            _moo_tree_view_unselect_all (view);
            _moo_tree_view_set_cursor (view, filter_path, FALSE);
        }
    }
    else
    {
        _moo_tree_view_unselect_all (view);
    }

    selected = _moo_tree_view_get_selected_rows (view);
    do_popup (fileview, event, selected);
    gtk_tree_path_free (filter_path);
    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);

    return TRUE;
}


void
_moo_file_view_set_show_hidden (MooFileView    *fileview,
                               gboolean        show)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    show = show ? TRUE : FALSE;

    if (fileview->priv->show_hidden_files != show)
    {
        fileview->priv->show_hidden_files = show;
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (
                fileview->priv->filter_model));
        g_object_notify (G_OBJECT (fileview), "show-hidden-files");
    }
}


void
_moo_file_view_set_show_parent (MooFileView    *fileview,
                               gboolean        show)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    show = show ? TRUE : FALSE;

    if (fileview->priv->show_two_dots != show)
    {
        fileview->priv->show_two_dots = show;
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (
                fileview->priv->filter_model));
        g_object_notify (G_OBJECT (fileview), "show-parent-folder");
    }
}


static void
toggle_show_hidden (MooFileView *fileview)
{
    _moo_file_view_set_show_hidden (fileview,
                                   !fileview->priv->show_hidden_files);
}


static GtkWidget *
get_view_widget (MooFileView *fileview)
{
    switch (fileview->priv->view_type)
    {
        case MOO_FILE_VIEW_ICON:
            return GTK_WIDGET(fileview->priv->iconview);
        case MOO_FILE_VIEW_LIST:
            return GTK_WIDGET(fileview->priv->treeview);
        case MOO_FILE_VIEW_BOOKMARK:
            return GTK_WIDGET(fileview->priv->bkview);
    }

    g_return_val_if_reached (NULL);
}


/*****************************************************************************/
/* Bookmarks menu
 */

static void
moo_file_view_set_bookmark_mgr  (MooFileView    *fileview,
                                 MooBookmarkMgr *mgr)
{
    if (!mgr)
    {
        mgr = _moo_bookmark_mgr_new ();
        moo_file_view_set_bookmark_mgr (fileview, mgr);
        g_object_unref (mgr);
        return;
    }

    if (fileview->priv->bookmark_mgr == mgr)
        return;

    if (fileview->priv->bookmark_mgr)
    {
        g_signal_handlers_disconnect_by_func (fileview->priv->bookmark_mgr,
                                              (gpointer) bookmark_activate,
                                              fileview);
        _moo_bookmark_mgr_remove_user (fileview->priv->bookmark_mgr,
                                       fileview);
        g_object_unref (fileview->priv->bookmark_mgr);
    }

    fileview->priv->bookmark_mgr = g_object_ref (mgr);
    _moo_bookmark_mgr_add_user (fileview->priv->bookmark_mgr, fileview,
                                fileview->priv->actions, fileview->priv->ui_xml,
                                "MooFileView/Toolbar/BookmarksMenu/Bookmarks");
    g_signal_connect (fileview->priv->bookmark_mgr,
                      "activate", G_CALLBACK (bookmark_activate),
                      fileview);

    _moo_bookmark_view_set_mgr (fileview->priv->bkview, mgr);

    g_object_set_data (G_OBJECT (fileview),
                       "moo-file-view-bookmarks-editor",
                       NULL);
    g_object_notify (G_OBJECT (fileview), "bookmark-mgr");
}


static void
view_bookmarks (MooFileView *fileview)
{
    MooFileViewType new_type;

    if (fileview->priv->view_type == MOO_FILE_VIEW_BOOKMARK)
        new_type = fileview->priv->file_view_type;
    else
        new_type = MOO_FILE_VIEW_BOOKMARK;

    _moo_file_view_set_view_type (fileview, new_type);
}

static void
view_files (MooFileView *fileview)
{
    if (fileview->priv->view_type == MOO_FILE_VIEW_BOOKMARK)
        _moo_file_view_set_view_type (fileview, fileview->priv->file_view_type);
}


static void
add_bookmark (MooFileView *fileview)
{
    const char *path;
    char *display_path;
    MooBookmark *bookmark;

    g_return_if_fail (fileview->priv->current_dir != NULL);

    path = _moo_folder_get_path (fileview->priv->current_dir);
    display_path = g_filename_display_name (path);
    bookmark = _moo_bookmark_new (display_path, path,
                                  GTK_STOCK_DIRECTORY);

    _moo_bookmark_mgr_add (fileview->priv->bookmark_mgr,
                           bookmark);

    _moo_bookmark_free (bookmark);
    g_free (display_path);
}


static void
edit_bookmarks (MooFileView *fileview)
{
    GtkWidget *dialog;

    dialog = g_object_get_data (G_OBJECT (fileview),
                                "moo-file-view-bookmarks-editor");

    if (!dialog)
    {
        dialog = _moo_bookmark_mgr_get_editor (fileview->priv->bookmark_mgr);
        gtk_object_sink (g_object_ref (dialog));
        g_object_set_data_full (G_OBJECT (fileview),
                                "moo-file-view-bookmarks-editor",
                                dialog, g_object_unref);
        g_signal_connect (dialog, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }

    moo_window_set_parent (dialog, GTK_WIDGET (fileview));
    gtk_window_present (GTK_WINDOW (dialog));
}


static void
bookmark_activate (G_GNUC_UNUSED MooBookmarkMgr *mgr,
                   MooBookmark    *bookmark,
                   MooFileView    *activated,
                   MooFileView    *fileview)
{
    if (activated != fileview)
        return;

    moo_file_view_chdir (fileview, bookmark->path, NULL);
}


static void
bookmark_activated (MooFileView    *fileview,
                    MooBookmark    *bookmark)
{
    g_return_if_fail (bookmark != NULL && bookmark->path != NULL);
    moo_file_view_chdir (fileview, bookmark->path, NULL);
}


/****************************************************************************/
/* Auxiliary stuff
 */

static void
file_view_move_selection (MooFileView    *fileview,
                          GtkTreeIter    *filter_iter)
{
    GtkTreePath *path = NULL;

    if (filter_iter)
    {
        path = gtk_tree_model_get_path (fileview->priv->filter_model,
                                        filter_iter);
        g_return_if_fail (path != NULL);
    }

    _moo_tree_view_unselect_all (fileview->priv->view);

    if (path)
    {
        _moo_tree_view_set_cursor (fileview->priv->view, path, FALSE);
        _moo_tree_view_scroll_to_cell (fileview->priv->view, path);
    }

    gtk_tree_path_free (path);
}


static void
file_view_select_iter (MooFileView *fileview,
                       GtkTreeIter *iter)
{
    if (iter)
    {
        GtkTreeIter filter_iter;
        GtkTreePath *filter_path, *path;

        path = gtk_tree_model_get_path (fileview->priv->model, iter);
        g_return_if_fail (path != NULL);

        filter_path = gtk_tree_model_filter_convert_child_path_to_path (
                GTK_TREE_MODEL_FILTER (fileview->priv->filter_model), path);

        if (!filter_path && _moo_tree_view_selection_is_empty (fileview->priv->view))
        {
            if (gtk_tree_model_get_iter_first (fileview->priv->filter_model, &filter_iter))
                file_view_move_selection (fileview, &filter_iter);
        }
        else if (filter_path)
        {
            gtk_tree_model_get_iter (fileview->priv->filter_model,
                                     &filter_iter, filter_path);
            file_view_move_selection (fileview, &filter_iter);
            gtk_tree_path_free (filter_path);
        }

        gtk_tree_path_free (path);
    }
    else
    {
        file_view_move_selection (fileview, NULL);
    }
}


void
_moo_file_view_select_name (MooFileView *fileview,
                            const char  *name)
{
    GtkTreeIter iter;
    MooFolderModel *model;

    if (!name)
    {
        file_view_select_iter (fileview, NULL);
        return;
    }

    model = MOO_FOLDER_MODEL (fileview->priv->model);

    if (_moo_folder_model_get_iter_by_name (model, name, &iter))
    {
        file_view_select_iter (fileview, &iter);
        return;
    }

    g_free (fileview->priv->select_file);
    fileview->priv->select_file = g_strdup (name);
}


void
_moo_file_view_select_display_name (MooFileView *fileview,
                                    const char  *name)
{
    GtkTreeIter iter;
    MooFolderModel *model;

    if (!name)
    {
        file_view_select_iter (fileview, NULL);
        return;
    }

    model = MOO_FOLDER_MODEL (fileview->priv->model);

    if (_moo_folder_model_get_iter_by_display_name (model, name, &iter))
    {
        file_view_select_iter (fileview, &iter);
        return;
    }

    g_free (fileview->priv->select_file);
    fileview->priv->select_file = g_filename_from_utf8 (name, -1, NULL, NULL, NULL);
}


static gboolean
do_select_name (MooFileView *fileview)
{
    GtkTreeIter iter;

    fileview->priv->select_file_idle = 0;

    if (!fileview->priv->select_file)
        return FALSE;

    if (_moo_folder_model_get_iter_by_name (MOO_FOLDER_MODEL (fileview->priv->model),
                                            fileview->priv->select_file, &iter))
    {
        g_free (fileview->priv->select_file);
        fileview->priv->select_file = NULL;
        file_view_select_iter (fileview, &iter);
    }

    return FALSE;
}


static void
file_added (MooFileView *fileview)
{
    if (fileview->priv->select_file && !fileview->priv->select_file_idle)
        fileview->priv->select_file_idle =
                g_idle_add ((GSourceFunc) do_select_name, fileview);
}


static const char*
get_selected_display_name (MooFileView *fileview)
{
    GtkTreeModel *model = fileview->priv->filter_model;
    GtkTreePath *filter_path;
    GtkTreeIter filter_iter;
    const char *name;
    MooFile *file = NULL;

    filter_path = _moo_tree_view_get_selected_path (fileview->priv->view);

    if (!filter_path)
        return NULL;

    gtk_tree_model_get_iter (model, &filter_iter, filter_path);
    gtk_tree_path_free (filter_path);

    gtk_tree_model_get (model, &filter_iter, COLUMN_FILE, &file, -1);
    g_return_val_if_fail (file != NULL, NULL);
    name = _moo_file_display_name (file);
    _moo_file_unref (file);

    return name;
}


/****************************************************************************/
/* Path entry
 */

enum {
    ENTRY_STATE_NONE        = 0,
    ENTRY_STATE_TYPEAHEAD   = 1,
    ENTRY_STATE_COMPLETION  = 2
};

static void     typeahead_create        (MooFileView    *fileview);
static void     typeahead_destroy       (MooFileView    *fileview);
static void     typeahead_try           (MooFileView    *fileview,
                                         gboolean        need_to_refilter);
static void     typeahead_tab_key       (MooFileView    *fileview);
static gboolean typeahead_stop_tab_cycle(MooFileView    *fileview);

static void     entry_changed           (GtkEntry       *entry,
                                         MooFileView    *fileview);
static gboolean entry_key_press         (GtkEntry       *entry,
                                         GdkEventKey    *event,
                                         MooFileView    *fileview);
static gboolean entry_focus_out         (GtkWidget      *entry,
                                         GdkEventFocus  *event,
                                         MooFileView    *fileview);
static void     entry_activate          (GtkEntry       *entry,
                                         MooFileView    *fileview);

static gboolean entry_stop_tab_cycle    (MooFileView    *fileview);
static gboolean entry_tab_key           (GtkEntry       *entry,
                                         MooFileView    *fileview);

static gboolean looks_like_path         (const char     *text);


static void
path_entry_init (MooFileView *fileview)
{
    GtkEntry *entry = fileview->priv->entry;

    /* XXX after? */
    g_signal_connect (entry, "changed",
                      G_CALLBACK (entry_changed), fileview);
    g_signal_connect (entry, "key-press-event",
                      G_CALLBACK (entry_key_press), fileview);
    g_signal_connect (entry, "focus-out-event",
                      G_CALLBACK (entry_focus_out), fileview);
    g_signal_connect (entry, "activate",
                      G_CALLBACK (entry_activate), fileview);

    typeahead_create (fileview);
}


static void
path_entry_deinit (MooFileView *fileview)
{
    typeahead_destroy (fileview);
}


static void
entry_changed (GtkEntry       *entry,
               MooFileView    *fileview)
{
    gboolean need_to_refilter = FALSE;

    const char *text = gtk_entry_get_text (entry);

    /*
    First, decide what's going on (doesn't look like something can be
    going on, but, anyway).
    Then:
    1) If text typed in is a beginning of some file in the list,
    select that file (like treeview interactive search).
    2) If text is a beginning of some hidden files (but not filtered
    out, those are always ignored), show those hidden files and
    select first of them.
    3) Otherwise, parse text as <path>/<file> (<file> may be empty),
    and do the entry completion stuff.

    Tab completion doesn't interfere with this code - tab completion sets
    entry text while this handler is blocked.
    */

    /* Check if some file was shown temporarily, and hide it */
    if (fileview->priv->temp_visible)
    {
        g_string_free (fileview->priv->temp_visible, TRUE);
        fileview->priv->temp_visible = NULL;
        need_to_refilter = TRUE;
    }

    if (!text[0])
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        file_view_move_selection (fileview, NULL);
        return;
    }

    /* TODO take ~file into account */
    /* If entered text looks like path, do completion stuff */
    if (looks_like_path (text))
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        file_view_move_selection (fileview, NULL);
        fileview->priv->entry_state = ENTRY_STATE_COMPLETION;
        /* XXX call complete() or something, for automatic popup */
        return;
    }
    else
    {
        fileview->priv->entry_state = ENTRY_STATE_TYPEAHEAD;
        return typeahead_try (fileview, need_to_refilter);
    }
}


static gboolean
entry_key_press (GtkEntry       *entry,
                 GdkEventKey    *event,
                 MooFileView    *fileview)
{
    GtkWidget *filewidget;
    const char *name;

    if (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK))
        return FALSE;

    if (event->keyval != GDK_Tab)
    {
        if (entry_stop_tab_cycle (fileview))
            g_signal_emit_by_name (entry, "changed");
    }

    switch (event->keyval)
    {
        case GDK_Escape:
            stop_path_entry (fileview, TRUE);
            return TRUE;

        case GDK_Up:
        case GDK_KP_Up:
        case GDK_Down:
        case GDK_KP_Down:
            filewidget = get_view_widget (fileview);
            GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS (filewidget))->
                    key_press_event (filewidget, event);
            name = get_selected_display_name (fileview);
            g_return_val_if_fail (name != NULL, TRUE);
            path_entry_set_text (fileview, name);
            gtk_editable_set_position (GTK_EDITABLE (entry), -1);
            return TRUE;

        case GDK_Tab:
            return entry_tab_key (entry, fileview);

        default:
            return FALSE;
    }
}


static gboolean
entry_stop_tab_cycle (MooFileView *fileview)
{
    switch (fileview->priv->entry_state)
    {
        case ENTRY_STATE_TYPEAHEAD:
            return typeahead_stop_tab_cycle (fileview);
        case ENTRY_STATE_COMPLETION:
            fileview->priv->entry_state = 0;
            return FALSE;
        default:
            return FALSE;
    }
}


static gboolean
entry_tab_key (GtkEntry       *entry,
               MooFileView    *fileview)
{
    const char *text;

    if (!fileview->priv->entry_state)
    {
        text = gtk_entry_get_text (entry);

        if (text[0])
        {
            g_signal_emit_by_name (entry, "changed");
            g_return_val_if_fail (fileview->priv->entry_state != 0, FALSE);
            return entry_tab_key (entry, fileview);
        }
        else
        {
            return FALSE;
        }
    }

    if (fileview->priv->entry_state == ENTRY_STATE_TYPEAHEAD)
    {
        typeahead_tab_key (fileview);
        return TRUE;
    }

    return FALSE;
}


static void
path_entry_set_text (MooFileView    *fileview,
                     const char     *text)
{
    GtkEntry *entry = fileview->priv->entry;
    g_signal_handlers_block_by_func (entry, (gpointer) entry_changed,
                                     fileview);
    gtk_entry_set_text (entry, text);
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    g_signal_handlers_unblock_by_func (entry, (gpointer) entry_changed,
                                       fileview);
}


static void
path_entry_delete_to_cursor (MooFileView *fileview)
{
    GtkEditable *entry = GTK_EDITABLE (fileview->priv->entry);
    if (gtk_widget_is_focus (GTK_WIDGET (entry)))
        gtk_editable_delete_text (entry, 0,
                                  gtk_editable_get_position (entry));
}


static void
entry_activate (GtkEntry       *entry,
                MooFileView    *fileview)
{
    char *filename = NULL;
    GtkTreePath *selected;

    selected = _moo_tree_view_get_selected_path (fileview->priv->view);

    if (selected)
    {
        GtkTreeIter iter;
        MooFile *file = NULL;
        gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, selected);
        gtk_tree_model_get (fileview->priv->filter_model, &iter, COLUMN_FILE, &file, -1);
        gtk_tree_path_free (selected);
        g_return_if_fail (file != NULL);

        /* XXX display_name() */
        filename = g_strdup (_moo_file_display_name (file));
        _moo_file_unref (file);
    }
    else
    {
        filename = g_strdup (gtk_entry_get_text (entry));
    }

    stop_path_entry (fileview, TRUE);
    file_view_activate_filename (fileview, filename);
    g_free (filename);
}


#if 0
#define PRINT_KEY_EVENT(event)                                      \
    g_print ("%s%s%s%s\n",                                          \
             event->state & GDK_SHIFT_MASK ? "<Shift>" : "",        \
             event->state & GDK_CONTROL_MASK ? "<Control>" : "",    \
             event->state & GDK_MOD1_MASK ? "<Alt>" : "",           \
             gdk_keyval_name (event->keyval))
#else
#define PRINT_KEY_EVENT(event)
#endif


static gboolean
moo_file_view_key_press (MooFileView    *fileview,
                         GtkWidget      *widget,
                         GdkEventKey    *event)
{
    if (fileview->priv->entry_state)
    {
        g_warning ("%s: something wrong", G_STRLOC);
        stop_path_entry (fileview, FALSE);
        return FALSE;
    }

    /* return immediately if event doesn't look like text typed in */

    if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
        return FALSE;

    switch (event->keyval)
    {
        case GDK_VoidSymbol:
        case GDK_BackSpace:
        case GDK_Tab:
        case GDK_Linefeed:
        case GDK_Clear:
        case GDK_Return:
        case GDK_Pause:
        case GDK_Scroll_Lock:
        case GDK_Sys_Req:
        case GDK_Escape:
        case GDK_Delete:
        case GDK_Multi_key:
        case GDK_Codeinput:
        case GDK_SingleCandidate:
        case GDK_MultipleCandidate:
        case GDK_PreviousCandidate:
        case GDK_Kanji:
        case GDK_Muhenkan:
        case GDK_Henkan_Mode:
        case GDK_Romaji:
        case GDK_Hiragana:
        case GDK_Katakana:
        case GDK_Hiragana_Katakana:
        case GDK_Zenkaku:
        case GDK_Hankaku:
        case GDK_Zenkaku_Hankaku:
        case GDK_Touroku:
        case GDK_Massyo:
        case GDK_Kana_Lock:
        case GDK_Kana_Shift:
        case GDK_Eisu_Shift:
        case GDK_Eisu_toggle:
        case GDK_Home:
        case GDK_Left:
        case GDK_Up:
        case GDK_Right:
        case GDK_Down:
        case GDK_Page_Up:
        case GDK_Page_Down:
        case GDK_End:
        case GDK_Begin:
        case GDK_Select:
        case GDK_Print:
        case GDK_Execute:
        case GDK_Insert:
        case GDK_Undo:
        case GDK_Redo:
        case GDK_Menu:
        case GDK_Find:
        case GDK_Cancel:
        case GDK_Help:
        case GDK_Break:
        case GDK_Mode_switch:
        case GDK_Num_Lock:
        case GDK_KP_Tab:
        case GDK_KP_Enter:
        case GDK_KP_F1:
        case GDK_KP_F2:
        case GDK_KP_F3:
        case GDK_KP_F4:
        case GDK_KP_Home:
        case GDK_KP_Left:
        case GDK_KP_Up:
        case GDK_KP_Right:
        case GDK_KP_Down:
        case GDK_KP_Page_Up:
        case GDK_KP_Page_Down:
        case GDK_KP_End:
        case GDK_KP_Begin:
        case GDK_KP_Insert:
        case GDK_KP_Delete:
        case GDK_F1:
        case GDK_F2:
        case GDK_F3:
        case GDK_F4:
        case GDK_F5:
        case GDK_F6:
        case GDK_F7:
        case GDK_F8:
        case GDK_F9:
        case GDK_F10:
        case GDK_F11:
        case GDK_F12:
        case GDK_F13:
        case GDK_F14:
        case GDK_F15:
        case GDK_F16:
        case GDK_F17:
        case GDK_F18:
        case GDK_F19:
        case GDK_F20:
        case GDK_F21:
        case GDK_F22:
        case GDK_F23:
        case GDK_F24:
        case GDK_F25:
        case GDK_F26:
        case GDK_F27:
        case GDK_F28:
        case GDK_F29:
        case GDK_F30:
        case GDK_F31:
        case GDK_F32:
        case GDK_F33:
        case GDK_F34:
        case GDK_F35:
        case GDK_Shift_L:
        case GDK_Shift_R:
        case GDK_Control_L:
        case GDK_Control_R:
        case GDK_Caps_Lock:
        case GDK_Shift_Lock:
        case GDK_Meta_L:
        case GDK_Meta_R:
        case GDK_Alt_L:
        case GDK_Alt_R:
        case GDK_Super_L:
        case GDK_Super_R:
        case GDK_Hyper_L:
        case GDK_Hyper_R:
        case GDK_ISO_Lock:
        case GDK_ISO_Level2_Latch:
        case GDK_ISO_Level3_Shift:
        case GDK_ISO_Level3_Latch:
        case GDK_ISO_Level3_Lock:
        case GDK_ISO_Group_Latch:
        case GDK_ISO_Group_Lock:
        case GDK_ISO_Next_Group:
        case GDK_ISO_Next_Group_Lock:
        case GDK_ISO_Prev_Group:
        case GDK_ISO_Prev_Group_Lock:
        case GDK_ISO_First_Group:
        case GDK_ISO_First_Group_Lock:
        case GDK_ISO_Last_Group:
        case GDK_ISO_Last_Group_Lock:
        case GDK_ISO_Left_Tab:
        case GDK_ISO_Move_Line_Up:
        case GDK_ISO_Move_Line_Down:
        case GDK_ISO_Partial_Line_Up:
        case GDK_ISO_Partial_Line_Down:
        case GDK_ISO_Partial_Space_Left:
        case GDK_ISO_Partial_Space_Right:
        case GDK_ISO_Set_Margin_Left:
        case GDK_ISO_Set_Margin_Right:
        case GDK_ISO_Release_Margin_Left:
        case GDK_ISO_Release_Margin_Right:
        case GDK_ISO_Release_Both_Margins:
        case GDK_ISO_Fast_Cursor_Left:
        case GDK_ISO_Fast_Cursor_Right:
        case GDK_ISO_Fast_Cursor_Up:
        case GDK_ISO_Fast_Cursor_Down:
        case GDK_ISO_Continuous_Underline:
        case GDK_ISO_Discontinuous_Underline:
        case GDK_ISO_Emphasize:
        case GDK_ISO_Center_Object:
        case GDK_ISO_Enter:
        case GDK_First_Virtual_Screen:
        case GDK_Prev_Virtual_Screen:
        case GDK_Next_Virtual_Screen:
        case GDK_Last_Virtual_Screen:
        case GDK_Terminate_Server:
        case GDK_AccessX_Enable:
        case GDK_AccessX_Feedback_Enable:
        case GDK_RepeatKeys_Enable:
        case GDK_SlowKeys_Enable:
        case GDK_BounceKeys_Enable:
        case GDK_StickyKeys_Enable:
        case GDK_MouseKeys_Enable:
        case GDK_MouseKeys_Accel_Enable:
        case GDK_Overlay1_Enable:
        case GDK_Overlay2_Enable:
        case GDK_AudibleBell_Enable:
        case GDK_Pointer_Left:
        case GDK_Pointer_Right:
        case GDK_Pointer_Up:
        case GDK_Pointer_Down:
        case GDK_Pointer_UpLeft:
        case GDK_Pointer_UpRight:
        case GDK_Pointer_DownLeft:
        case GDK_Pointer_DownRight:
        case GDK_Pointer_Button_Dflt:
        case GDK_Pointer_Button1:
        case GDK_Pointer_Button2:
        case GDK_Pointer_Button3:
        case GDK_Pointer_Button4:
        case GDK_Pointer_Button5:
        case GDK_Pointer_DblClick_Dflt:
        case GDK_Pointer_DblClick1:
        case GDK_Pointer_DblClick2:
        case GDK_Pointer_DblClick3:
        case GDK_Pointer_DblClick4:
        case GDK_Pointer_DblClick5:
        case GDK_Pointer_Drag_Dflt:
        case GDK_Pointer_Drag1:
        case GDK_Pointer_Drag2:
        case GDK_Pointer_Drag3:
        case GDK_Pointer_Drag4:
        case GDK_Pointer_Drag5:
        case GDK_Pointer_EnableKeys:
        case GDK_Pointer_Accelerate:
        case GDK_Pointer_DfltBtnNext:
        case GDK_Pointer_DfltBtnPrev:
        case GDK_3270_Duplicate:
        case GDK_3270_FieldMark:
        case GDK_3270_Right2:
        case GDK_3270_Left2:
        case GDK_3270_BackTab:
        case GDK_3270_EraseEOF:
        case GDK_3270_EraseInput:
        case GDK_3270_Reset:
        case GDK_3270_Quit:
        case GDK_3270_PA1:
        case GDK_3270_PA2:
        case GDK_3270_PA3:
        case GDK_3270_Test:
        case GDK_3270_Attn:
        case GDK_3270_CursorBlink:
        case GDK_3270_AltCursor:
        case GDK_3270_KeyClick:
        case GDK_3270_Jump:
        case GDK_3270_Ident:
        case GDK_3270_Rule:
        case GDK_3270_Copy:
        case GDK_3270_Play:
        case GDK_3270_Setup:
        case GDK_3270_Record:
        case GDK_3270_ChangeScreen:
        case GDK_3270_DeleteWord:
        case GDK_3270_ExSelect:
        case GDK_3270_CursorSelect:
        case GDK_3270_PrintScreen:
        case GDK_3270_Enter:
            return FALSE;
    }

    PRINT_KEY_EVENT (event);

    if (GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS (widget))->key_press_event (widget, event))
        return TRUE;

    if (GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS (fileview))->key_press_event (widget, event))
        return TRUE;

    if (event->string && event->length)
    {
        GdkEvent *copy;
        GtkWidget *entry = GTK_WIDGET (fileview->priv->entry);

        g_return_val_if_fail (event != NULL, FALSE);
        g_return_val_if_fail (GTK_WIDGET_REALIZED (entry), FALSE);

        copy = gdk_event_copy ((GdkEvent*) event);
        g_object_unref (copy->key.window);
        copy->key.window = g_object_ref (entry->window);

        gtk_widget_grab_focus (entry);

        path_entry_set_text (fileview, "");
        gtk_widget_event (entry, copy);

        gdk_event_free (copy);
        return TRUE;
    }

    return FALSE;
}


static gboolean
entry_focus_out (GtkWidget      *entry,
                 G_GNUC_UNUSED GdkEventFocus *event,
                 MooFileView    *fileview)
{
    /* focus may be lost due to switching to other window, do nothing then */
    if (!gtk_widget_is_focus (entry))
        stop_path_entry (fileview, TRUE);
    return FALSE;
}


static void
stop_path_entry (MooFileView    *fileview,
                 gboolean        focus_file_list)
{
    char *text;

    typeahead_stop_tab_cycle (fileview);

    fileview->priv->entry_state = 0;

    if (fileview->priv->current_dir)
        text = g_filename_display_name (_moo_folder_get_path (fileview->priv->current_dir));
    else
        text = g_strdup ("");

    path_entry_set_text (fileview, text);

    if (focus_file_list)
        focus_to_file_view (fileview);

    g_free (text);
}


/* WIN32_XXX */
static void
file_view_activate_filename (MooFileView    *fileview,
                             const char     *display_name)
{
    GError *error = NULL;
    char *dirname, *basename;
    char *path = NULL;
    const char *current_dir = NULL;

    if (fileview->priv->current_dir)
        current_dir = _moo_folder_get_path (fileview->priv->current_dir);

    path = _moo_file_system_get_absolute_path (fileview->priv->file_system,
                                               display_name, current_dir);

    if (!path || !g_file_test (path, G_FILE_TEST_EXISTS))
    {
        g_free (path);
        return;
    }

    if (g_file_test (path, G_FILE_TEST_IS_DIR))
    {
        if (!moo_file_view_chdir (fileview, path, &error))
        {
            g_warning ("%s: could not chdir to %s",
                       G_STRLOC, path);
            if (error)
            {
                g_warning ("%s: %s", G_STRLOC, error->message);
                g_error_free (error);
            }
        }

        g_free (path);
        return;
    }

    dirname = g_path_get_dirname (path);
    basename = g_path_get_basename (path);

    if (!dirname || !basename)
    {
        g_free (path);
        g_free (dirname);
        g_free (basename);
        g_return_if_reached ();
    }

    if (!moo_file_view_chdir (fileview, dirname, &error))
    {
        g_warning ("%s: could not chdir to %s",
                    G_STRLOC, dirname);

        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }

        g_free (path);
        g_free (dirname);
        g_free (basename);
        return;
    }

    _moo_file_view_select_name (fileview, basename);
    g_signal_emit (fileview, signals[ACTIVATE], 0, path);

    g_free (path);
    g_free (dirname);
    g_free (basename);
}


static gboolean
looks_like_path (const char *text)
{
    if (strchr (text, '/'))
        return TRUE;
#ifdef __WIN32__
    else if (strchr (text, '\\'))
        return TRUE;
#endif
    else
        return FALSE;
}


/*********************************************************************/
/* Typeahead
 */

struct _Typeahead {
    MooFileView *fileview;
    GtkEntry *entry;
    GString *matched_prefix;
    int matched_prefix_char_len;
    TextFuncs text_funcs;
    gboolean case_sensitive;
};

static gboolean typeahead_find_match_visible    (MooFileView    *fileview,
                                                 const char     *text,
                                                 GtkTreeIter    *filter_iter,
                                                 gboolean        exact_match);
static gboolean typeahead_find_match_hidden     (MooFileView    *fileview,
                                                 const char     *text,
                                                 GtkTreeIter    *iter,
                                                 gboolean        exact_match);


static void
typeahead_try (MooFileView *fileview,
               gboolean     need_to_refilter)
{
    const char *text;
    Typeahead *stuff = fileview->priv->typeahead;
    GtkEntry *entry = stuff->entry;
    GtkTreeIter filter_iter, iter;

    g_assert (fileview->priv->entry_state == ENTRY_STATE_TYPEAHEAD);

    if (stuff->matched_prefix)
    {
        g_string_free (stuff->matched_prefix, TRUE);
        stuff->matched_prefix = NULL;
    }

    text = gtk_entry_get_text (entry);
    g_assert (text[0] != 0);

    /* TODO windows */

    if (fileview->priv->show_hidden_files || text[0] != '.' || !text[1])
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        if (typeahead_find_match_visible (fileview, text, &filter_iter, FALSE))
            file_view_move_selection (fileview, &filter_iter);
        else
            file_view_move_selection (fileview, NULL);

        return;
    }

    /* check if partial name of hidden file was typed in */
    if (typeahead_find_match_hidden (fileview, text, &iter, FALSE))
    {
        fileview->priv->temp_visible = g_string_new (text);
        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                (fileview->priv->filter_model));

        gtk_tree_model_filter_convert_child_iter_to_iter (
                GTK_TREE_MODEL_FILTER (fileview->priv->filter_model),
        &filter_iter, &iter);
        file_view_move_selection (fileview, &filter_iter);
    }
    else
    {
        if (need_to_refilter)
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                    (fileview->priv->filter_model));

        file_view_move_selection (fileview, NULL);
    }
}


static void
typeahead_tab_key (MooFileView *fileview)
{
    const char *text;
    Typeahead *stuff = fileview->priv->typeahead;
    GtkEntry *entry = stuff->entry;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath *path;
    MooFile *file = NULL;
    const char *name;

    g_assert (fileview->priv->entry_state == ENTRY_STATE_TYPEAHEAD);

    model = fileview->priv->filter_model;

    if (!gtk_tree_model_get_iter_first (model, &iter))
        return;

    /* see if it's cycling over matched names */
    if (stuff->matched_prefix)
    {
        gboolean found = FALSE;

        path = _moo_tree_view_get_selected_path (fileview->priv->view);

        /* check if there is next name in the list to jump to */
        if (path && gtk_tree_model_get_iter (model, &iter, path) &&
            gtk_tree_model_iter_next (model, &iter))
        {
            found = model_find_next_match (model, &iter,
                                           stuff->matched_prefix->str,
                                           stuff->matched_prefix->len,
                                           &stuff->text_funcs,
                                           FALSE);
        }

        /* if nothing found, start cycling again */
        if (!found)
            found = typeahead_find_match_visible (fileview,
                                                  stuff->matched_prefix->str,
                                                  &iter, FALSE);

        gtk_tree_path_free (path);

        if (!found)
            goto error;
        else
            file_view_move_selection (fileview, &iter);
    }
    else
    {
        gboolean unique;

        text = gtk_entry_get_text (entry);

        if (!text[0])
            return;

        stuff->matched_prefix =
                model_find_max_prefix (fileview->priv->filter_model,
                                       text, &stuff->text_funcs, &unique, &iter);

        if (!stuff->matched_prefix)
            return;

        stuff->matched_prefix_char_len =
                g_utf8_strlen (stuff->matched_prefix->str, stuff->matched_prefix->len);

        /* if match is unique and it's a directory, append a slash */
        if (unique)
        {
            file = NULL;
            gtk_tree_model_get (model, &iter, COLUMN_FILE, &file, -1);

            if (!file)
                goto error;

            name = _moo_file_display_name (file);

//             if (!file || stuff->strcmp_func (stuff->matched_prefix->str, file))
//                 goto error;

            if (MOO_FILE_IS_DIR (file))
            {
                char *new_text = g_strdup_printf ("%s%c", name, G_DIR_SEPARATOR);
                g_string_free (stuff->matched_prefix, TRUE);
                stuff->matched_prefix = NULL;
                path_entry_set_text (fileview, new_text);
                g_free (new_text);
                _moo_file_unref (file);
                g_signal_emit_by_name (entry, "changed");
                return;
            }

            _moo_file_unref (file);
            file = NULL;
        }

        path = _moo_tree_view_get_selected_path (fileview->priv->view);

        if (!path && !typeahead_find_match_visible (fileview, stuff->matched_prefix->str, &iter, FALSE))
            goto error;

        if (path)
            gtk_tree_model_get_iter (model, &iter, path);

        gtk_tree_path_free (path);
    }

    gtk_tree_model_get (model, &iter, COLUMN_FILE, &file, -1);

    if (!file)
        goto error;

//     if (stuff->strncmp_func (stuff->matched_prefix->str, file, stuff->matched_prefix->len))
//         goto error;

    name = _moo_file_display_name (file);
    path_entry_set_text (fileview, name);
    gtk_editable_select_region (GTK_EDITABLE (entry),
                                stuff->matched_prefix_char_len,
                                -1);

    _moo_file_unref (file);
    return;

error:
    if (stuff->matched_prefix)
        g_string_free (stuff->matched_prefix, TRUE);
    stuff->matched_prefix = NULL;
    _moo_file_unref (file);
    g_return_if_reached ();
}


static gboolean
typeahead_stop_tab_cycle (MooFileView *fileview)
{
    Typeahead *stuff = fileview->priv->typeahead;

    if (stuff->matched_prefix)
    {
        g_string_free (stuff->matched_prefix, TRUE);
        stuff->matched_prefix = NULL;
        stuff->matched_prefix_char_len = 0;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


static void
typeahead_create (MooFileView *fileview)
{
    Typeahead *stuff = g_new0 (Typeahead, 1);

    stuff->fileview = fileview;
    stuff->entry = fileview->priv->entry;
    stuff->case_sensitive = fileview->priv->typeahead_case_sensitive;

    if (stuff->case_sensitive)
    {
        stuff->text_funcs.strcmp_func = strcmp_func;
        stuff->text_funcs.strncmp_func = strncmp_func;
        stuff->text_funcs.normalize_func = normalize_func;
    }
    else
    {
        stuff->text_funcs.strcmp_func = case_strcmp_func;
        stuff->text_funcs.strncmp_func = case_strncmp_func;
        stuff->text_funcs.normalize_func = case_normalize_func;
    }

    fileview->priv->typeahead = stuff;
}


static void
typeahead_destroy (MooFileView *fileview)
{
    Typeahead *stuff = fileview->priv->typeahead;
    if (stuff->matched_prefix)
        g_string_free (stuff->matched_prefix, TRUE);
    g_free (stuff);
    fileview->priv->typeahead = NULL;
}


void
_moo_file_view_set_typeahead_case_sensitive (MooFileView *fileview,
                                             gboolean     case_sensitive)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    if (case_sensitive != fileview->priv->typeahead_case_sensitive)
    {
        Typeahead *stuff = fileview->priv->typeahead;

        fileview->priv->typeahead_case_sensitive = case_sensitive;
        stuff->case_sensitive = case_sensitive;

        if (case_sensitive)
        {
            stuff->text_funcs.strcmp_func = strcmp_func;
            stuff->text_funcs.strncmp_func = strncmp_func;
            stuff->text_funcs.normalize_func = normalize_func;
        }
        else
        {
            stuff->text_funcs.strcmp_func = case_strcmp_func;
            stuff->text_funcs.strncmp_func = case_strncmp_func;
            stuff->text_funcs.normalize_func = case_normalize_func;
        }

        g_object_notify (G_OBJECT (fileview), "typeahead-case-sensitive");
    }
}


static gboolean
typeahead_find_match_visible (MooFileView    *fileview,
                              const char     *text,
                              GtkTreeIter    *filter_iter,
                              gboolean        exact_match)
{
    guint len;

    g_return_val_if_fail (text && text[0], FALSE);

    if (!gtk_tree_model_get_iter_first (fileview->priv->filter_model, filter_iter))
        return FALSE;

    len = strlen (text);

    return model_find_next_match (fileview->priv->filter_model,
                                  filter_iter, text, len,
                                  &fileview->priv->typeahead->text_funcs,
                                  exact_match);
}


static gboolean
typeahead_find_match_hidden (MooFileView    *fileview,
                             const char     *text,
                             GtkTreeIter    *iter,
                             gboolean        exact)
{
    guint len;
    GtkTreeModel *model = fileview->priv->model;

    g_return_val_if_fail (text && text[0], FALSE);

    if (!gtk_tree_model_get_iter_first (model, iter))
        return FALSE;

    len = strlen (text);

    while (TRUE)
    {
        MooFile *file = NULL;

        if (!model_find_next_match (model, iter, text, len,
                                    &fileview->priv->typeahead->text_funcs,
                                    exact))
            return FALSE;

        gtk_tree_model_get (model, iter, COLUMN_FILE, &file, -1);

        if (file && moo_file_view_check_visible (fileview, file, TRUE, TRUE))
        {
            _moo_file_unref (file);
            return TRUE;
        }

        _moo_file_unref (file);

        if (!gtk_tree_model_iter_next (model, iter))
            return FALSE;
    }
}


/***************************************************************************/
/* Drag source
 */

static void
icon_drag_begin (MooFileView    *fileview,
                 G_GNUC_UNUSED GdkDragContext *context,
                 G_GNUC_UNUSED MooIconView *iconview)
{
    GList *files, *l;
    char **uris;
    guint n_files, i;
    MooFolder *folder;

    folder = fileview->priv->current_dir;
    g_return_if_fail (folder != NULL);

    files = file_view_get_selected_files (fileview);
    g_return_if_fail (files != NULL);

    n_files = g_list_length (files);
    uris = g_new0 (char*, n_files + 1);

    for (l = files, i = 0; l != NULL; i++, l = l->next)
    {
        MooFile *file = l->data;
        uris[i] = _moo_folder_get_file_uri (folder, file);
    }

    g_object_set_data_full (G_OBJECT (fileview), "moo-file-view-source-uris",
                            uris, (GDestroyNotify) g_strfreev);
    g_object_set_data_full (G_OBJECT (fileview), "moo-file-view-source-dir",
                            g_object_ref (folder), g_object_unref);

    g_list_foreach (files, (GFunc) _moo_file_unref, NULL);
    g_list_free (files);
}


static void
icon_drag_data_get (MooFileView    *fileview,
                    G_GNUC_UNUSED GdkDragContext *context,
                    GtkSelectionData *data,
                    G_GNUC_UNUSED guint           info,
                    G_GNUC_UNUSED guint           time,
                    G_GNUC_UNUSED MooIconView    *iconview)
{
    char **uris;

    uris = g_object_get_data (G_OBJECT (fileview), "moo-file-view-source-uris");
    g_return_if_fail (uris && *uris);

    gtk_selection_data_set_uris (data, uris);
}


static void
icon_drag_end (MooFileView    *fileview,
               G_GNUC_UNUSED GdkDragContext *context,
               G_GNUC_UNUSED MooIconView    *iconview)
{
    g_object_set_data (G_OBJECT (fileview), "moo-file-view-source-uris", NULL);
    g_object_set_data (G_OBJECT (fileview), "moo-file-view-source-dir", NULL);
}


/***************************************************************************/
/* Drag destination
 */

#define DROP_OPEN_TIMEOUT 500
#define DROP_OPEN_BLINK_TIME 80


static gboolean
drop_open_timeout_func2 (MooFileView *fileview)
{
    GtkTreeRowReference *ref;
    GtkWidget *button;

    ref = fileview->priv->drop_to.row;
    fileview->priv->drop_to.row = NULL;
    button = fileview->priv->drop_to.button;

    cancel_drop_open (fileview);

    if (ref)
    {
        MooFile *file = NULL;
        const char *goto_dir = NULL;
        GtkTreePath *path;
        MooFolder *current_dir;
        GtkTreeIter iter;

        path = gtk_tree_row_reference_get_path (ref);
        gtk_tree_row_reference_free (ref);

        g_return_val_if_fail (path != NULL, FALSE);

        if (!gtk_tree_model_get_iter (fileview->priv->filter_model, &iter, path))
        {
            gtk_tree_path_free (path);
            g_return_val_if_reached (FALSE);
        }

        gtk_tree_path_free (path);

        current_dir = fileview->priv->current_dir;
        g_return_val_if_fail (current_dir != NULL, FALSE);

        gtk_tree_model_get (fileview->priv->filter_model,
                            &iter, COLUMN_FILE, &file, -1);
        g_return_val_if_fail (file != NULL, FALSE);

        if (!strcmp (_moo_file_name (file), "..") || MOO_FILE_IS_DIR (file))
        {
            goto_dir = _moo_file_name (file);
        }
        else
        {
            g_warning ("%s: oops", G_STRLOC);
        }

        if (goto_dir && moo_file_view_chdir (fileview, goto_dir, NULL))
            _moo_file_view_select_name (fileview, NULL);

        if (file)
            _moo_file_unref (file);
    }
    else if (button)
    {
        g_signal_emit_by_name (button, "clicked");
        _moo_file_view_select_name (fileview, NULL);
    }
    else
    {
        g_return_val_if_reached (FALSE);
    }

    return FALSE;
}


static gboolean
drop_open_timeout_func (MooFileView *fileview)
{
    fileview->priv->drop_to.timeout = 0;

    if (fileview->priv->drop_to.row)
    {
        GtkTreePath *path;

        path = gtk_tree_row_reference_get_path (fileview->priv->drop_to.row);

        if (!path)
        {
            cancel_drop_open (fileview);
            _moo_icon_view_set_drag_dest_row (fileview->priv->iconview, NULL);
            g_return_val_if_reached (FALSE);
        }

        if (fileview->priv->drop_to.blink_clear)
        {
            fileview->priv->drop_to.blink_clear = FALSE;
            _moo_icon_view_set_drag_dest_row (fileview->priv->iconview, path);

            if (++fileview->priv->drop_to.n_blinks > 1)
                fileview->priv->drop_to.timeout =
                        g_timeout_add (DROP_OPEN_BLINK_TIME,
                                       (GSourceFunc) drop_open_timeout_func2,
                                       fileview);
        }
        else
        {
            fileview->priv->drop_to.blink_clear = TRUE;
            _moo_icon_view_set_drag_dest_row (fileview->priv->iconview, NULL);
        }
    }
    else if (fileview->priv->drop_to.button)
    {
        if (!fileview->priv->drop_to.highlight)
        {
            fileview->priv->drop_to.highlight = TRUE;
            gtk_drag_highlight (fileview->priv->drop_to.button);

            if (++fileview->priv->drop_to.n_blinks > 1)
                fileview->priv->drop_to.timeout =
                        g_timeout_add (DROP_OPEN_BLINK_TIME,
                                       (GSourceFunc) drop_open_timeout_func2,
                                       fileview);
        }
        else
        {
            fileview->priv->drop_to.highlight = FALSE;
            gtk_drag_unhighlight (fileview->priv->drop_to.button);
        }
    }
    else
    {
        g_return_val_if_reached (FALSE);
    }

    if (!fileview->priv->drop_to.timeout)
        fileview->priv->drop_to.timeout =
                g_timeout_add (DROP_OPEN_BLINK_TIME,
                               (GSourceFunc) drop_open_timeout_func,
                               fileview);

    return FALSE;
}


static void
drag_data_received (MooFileView    *fileview,
                    GdkDragContext *context,
                    int             x,
                    int             y,
                    GtkSelectionData *data,
                    guint           info,
                    guint           time,
                    MooIconView    *view)
{
    gboolean dummy;
    char *dir = NULL;
    GtkTreePath *path = NULL;
    MooFolder *current_dir;

    cancel_drop_open (fileview);

    current_dir = fileview->priv->current_dir;

    if (!current_dir)
    {
        g_critical ("%s: oops", G_STRLOC);
        _moo_file_view_drag_finish (fileview, context, FALSE, FALSE, time);
        return;
    }

    if (_moo_icon_view_get_path_at_pos (view, x, y, &path, NULL, NULL, NULL))
    {
        MooFile *file = file_view_get_file_at_path (fileview, path);

        if (MOO_FILE_IS_DIR (file))
        {
            dir = _moo_folder_get_file_path (current_dir, file);
            _moo_icon_view_set_drag_dest_row (view, path);
        }

        _moo_file_unref (file);
    }

    if (!dir)
        dir = g_strdup (_moo_folder_get_path (current_dir));

    g_signal_emit (fileview, signals[DROP_DATA_RECEIVED], 0,
                   dir, view, context, x, y, data, info, time, &dummy);

    gtk_tree_path_free (path);
    g_free (dir);
}


static gboolean
drag_drop (MooFileView    *fileview,
           GdkDragContext *context,
           int             x,
           int             y,
           guint           time,
           MooIconView    *view)
{
    gboolean dummy;

    char *dir = NULL;
    GtkTreePath *path = NULL;
    MooFolder *current_dir;

    cancel_drop_open (fileview);

    current_dir = fileview->priv->current_dir;

    if (!current_dir)
    {
        g_critical ("%s: oops", G_STRLOC);
        _moo_file_view_drag_finish (fileview, context, FALSE, FALSE, time);
        return FALSE;
    }

    if (_moo_icon_view_get_path_at_pos (view, x, y, &path, NULL, NULL, NULL))
    {
        MooFile *file = file_view_get_file_at_path (fileview, path);

        if (MOO_FILE_IS_DIR (file))
        {
            dir = _moo_folder_get_file_path (current_dir, file);
            _moo_icon_view_set_drag_dest_row (view, path);
        }

        _moo_file_unref (file);
    }

    if (!dir)
        dir = g_strdup (_moo_folder_get_path (current_dir));

    g_signal_emit (fileview, signals[DROP], 0, dir, view, context, x, y, time, &dummy);

    gtk_tree_path_free (path);
    g_free (dir);
    return TRUE;
}


static gboolean
moo_file_view_drop (MooFileView    *fileview,
                    G_GNUC_UNUSED const char *path,
                    GtkWidget      *widget,
                    GdkDragContext *context,
                    G_GNUC_UNUSED int x,
                    G_GNUC_UNUSED int y,
                    guint           time)
{
    GdkAtom target;

    target = gtk_drag_dest_find_target (widget, context, fileview->priv->targets);

    if (target != GDK_NONE)
    {
        gtk_drag_get_data (widget, context, target, time);
        return TRUE;
    }
    else
    {
        g_warning ("%s: oops", G_STRLOC);
        return FALSE;
    }
}


static gboolean
moo_file_view_drop_data_received (MooFileView    *fileview,
                                  const char     *path,
                                  GtkWidget      *widget,
                                  GdkDragContext *context,
                                  int             x,
                                  int             y,
                                  GtkSelectionData *data,
                                  G_GNUC_UNUSED guint info,
                                  guint           time)
{
    gboolean success = FALSE;
    gboolean delete = FALSE;

    if (data->target == gdk_atom_intern ("text/uri-list", FALSE))
    {
        char **uris = gtk_selection_data_get_uris (data);

        if (uris)
        {
            /* it has to call drag_finish in order to be able to show menu */
            moo_file_view_drop_uris (fileview, uris, path, widget,
                                     context, x, y, time);
            g_strfreev (uris);
            return TRUE;
        }
        else
        {
            g_critical ("%s: oops", G_STRLOC);
        }

        g_strfreev (uris);
    }
    else
    {
        char *text = (char*) gtk_selection_data_get_text (data);

        delete = context->suggested_action & GDK_ACTION_MOVE;

        if (text)
            success = moo_file_view_drop_text (fileview, text, path, widget,
                                               context, x, y, time, &delete);
        else
            g_critical ("%s: oops", G_STRLOC);

        g_free (text);
    }

    _moo_file_view_drag_finish (fileview, context, success, delete, time);
    return TRUE;
}


void
_moo_file_view_drag_finish (MooFileView    *fileview,
                            GdkDragContext *context,
                            gboolean        success,
                            gboolean        delete_data,
                            guint           time)
{
    GtkTreePath *path;

    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));

    path = _moo_icon_view_get_drag_dest_row (fileview->priv->iconview);
    _moo_icon_view_set_drag_dest_row (fileview->priv->iconview, NULL);

    gtk_drag_finish (context, success, delete_data, time);

    if (success && path)
    {
        _moo_icon_view_unselect_all (fileview->priv->iconview);
        _moo_icon_view_set_cursor (fileview->priv->iconview, path, FALSE);
    }

    if (path)
        gtk_tree_path_free (path);
}


static void
drag_leave (MooFileView *fileview,
            G_GNUC_UNUSED GdkDragContext *context,
            G_GNUC_UNUSED guint time,
            GtkWidget   *widget)
{
    _moo_tree_view_set_drag_dest_row (widget, NULL);
    cancel_drop_open (fileview);
}


static void
cancel_drop_open (MooFileView *fileview)
{
    if (fileview->priv->drop_to.row)
        gtk_tree_row_reference_free (fileview->priv->drop_to.row);

    if (fileview->priv->drop_to.timeout)
        g_source_remove (fileview->priv->drop_to.timeout);

    if (fileview->priv->drop_to.highlight && fileview->priv->drop_to.button)
        gtk_drag_unhighlight (fileview->priv->drop_to.button);

    fileview->priv->drop_to.button = NULL;
    fileview->priv->drop_to.highlight = FALSE;
    fileview->priv->drop_to.blink_clear = FALSE;
    fileview->priv->drop_to.n_blinks = 0;
    fileview->priv->drop_to.row = NULL;
    fileview->priv->drop_to.timeout = 0;
}


static gboolean
check_drop_targets (MooFileView    *fileview,
                    GdkDragContext *context,
                    GtkWidget      *widget)
{
    GdkAtom target;
    target = gtk_drag_dest_find_target (widget, context, fileview->priv->targets);
    return target != GDK_NONE;
}


static gboolean
drag_motion (GtkWidget      *widget,
             GdkDragContext *context,
             int             x,
             int             y,
             guint           time,
             MooFileView    *fileview)
{
    MooFolder *source_dir;
    MooFolder *current_dir;
    GtkTreePath *path = NULL;
    gboolean highlight_target = FALSE;
    gboolean can_drop = FALSE;
    int abs_x, abs_y;

    if (!check_drop_targets (fileview, context, widget))
    {
        _moo_tree_view_set_drag_dest_row (widget, NULL);
        return FALSE;
    }

    _moo_tree_view_widget_to_abs_coords (widget, x, y, &abs_x, &abs_y);
    _moo_tree_view_get_path_at_pos (widget, x, y, &path);

    if (MOO_IS_ICON_VIEW (widget))
    {
        current_dir = fileview->priv->current_dir;
        source_dir = g_object_get_data (G_OBJECT (fileview), "moo-file-view-source-dir");

        if (!current_dir)
            goto out;

        if (path)
        {
            MooFile *file = file_view_get_file_at_path (fileview, path);

            if (MOO_FILE_IS_DIR (file))
            {
                highlight_target = TRUE;
                can_drop = TRUE;
            }

            _moo_file_unref (file);
        }

        if (source_dir != current_dir)
            can_drop = TRUE;
    }
    else if (MOO_IS_BOOKMARK_VIEW (widget))
    {
        highlight_target = TRUE;
        can_drop = path != NULL;
    }

    if (highlight_target && path)
        _moo_tree_view_set_drag_dest_row (widget, path);
    else
        _moo_tree_view_set_drag_dest_row (widget, NULL);

    if (can_drop)
        gdk_drag_status (context, context->actions & GDK_ACTION_MOVE ?
                GDK_ACTION_MOVE : context->suggested_action, time);
    else
        gdk_drag_status (context, 0, time);

    if (highlight_target)
    {
        gboolean new_timeout = TRUE;

        if (fileview->priv->drop_to.row)
        {
            GtkTreePath *old_path = gtk_tree_row_reference_get_path (fileview->priv->drop_to.row);

            if (old_path && !gtk_tree_path_compare (path, old_path) &&
                !gtk_drag_check_threshold (widget,
                                           fileview->priv->drop_to.x,
                                           fileview->priv->drop_to.y,
                                           abs_x,
                                           abs_y))
            {
                new_timeout = FALSE;
                g_assert (fileview->priv->drop_to.timeout != 0);
            }
        }

        if (new_timeout)
        {
            cancel_drop_open (fileview);

            fileview->priv->drop_to.row =
                    gtk_tree_row_reference_new (_moo_tree_view_get_model (widget), path);

            fileview->priv->drop_to.timeout =
                    g_timeout_add (DROP_OPEN_TIMEOUT,
                                   (GSourceFunc) drop_open_timeout_func,
                                   fileview);

            fileview->priv->drop_to.x = abs_x;
            fileview->priv->drop_to.y = abs_y;
        }
    }
    else
    {
        cancel_drop_open (fileview);
    }

out:
    if (path)
        gtk_tree_path_free (path);
    return TRUE;
}


static void
button_drag_leave (MooFileView    *fileview,
                   G_GNUC_UNUSED GdkDragContext *context,
                   G_GNUC_UNUSED guint           time,
                   G_GNUC_UNUSED GtkWidget      *button)
{
    cancel_drop_open (fileview);
}


static gboolean
button_drag_motion (MooFileView    *fileview,
                    GdkDragContext *context,
                    G_GNUC_UNUSED int x,
                    G_GNUC_UNUSED int y,
                    guint           time,
                    GtkWidget      *button)
{
    gboolean new_timeout = TRUE;

    if (!check_drop_targets (fileview, context, button))
    {
        cancel_drop_open (fileview);
        return FALSE;
    }

    if (fileview->priv->drop_to.button == button)
    {
        new_timeout = FALSE;
        g_assert (fileview->priv->drop_to.timeout != 0);
    }

    if (new_timeout)
    {
        cancel_drop_open (fileview);

        if (!fileview->priv->drop_to.highlight)
        {
            gtk_drag_highlight (button);
            fileview->priv->drop_to.highlight = TRUE;
        }

        fileview->priv->drop_to.button = button;
        fileview->priv->drop_to.timeout =
                g_timeout_add (DROP_OPEN_TIMEOUT,
                               (GSourceFunc) drop_open_timeout_func,
                               fileview);
    }

    gdk_drag_status (context, context->suggested_action, time);
    return TRUE;
}


void
_moo_file_view_add_target (MooFileView    *fileview,
                           GdkAtom         target,
                           guint           flags,
                           guint           info)
{
    g_return_if_fail (MOO_IS_FILE_VIEW (fileview));
    gtk_target_list_add (fileview->priv->targets, target, flags, info);
    sync_dest_targets (fileview);
}


static void
sync_dest_targets (MooFileView *fileview)
{
    GSList *l;

    _moo_icon_view_set_dest_targets (fileview->priv->iconview,
                                     fileview->priv->targets);
    gtk_drag_dest_set_target_list (GTK_WIDGET (fileview->priv->bkview),
                                   fileview->priv->targets);

    for (l = fileview->priv->drag_dest_widgets; l != NULL; l = l->next)
        gtk_drag_dest_set_target_list (l->data, fileview->priv->targets);
}


/***************************************************************************/
/* Dropping stuff
 */

static void
run_command_on_files (MooFileView *fileview,
                      GList       *filenames,
                      const char  *destdir,
                      const char **first_args,
                      int          n_first_args)
{
    GError *error = NULL;
    char **argv;
    int list_len, n_args, i;
    GList *l;

    g_return_if_fail (filenames != NULL);
    g_return_if_fail (destdir != NULL);
    g_return_if_fail (n_first_args > 0);

    list_len = g_list_length (filenames);

    n_args = list_len + n_first_args + 1;
    argv = g_new (char*, n_args + 1);

    for (i = 0; i < n_first_args; ++i)
    {
        g_assert (first_args[i] != NULL);
        argv[i] = (char*) first_args[i];
    }

    argv[n_args-1] = (char*) destdir;
    argv[n_args] = NULL;

    for (i = 0, l = filenames; l != NULL; l = l->next, i++)
        argv[n_first_args + i] = l->data;

    if (!_moo_unix_spawn_async (argv, G_SPAWN_SEARCH_PATH, &error))
    {
        g_critical ("%s: could not spawn '%s'",
                    G_STRLOC, first_args[0]);
        g_critical ("%s: %s", G_STRLOC, error->message);
        g_error_free (error);
        goto out;
    }

    /* XXX strcmp */
    if (fileview->priv->current_dir &&
        !strcmp (destdir, _moo_folder_get_path (fileview->priv->current_dir)) &&
        list_len == 1)
    {
        char *basename = g_path_get_basename (filenames->data);

        if (basename)
            _moo_file_view_select_name (fileview, basename);

        g_free (basename);
    }

out:
    g_free (argv);
}


static void
copy_files (MooFileView *fileview,
            GList       *filenames,
            const char  *destdir)
{
    const char *args[] = {"cp", "-R", "--"};
    run_command_on_files (fileview, filenames, destdir,
                          args, G_N_ELEMENTS (args));
}


static void
move_files (MooFileView *fileview,
            GList       *filenames,
            const char  *destdir)
{
    const char *args[] = {"mv", "--"};
    run_command_on_files (fileview, filenames, destdir,
                          args, G_N_ELEMENTS (args));
}


static void
link_files (MooFileView *fileview,
            GList       *filenames,
            const char  *destdir)
{
    const char *args[] = {"ln", "-s", "--"};
    run_command_on_files (fileview, filenames, destdir,
                          args, G_N_ELEMENTS (args));
}


static void
free_string_list (GList *list)
{
    g_list_foreach (list, (GFunc) g_free, NULL);
    g_list_free (list);
}


static void
drop_item_activated (GObject     *item,
                     MooFileView *fileview)
{
    GdkDragAction action;
    gpointer data;
    GList *filenames;
    char *destdir;

    data = g_object_get_data (item, "moo-file-view-drop-action");
    filenames = g_object_get_data (item, "moo-file-view-drop-files");
    destdir = g_object_get_data (item, "moo-file-view-drop-dir");
    g_return_if_fail (filenames != NULL && destdir != NULL);

    action = GPOINTER_TO_INT (data);

    switch (action)
    {
        case GDK_ACTION_COPY:
            copy_files (fileview, filenames, destdir);
            break;
        case GDK_ACTION_MOVE:
            move_files (fileview, filenames, destdir);
            break;
        case GDK_ACTION_LINK:
            link_files (fileview, filenames, destdir);
            break;
        default:
            g_return_if_reached ();
    }
}


// /* XXX */
// static gboolean
// same_path (const char *path1,
//            const char *path2)
// {
//     return !strcmp (path1, path2);
// }
//
//
// static void
// copy_file (MooFileView  *fileview,
//            const char   *src,
//            const char   *destdir)
// {
//     const char *args[] = {"cp", "-R", "--"};
//     char *name = NULL;
//     GList *list = NULL;
//
//     name = _moo_file_view_copy_file_dialog (GTK_WIDGET (fileview), src, destdir);
//
//     if (!name)
//         goto out;
//
//     if (g_file_test (name, G_FILE_TEST_EXISTS))
//     {
//         g_critical ("%s: oops", G_STRLOC);
//         goto out;
//     }
//
//     list = g_list_prepend (NULL, (char*) src);
//     run_command_on_files (fileview, list, name, args, G_N_ELEMENTS (args));
//
// out:
//     g_free (name);
//     g_list_free (list);
// }


static void
moo_file_view_drop_uris (MooFileView    *fileview,
                         char          **uris,
                         const char     *destdir,
                         GtkWidget      *widget,
                         GdkDragContext *context,
                         G_GNUC_UNUSED int x,
                         G_GNUC_UNUSED int y,
                         guint           time)
{
    char **u;
    GList *filenames = NULL;
    GError *error = NULL;
    GdkModifierType mask;
    GdkDragAction action;
    gboolean success = FALSE;

    g_assert (uris != NULL);

    for (u = uris; *u; ++u)
    {
        char *file = g_filename_from_uri (*u, NULL, &error);

        if (!file)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
            goto out;
        }

        filenames = g_list_prepend (filenames, file);
    }

    if (!filenames)
    {
        g_warning ("%s: got empty uri list", G_STRLOC);
        goto out;
    }

//     if (!filenames->next)
//     {
//         char *dirname;
//
//         if (same_path (destdir, filenames->data))
//         {
//             g_warning ("%s: dragging folder to itself", G_STRLOC);
//             goto out;
//         }
//
//         dirname = g_path_get_dirname (filenames->data);
//
//         if (same_path (destdir, dirname))
//         {
//             success = TRUE;
//             copy_file (fileview, filenames->data, destdir);
//             g_free (dirname);
//             goto out;
//         }
//
//         g_free (dirname);
//     }

    mask = _moo_get_modifiers (widget);

#if 0
#define ACTION_NAME(ac) (ac == GDK_ACTION_DEFAULT ? "DEFAULT" :             \
                         (ac == GDK_ACTION_COPY ? "COPY" :                  \
                          (ac == GDK_ACTION_MOVE ? "MOVE" :                 \
                           (ac == GDK_ACTION_LINK ? "LINK" :                \
                            (ac == GDK_ACTION_PRIVATE ? "PRIVATE" :         \
                             (ac == GDK_ACTION_ASK ? "ASK" : "???"))))))

    g_print ("suggested: %s\naction: %s\n",
             ACTION_NAME (context->suggested_action),
             ACTION_NAME (context->action));

    g_print ("actions: %s%s%s%s%s%s\n",
             context->actions & GDK_ACTION_DEFAULT ? "DEFAULT " : "",
             context->actions & GDK_ACTION_COPY ? "COPY " : "",
             context->actions & GDK_ACTION_MOVE ? "MOVE " : "",
             context->actions & GDK_ACTION_LINK ? "LINK " : "",
             context->actions & GDK_ACTION_PRIVATE ? "PRIVATE " : "",
             context->actions & GDK_ACTION_ASK ? "ASK " : "");

    g_print ("modifiers: %s%s%s\n",
             mask & GDK_SHIFT_MASK ? "SHIFT " : "",
             mask & GDK_CONTROL_MASK ? "CONTROL " : "",
             mask & GDK_MOD1_MASK ? "MOD1 " : "");
#endif

    if (mask & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
        action = context->suggested_action;
    else
        action = GDK_ACTION_ASK;

    switch (action)
    {
        case GDK_ACTION_COPY:
        case GDK_ACTION_MOVE:
        case GDK_ACTION_LINK:
        case GDK_ACTION_ASK:
            break;

        default:
            g_warning ("%s: oops", G_STRLOC);
            action = GDK_ACTION_ASK;
    }

    if (action == GDK_ACTION_ASK)
    {
        GtkWidget *menu, *item;
        char *dir_copy = g_strdup (destdir);

        menu = gtk_menu_new ();
        gtk_object_sink (g_object_ref (menu));
        g_signal_connect (menu, "deactivate", G_CALLBACK (destroy_menu), NULL);
        g_object_set_data_full (G_OBJECT (menu), "moo-file-view-drop-files",
                                filenames, (GDestroyNotify) free_string_list);
        g_object_set_data_full (G_OBJECT (menu), "moo-file-view-drop-dir",
                                dir_copy, g_free);

#define CREATE_IT(stock,action,accel_label)                                                 \
        item = gtk_image_menu_item_new_from_stock (stock, NULL);                            \
        g_object_set_data (G_OBJECT (item), "moo-file-view-drop-files", filenames);         \
        g_object_set_data (G_OBJECT (item), "moo-file-view-drop-dir", dir_copy);            \
        g_object_set_data (G_OBJECT (item), "moo-file-view-drop-action",                    \
                           GINT_TO_POINTER (action));                                       \
        g_signal_connect (item, "activate", G_CALLBACK (drop_item_activated), fileview);    \
        gtk_widget_show (item);                                                             \
        _moo_menu_item_set_accel_label (item, accel_label);                                  \
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        CREATE_IT (MOO_STOCK_FILE_MOVE, GDK_ACTION_MOVE, "Shift");
        CREATE_IT (MOO_STOCK_FILE_COPY, GDK_ACTION_COPY, "Control");
        CREATE_IT (MOO_STOCK_FILE_LINK, GDK_ACTION_LINK, "Control+Shift");
#undef CREATE_IT

        item = gtk_separator_menu_item_new ();
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CANCEL, NULL);
        gtk_widget_show (item);
        _moo_menu_item_set_accel_label (item, "Escape");
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        _moo_file_view_drag_finish (fileview, context, TRUE, FALSE, time);
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);

        return;
    }

    switch (action)
    {
        case GDK_ACTION_COPY:
            copy_files (fileview, filenames, destdir);
            break;
        case GDK_ACTION_MOVE:
            move_files (fileview, filenames, destdir);
            break;
        case GDK_ACTION_LINK:
            link_files (fileview, filenames, destdir);
            break;

        default:
            g_assert_not_reached ();
    }

    success = TRUE;

out:
    free_string_list (filenames);
    _moo_file_view_drag_finish (fileview, context, success, FALSE, time);
}


static gboolean
moo_file_view_drop_text (G_GNUC_UNUSED MooFileView *fileview,
                         const char     *text,
                         const char     *destdir,
                         GtkWidget      *widget,
                         G_GNUC_UNUSED GdkDragContext *context,
                         G_GNUC_UNUSED int x,
                         G_GNUC_UNUSED int y,
                         G_GNUC_UNUSED guint time,
                         G_GNUC_UNUSED gboolean *delete)
{
    char *name = NULL;
    gboolean result = FALSE;
    GError *error = NULL;

    g_return_val_if_fail (text != NULL, FALSE);

    while (TRUE)
    {
        name = _moo_file_view_save_drop_dialog (widget, destdir);

        if (!name)
            return FALSE;

        if (g_file_test (name, G_FILE_TEST_EXISTS))
        {
            g_critical ("%s: oops", G_STRLOC);
            goto out;
        }

        if (!_moo_save_file_utf8 (name, text, -1, &error))
        {
            char *utf8_name = g_filename_display_name (name);
            char *err_text = g_strdup_printf ("Could not save file\n%s", utf8_name);

            moo_error_dialog (widget, err_text, error->message);

            g_free (err_text);
            g_free (utf8_name);

            g_free (name);
            g_error_free (error);
            name = NULL;
            error = NULL;
        }
        else
        {
            result = TRUE;
            goto out;
        }
    }

out:
    g_free (name);
    return result;
}


#if 0
static void
bookmark_drag_data_received (MooFileView    *fileview,
                             GdkDragContext *context,
                             int             x,
                             int             y,
                             GtkSelectionData *data,
                             guint           info,
                             guint           time,
                             MooBookmarkView *bkview);


static gboolean
bookmark_drag_drop (MooFileView    *fileview,
                    GdkDragContext *context,
                    int             x,
                    int             y,
                    guint           time,
                    MooBookmarkView *bkview);


static void
bookmark_drag_leave (MooFileView    *fileview,
                     GdkDragContext *context,
                     guint           time,
                     MooBookmarkView *bkview);


static gboolean
bookmark_drag_motion (MooBookmarkView *bkview,
                      GdkDragContext *context,
                      int             x,
                      int             y,
                      guint           time,
                      MooFileView    *fileview)
{
    MooFolder *source_dir;
    MooFolder *current_dir;
    GtkTreePath *path = NULL;
    MooIconViewCell cell;
    int cell_x, cell_y;
    gboolean new_timeout = TRUE;

    if (!check_drop_targets (fileview, context, GTK_WIDGET (bkview)) ||
         !_moo_tree_view_get_path_at_pos (bkview, x, y, &path, &cell_x, &cell_y))
    {
        _moo_tree_view_set_drag_dest_row (bkview, NULL);
        cancel_drop_open (fileview);
        return FALSE;
    }

    _moo_tree_view_set_drag_dest_row (bkview, path);

    gdk_drag_status (context, context->actions & GDK_ACTION_MOVE ?
                     GDK_ACTION_MOVE : context->suggested_action, time);

    if (fileview->priv->drop_to.row)
    {
        GtkTreePath *old_path = gtk_tree_row_reference_get_path (fileview->priv->drop_to.row);

        if (old_path && !gtk_tree_path_compare (path, old_path) &&
            fileview->priv->drop_to.cell == cell &&
            !gtk_drag_check_threshold (GTK_WIDGET (bkview),
                                       fileview->priv->drop_to.x,
                                       fileview->priv->drop_to.y,
                                       cell_x,
                                       cell_y))
        {
            new_timeout = FALSE;
            g_assert (fileview->priv->drop_to.timeout != 0);
        }
    }

    if (new_timeout)
    {
        cancel_drop_open (fileview);

        fileview->priv->drop_to.row =
                gtk_tree_row_reference_new (_moo_tree_view_get_model (bkview), path);
        fileview->priv->drop_to.timeout =
                g_timeout_add (DROP_OPEN_TIMEOUT,
                               (GSourceFunc) drop_open_timeout_func,
                               fileview);
        fileview->priv->drop_to.x = cell_x;
        fileview->priv->drop_to.y = cell_y;
        fileview->priv->drop_to.cell = cell;
    }

out:
    if (path)
        gtk_tree_path_free (path);
    return TRUE;
}
#endif