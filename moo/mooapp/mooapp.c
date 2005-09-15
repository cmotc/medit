/*
 *   mooapp/mooapp.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_PYTHON
#include <Python.h>
#include "mooapp/moopythonconsole.h"
#include "mooapp/mooapp-python.h"
#include "mooapp/moopython.h"
#include "mooapp/mooappinput.h"
#endif

#include "mooapp/mooapp-private.h"
#include "mooedit/mooeditprefs.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooplugin.h"
#include "mooedit/plugins/mooeditplugins.h"
#include "mooui/moouiobject.h"
#include "mooutils/moomarshals.h"
#include "mooutils/moocompat.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moostock.h"


/* moo-pygtk.c */
void initmoo (void);


static MooApp *moo_app_instance = NULL;
#ifdef USE_PYTHON
static MooPython *moo_app_python = NULL;
static MooAppInput *moo_app_input = NULL;
#endif


struct _MooAppPrivate {
    char      **argv;
    int         exit_code;
    MooEditor  *editor;
    MooAppInfo *info;
    gboolean    running;
    gboolean    run_python;
    MooAppWindowPolicy window_policy;
    GSList     *terminals;
    MooTermWindow *term_window;
    MooUIXML   *ui_xml;
    guint       quit_handler_id;
};


static void     moo_app_class_init      (MooAppClass    *klass);
static void     moo_app_gobj_init       (MooApp         *app);
static GObject *moo_app_constructor     (GType           type,
                                         guint           n_params,
                                         GObjectConstructParam *params);
static void     moo_app_finalize        (GObject        *object);

static void     install_actions         (MooApp         *app,
                                         GType           type);
static void     install_editor_actions  (void);
static void     install_terminal_actions(void);

static void     moo_app_set_property    (GObject        *object,
                                         guint           prop_id,
                                         const GValue   *value,
                                         GParamSpec     *pspec);
static void     moo_app_get_property    (GObject        *object,
                                         guint           prop_id,
                                         GValue         *value,
                                         GParamSpec     *pspec);

static void     moo_app_set_argv        (MooApp         *app,
                                         char          **argv);
static char   **moo_app_get_argv        (MooApp         *app);

static gboolean moo_app_init_real       (MooApp         *app);
static int      moo_app_run_real        (MooApp         *app);
static void     moo_app_quit_real       (MooApp         *app);
static gboolean moo_app_try_quit_real   (MooApp         *app);

static void     moo_app_set_name        (MooApp         *app,
                                         const char     *short_name,
                                         const char     *full_name);
static void     moo_app_set_description (MooApp         *app,
                                         const char     *description);

static void     all_editors_closed      (MooApp         *app);

static void     start_python            (MooApp         *app);
#ifdef USE_PYTHON
static void     execute_selection       (MooEditWindow  *window);
#endif


static GObjectClass *moo_app_parent_class;
GType moo_app_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        static const GTypeInfo type_info = {
            sizeof (MooAppClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) moo_app_class_init,
            (GClassFinalizeFunc) NULL,
            NULL,   /* class_data */
            sizeof (MooApp),
            0,      /* n_preallocs */
            (GInstanceInitFunc) moo_app_gobj_init,
            NULL    /* value_table */
        };

        type = g_type_register_static (G_TYPE_OBJECT,
                                       "MooApp",
                                       &type_info,
                                       0);
    }

    return type;
}


enum {
    PROP_0,
    PROP_ARGV,
    PROP_SHORT_NAME,
    PROP_FULL_NAME,
    PROP_DESCRIPTION,
    PROP_WINDOW_POLICY,
    PROP_RUN_PYTHON
};

enum {
    INIT,
    RUN,
    QUIT,
    TRY_QUIT,
    PREFS_DIALOG,
    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL];


static void moo_app_class_init (MooAppClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    moo_create_stock_items ();

    moo_app_parent_class = g_type_class_peek_parent (klass);

    gobject_class->constructor = moo_app_constructor;
    gobject_class->finalize = moo_app_finalize;
    gobject_class->set_property = moo_app_set_property;
    gobject_class->get_property = moo_app_get_property;

    klass->init = moo_app_init_real;
    klass->run = moo_app_run_real;
    klass->quit = moo_app_quit_real;
    klass->try_quit = moo_app_try_quit_real;
    klass->prefs_dialog = _moo_app_create_prefs_dialog;

    g_object_class_install_property (gobject_class,
                                     PROP_ARGV,
                                     g_param_spec_pointer
                                             ("argv",
                                              "argv",
                                              "Null-terminated array of application arguments",
                                              G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_SHORT_NAME,
                                     g_param_spec_string
                                             ("short-name",
                                              "short-name",
                                              "short-name",
                                              "ggap",
                                              G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_FULL_NAME,
                                     g_param_spec_string
                                             ("full-name",
                                              "full-name",
                                              "full-name",
                                              NULL,
                                              G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_DESCRIPTION,
                                     g_param_spec_string
                                             ("description",
                                              "description",
                                              "description",
                                              NULL,
                                              G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_WINDOW_POLICY,
                                     g_param_spec_flags
                                             ("window-policy",
                                              "window-policy",
                                              "window-policy",
                                              MOO_TYPE_APP_WINDOW_POLICY,
                                              MOO_APP_ONE_EDITOR | MOO_APP_ONE_TERMINAL |
                                                      MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS,
                                              G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

    g_object_class_install_property (gobject_class,
                                     PROP_RUN_PYTHON,
                                     g_param_spec_boolean
                                             ("run-python",
                                              "run-python",
                                              "run-python",
                                              TRUE,
                                              G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

    signals[INIT] = g_signal_new ("init",
                                  G_OBJECT_CLASS_TYPE (klass),
                                  0,
                                  G_STRUCT_OFFSET (MooAppClass, init),
                                  NULL, NULL,
                                  _moo_marshal_BOOLEAN__VOID,
                                  G_TYPE_STRING, 0);

    signals[RUN] = g_signal_new ("run",
                                 G_OBJECT_CLASS_TYPE (klass),
                                 0,
                                 G_STRUCT_OFFSET (MooAppClass, run),
                                 NULL, NULL,
                                 _moo_marshal_INT__VOID,
                                 G_TYPE_INT, 0);

    signals[QUIT] = g_signal_new ("quit",
                                  G_OBJECT_CLASS_TYPE (klass),
                                  0,
                                  G_STRUCT_OFFSET (MooAppClass, quit),
                                  NULL, NULL,
                                  _moo_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);

    signals[TRY_QUIT] = g_signal_new ("try-quit",
                                  G_OBJECT_CLASS_TYPE (klass),
                                  (GSignalFlags) (G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST),
                                  G_STRUCT_OFFSET (MooAppClass, try_quit),
                                  g_signal_accumulator_true_handled, NULL,
                                  _moo_marshal_BOOLEAN__VOID,
                                  G_TYPE_BOOLEAN, 0);

    signals[PREFS_DIALOG] = g_signal_new ("prefs-dialog",
                                          G_OBJECT_CLASS_TYPE (klass),
                                          0,
                                          G_STRUCT_OFFSET (MooAppClass, quit),
                                          NULL, NULL,
                                          _moo_marshal_OBJECT__VOID,
                                          MOO_TYPE_PREFS_DIALOG, 0);
}


static void moo_app_gobj_init (MooApp *app)
{
    g_assert (moo_app_instance == NULL);

    moo_app_instance = app;

    app->priv = g_new0 (MooAppPrivate, 1);
    app->priv->info = g_new0 (MooAppInfo, 1);

#ifdef VERSION
    app->priv->info->version = g_strdup (VERSION);
#else
    app->priv->info->version = g_strdup ("<unknown version>");
#endif
    app->priv->info->website = g_strdup ("http://ggap.sourceforge.net/");
    app->priv->info->website_label = g_strdup ("ggap.sourceforge.net");
}


static GObject *moo_app_constructor     (GType           type,
                                         guint           n_params,
                                         GObjectConstructParam *params)
{
    GObject *object = moo_app_parent_class->constructor (type, n_params, params);
    MooApp *app = MOO_APP (object);

    if (!app->priv->info->full_name)
        app->priv->info->full_name = g_strdup (app->priv->info->short_name);

    install_actions (app, MOO_TYPE_EDIT_WINDOW);
    install_actions (app, MOO_TYPE_TERM_WINDOW);
    install_terminal_actions ();
    install_editor_actions ();

    return object;
}


static void moo_app_finalize       (GObject      *object)
{
    MooApp *app = MOO_APP(object);

    moo_app_quit_real (app);

    moo_app_instance = NULL;

    g_free (app->priv->info->short_name);
    g_free (app->priv->info->full_name);
    g_free (app->priv->info->app_dir);
    g_free (app->priv->info->rc_file);
    g_free (app->priv->info);

    if (app->priv->argv)
        g_strfreev (app->priv->argv);
    if (app->priv->editor)
        g_object_unref (app->priv->editor);
    if (app->priv->ui_xml)
        g_object_unref (app->priv->ui_xml);

    g_free (app->priv);

    G_OBJECT_CLASS (moo_app_parent_class)->finalize (object);
}


static void moo_app_set_property    (GObject        *object,
                                     guint           prop_id,
                                     const GValue   *value,
                                     GParamSpec     *pspec)
{
    MooApp *app = MOO_APP (object);
    switch (prop_id)
    {
        case PROP_ARGV:
            moo_app_set_argv (app, (char**)g_value_get_pointer (value));
            break;

        case PROP_SHORT_NAME:
            moo_app_set_name (app, g_value_get_string (value), NULL);
            break;

        case PROP_FULL_NAME:
            moo_app_set_name (app, NULL, g_value_get_string (value));
            break;

        case PROP_DESCRIPTION:
            moo_app_set_description (app, g_value_get_string (value));
            break;

        case PROP_WINDOW_POLICY:
            app->priv->window_policy = g_value_get_flags (value);
            g_object_notify (object, "window-policy");
            break;

        case PROP_RUN_PYTHON:
            app->priv->run_python = g_value_get_boolean (value);
            g_object_notify (object, "run-python");
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void moo_app_get_property    (GObject        *object,
                                     guint           prop_id,
                                     GValue         *value,
                                     GParamSpec     *pspec)
{
    MooApp *app = MOO_APP (object);
    switch (prop_id)
    {
        case PROP_ARGV:
            g_value_set_pointer (value, moo_app_get_argv (app));
            break;

        case PROP_SHORT_NAME:
            g_value_set_string (value, app->priv->info->short_name);
            break;

        case PROP_FULL_NAME:
            g_value_set_string (value, app->priv->info->full_name);
            break;

        case PROP_DESCRIPTION:
            g_value_set_string (value, app->priv->info->description);
            break;

        case PROP_WINDOW_POLICY:
            g_value_set_flags (value, app->priv->window_policy);
            break;

        case PROP_RUN_PYTHON:
            g_value_set_boolean (value, app->priv->run_python);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


MooApp          *moo_app_get_instance          (void)
{
    return moo_app_instance;
}


static void     moo_app_set_argv        (MooApp         *app,
                                         char          **argv)
{
    g_strfreev (app->priv->argv);
    app->priv->argv = g_strdupv (argv);
    g_object_notify (G_OBJECT (app), "argv");
}


static char   **moo_app_get_argv        (MooApp         *app)
{
    return g_strdupv (app->priv->argv);
}


int              moo_app_get_exit_code         (MooApp      *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), 0);
    return app->priv->exit_code;
}

void             moo_app_set_exit_code         (MooApp      *app,
                                                int          code)
{
    g_return_if_fail (MOO_IS_APP (app));
    app->priv->exit_code = code;
}


const char      *moo_app_get_application_dir   (MooApp      *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), ".");

    if (!app->priv->info->app_dir)
    {
        g_return_val_if_fail (app->priv->argv && app->priv->argv[0], ".");
        app->priv->info->app_dir = g_path_get_dirname (app->priv->argv[0]);
    }

    return app->priv->info->app_dir;
}


const char      *moo_app_get_input_pipe_name   (G_GNUC_UNUSED MooApp *app)
{
#ifdef USE_PYTHON
    g_return_val_if_fail (moo_app_input != NULL, NULL);
    return moo_app_input->pipe_name;
#else /* !USE_PYTHON */
    g_warning ("%s: python support is not compiled in", G_STRLOC);
    return NULL;
#endif /* !USE_PYTHON */
}


const char      *moo_app_get_rc_file_name       (MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);

    if (!app->priv->info->rc_file)
    {
#ifdef __WIN32__
        char *basename = g_strdup_printf ("%s.ini",
                                          app->priv->info->short_name);
        app->priv->info->rc_file =
                g_build_filename (g_get_user_config_dir (),
                                  basename,
                                  NULL);
        g_free (basename);
#else
        char *basename = g_strdup_printf (".%src",
                                          app->priv->info->short_name);
        app->priv->info->rc_file =
                g_build_filename (g_get_home_dir (),
                                  basename,
                                  NULL);
        g_free (basename);
#endif
    }

    return app->priv->info->rc_file;
}


#ifdef USE_PYTHON
void             moo_app_python_execute_file   (G_GNUC_UNUSED GtkWindow *parent_window)
{
    GtkWidget *parent;
    const char *filename = NULL;
    FILE *file;

    g_return_if_fail (moo_app_python != NULL);

    parent = parent_window ? GTK_WIDGET (parent_window) : NULL;
    if (!filename)
        filename = moo_file_dialogp (parent,
                                     MOO_DIALOG_FILE_OPEN_EXISTING,
                                     "Choose Python Script to Execute",
                                     "python_exec_file", NULL);

    if (!filename) return;

    file = fopen (filename, "r");
    if (!file)
    {
        moo_error_dialog (parent, "Could not open file", NULL);
    }
    else
    {
        PyObject *res = (PyObject*)moo_python_run_file (moo_app_python, file, filename);
        fclose (file);
        if (res)
            Py_XDECREF (res);
        else
            PyErr_Print ();
    }
}


gboolean         moo_app_python_run_file       (G_GNUC_UNUSED MooApp      *app,
                                                G_GNUC_UNUSED const char  *filename)
{
    FILE *file;
    PyObject *res;

    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (moo_app_python != NULL, FALSE);

    file = fopen (filename, "r");
    g_return_val_if_fail (file != NULL, FALSE);

    res = (PyObject*)moo_python_run_file (moo_app_python, file, filename);
    fclose (file);
    if (res) {
        Py_XDECREF (res);
        return TRUE;
    }
    else {
        PyErr_Print ();
        return FALSE;
    }
}


gboolean         moo_app_python_run_string     (G_GNUC_UNUSED MooApp      *app,
                                                G_GNUC_UNUSED const char  *string)
{
    PyObject *res;
    g_return_val_if_fail (string != NULL, FALSE);
    g_return_val_if_fail (moo_app_python != NULL, FALSE);
    res = (PyObject*) moo_python_run_string (moo_app_python, string, FALSE);
    if (res) {
        Py_XDECREF (res);
        return TRUE;
    }
    else {
        PyErr_Print ();
        return FALSE;
    }
}


GtkWidget       *moo_app_get_python_console    (G_GNUC_UNUSED MooApp *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    g_return_val_if_fail (moo_app_python != NULL, NULL);
    return GTK_WIDGET (moo_app_python->console);
}


void             moo_app_show_python_console   (G_GNUC_UNUSED MooApp *app)
{
    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (moo_app_python != NULL);
    gtk_window_present (GTK_WINDOW (moo_app_python->console));
}


void             moo_app_hide_python_console   (G_GNUC_UNUSED MooApp *app)
{
    g_return_if_fail (MOO_IS_APP (app));
    g_return_if_fail (moo_app_python != NULL);
    gtk_widget_hide (GTK_WIDGET (moo_app_python->console));
}


static guint strv_length (char **argv)
{
    guint len = 0;
    char **c;

    if (!argv)
        return 0;

    for (c = argv; *c != NULL; ++c)
        ++len;

    return len;
}
#endif /* !USE_PYTHON */


MooEditor       *moo_app_get_editor            (MooApp          *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    return app->priv->editor;
}


const MooAppInfo*moo_app_get_info               (MooApp     *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);
    return app->priv->info;
}


static gboolean moo_app_init_real       (MooApp         *app)
{
    G_GNUC_UNUSED const char *app_dir;
    const char *rc_file;
    MooEditLangMgr *mgr;
    MooUIXML *ui_xml;
    GError *error = NULL;
    MooAppWindowPolicy policy = app->priv->window_policy;

#ifdef __WIN32__
    app_dir = moo_app_get_application_dir (app);
#endif

    rc_file = moo_app_get_rc_file_name (app);

    if (!moo_prefs_load (rc_file, &error))
    {
        g_warning ("%s: could not read config file", G_STRLOC);
        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }

    ui_xml = moo_app_get_ui_xml (app);

    if (policy & MOO_APP_USE_EDITOR)
    {
        char *plugin_dir, *user_plugin_dir = NULL;
        char *lang_dir, *user_lang_dir = NULL;
        char *user_data_dir = NULL;

#ifdef __WIN32__

        const char *data_dir;

        if (app_dir[0])
            data_dir = app_dir;
        else
            data_dir = ".";

        lang_dir = g_build_filename (data_dir, "language-specs", NULL);
        plugin_dir = g_build_filename (data_dir, "plugins", NULL);

#else /* !__WIN32__ */

#ifdef MOO_EDIT_LANG_FILES_DIR
        lang_dir = g_strdup (MOO_EDIT_LANG_FILES_DIR);
#else
        lang_dir = g_build_filename (".", "language-specs", NULL);
#endif

#ifdef MOO_PLUGINS_DIR
        plugin_dir = g_strdup (MOO_PLUGINS_DIR);
#else
        plugin_dir = g_build_filename (".", "plugins", NULL);
#endif

        user_data_dir = g_strdup_printf (".%s", app->priv->info->short_name);
        user_plugin_dir = g_build_filename (user_data_dir, "plugins", NULL);
        user_lang_dir = g_build_filename (user_data_dir, "language-specs", NULL);

#endif /* !__WIN32__ */

        app->priv->editor = moo_editor_new ();
        moo_editor_set_ui_xml (app->priv->editor, ui_xml);
        moo_editor_set_app_name (app->priv->editor,
                                 app->priv->info->short_name);

        mgr = moo_editor_get_lang_mgr (app->priv->editor);
        moo_edit_lang_mgr_add_lang_files_dir (mgr, lang_dir);

        if (user_lang_dir)
            moo_edit_lang_mgr_add_lang_files_dir (mgr, user_lang_dir);

        g_signal_connect_swapped (app->priv->editor,
                                  "all-windows-closed",
                                  G_CALLBACK (all_editors_closed),
                                  app);

        moo_find_init ();
        moo_file_selector_init ();

        moo_plugin_read_dir (plugin_dir);

        if (user_plugin_dir)
            moo_plugin_read_dir (user_plugin_dir);

        g_free (lang_dir);
        g_free (plugin_dir);
        g_free (user_lang_dir);
        g_free (user_plugin_dir);
        g_free (user_data_dir);
    }

#ifdef __WIN32__
    moo_term_set_helper_directory (app_dir);
    g_message ("app dir: %s", app_dir);
#endif /* __WIN32__ */

    start_python (app);

    return TRUE;
}


static void     start_python            (G_GNUC_UNUSED MooApp *app)
{
#ifdef USE_PYTHON
    if (app->priv->run_python)
    {
        moo_app_python = moo_python_new ();
        moo_python_start (moo_app_python,
                          strv_length (app->priv->argv),
                          app->priv->argv);
#ifdef USE_PYGTK
        initmoo ();
#endif

        moo_app_input = moo_app_input_new (moo_app_python,
                                           app->priv->info->short_name);
        moo_app_input_start (moo_app_input);
    }
    else
    {
        moo_app_python = moo_python_get_instance ();
        if (moo_app_python)
            g_object_ref (moo_app_python);
    }
#endif /* !USE_PYTHON */
}


static gboolean on_gtk_main_quit (MooApp *app)
{
    app->priv->quit_handler_id = 0;

    if (!moo_app_quit (app))
        MOO_APP_GET_CLASS(app)->quit (app);

    return FALSE;
}


static int      moo_app_run_real        (MooApp         *app)
{
    g_return_val_if_fail (!app->priv->running, 0);
    app->priv->running = TRUE;

    app->priv->quit_handler_id =
            gtk_quit_add (0, (GtkFunction) on_gtk_main_quit, app);
    gtk_main ();

    return app->priv->exit_code;
}


static gboolean moo_app_try_quit_real   (MooApp         *app)
{
    GSList *l, *list;

    if (!app->priv->running)
        return FALSE;

    list = g_slist_copy (app->priv->terminals);
    for (l = list; l != NULL; l = l->next)
    {
        if (!moo_window_close (MOO_WINDOW (l->data)))
        {
            g_slist_free (list);
            return TRUE;
        }
    }
    g_slist_free (list);

    if (!moo_editor_close_all (app->priv->editor))
        return TRUE;

    return FALSE;
}


static void     moo_app_quit_real       (MooApp         *app)
{
    GSList *l, *list;
    GError *error = NULL;

    if (!app->priv->running)
        return;
    else
        app->priv->running = FALSE;

#ifdef USE_PYTHON
    if (moo_app_input)
    {
        moo_app_input_shutdown (moo_app_input);
        moo_app_input_unref (moo_app_input);
        moo_app_input = NULL;
    }

    if (moo_app_python)
    {
        moo_python_shutdown (moo_app_python);
        g_object_unref (moo_app_python);
        moo_app_python = NULL;
    }
#endif

    list = g_slist_copy (app->priv->terminals);
    for (l = list; l != NULL; l = l->next)
        moo_window_close (MOO_WINDOW (l->data));
    g_slist_free (list);
    g_slist_free (app->priv->terminals);
    app->priv->terminals = NULL;
    app->priv->term_window = NULL;

    moo_editor_close_all (app->priv->editor);
    g_object_unref (app->priv->editor);
    app->priv->editor = NULL;

    if (!moo_prefs_save (moo_app_get_rc_file_name (app), &error))
    {
        g_warning ("%s: could not save config file", G_STRLOC);
        if (error)
        {
            g_warning ("%s: %s", G_STRLOC, error->message);
            g_error_free (error);
        }
    }

    if (app->priv->quit_handler_id)
        gtk_quit_remove (app->priv->quit_handler_id);

    gtk_main_quit ();
}


void             moo_app_init                   (MooApp     *app)
{
    g_return_if_fail (MOO_IS_APP (app));

    MOO_APP_GET_CLASS(app)->init (app);
}


int              moo_app_run                    (MooApp     *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), -1);

    return MOO_APP_GET_CLASS(app)->run (app);
}


gboolean         moo_app_quit                   (MooApp     *app)
{
    gboolean quit;

    g_return_val_if_fail (MOO_IS_APP (app), FALSE);

    g_signal_emit (app, signals[TRY_QUIT], 0, &quit);

    if (!quit)
    {
        MOO_APP_GET_CLASS(app)->quit (app);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


static void     moo_app_set_name        (MooApp         *app,
                                         const char     *short_name,
                                         const char     *full_name)
{
    if (short_name)
    {
        g_free (app->priv->info->short_name);
        app->priv->info->short_name = g_strdup (short_name);
        g_object_notify (G_OBJECT (app), "short_name");
    }

    if (full_name)
    {
        g_free (app->priv->info->full_name);
        app->priv->info->full_name = g_strdup (full_name);
        g_object_notify (G_OBJECT (app), "full_name");
    }
}


static void     moo_app_set_description (MooApp         *app,
                                         const char     *description)
{
    g_free (app->priv->info->description);
    app->priv->info->description = g_strdup (description);
    g_object_notify (G_OBJECT (app), "description");
}


static void install_actions (MooApp *app, GType  type)
{
    GObjectClass *klass = g_type_class_ref (type);
    char *about, *_about;

    g_return_if_fail (klass != NULL);

    about = g_strdup_printf ("About %s", app->priv->info->full_name);
    _about = g_strdup_printf ("_About %s", app->priv->info->full_name);

    moo_ui_object_class_new_action (klass, "Quit",
                                    "name", "Quit",
                                    "label", "_Quit",
                                    "tooltip", "Quit",
                                    "icon-stock-id", GTK_STOCK_QUIT,
                                    "accel", "<ctrl>Q",
                                    "closure::callback", moo_app_quit,
                                    "closure::proxy-func", moo_app_get_instance,
                                    NULL);

    moo_ui_object_class_new_action (klass, "Preferences",
                                    "name", "Preferences",
                                    "label", "Pre_ferences",
                                    "tooltip", "Preferences",
                                    "icon-stock-id", GTK_STOCK_PREFERENCES,
                                    "accel", "<ctrl>P",
                                    "closure::callback", moo_app_prefs_dialog,
                                    NULL);

    moo_ui_object_class_new_action (klass, "About",
                                    "name", "About",
                                    "label", _about,
                                    "tooltip", about,
                                    "icon-stock-id", GTK_STOCK_ABOUT,
                                    "closure::callback", moo_app_about_dialog,
                                    NULL);

#ifdef USE_PYTHON
    moo_ui_object_class_new_action (klass, "PythonMenu",
                                    "name", "Python Menu",
                                    "label", "P_ython",
                                    "visible", TRUE,
                                    "no-accel", TRUE,
                                    NULL);

    moo_ui_object_class_new_action (klass, "ExecuteScript",
                                    "name", "Execute Script",
                                    "label", "_Execute Script",
                                    "tooltip", "Execute Script",
                                    "icon-stock-id", GTK_STOCK_EXECUTE,
                                    "closure::callback", moo_app_python_execute_file,
                                    NULL);

    moo_ui_object_class_new_action (klass, "ShowConsole",
                                    "name", "Show Console",
                                    "label", "Show Conso_le",
                                    "tooltip", "Show Console",
                                    "accel", "<alt>L",
                                    "closure::callback", moo_app_show_python_console,
                                    "closure::proxy-func", moo_app_get_instance,
                                    NULL);
#else /* !USE_PYTHON */
    moo_ui_object_class_new_action (klass, "PythonMenu",
                                    "dead", TRUE,
                                    NULL);
#endif /* USE_PYTHON */

    g_type_class_unref (klass);
    g_free (about);
    g_free (_about);
}


static void terminal_destroyed (MooTermWindow *term,
                                MooApp        *app)
{
    MooAppWindowPolicy policy;
    gboolean quit;

    app->priv->terminals = g_slist_remove (app->priv->terminals,
                                           term);

    policy = app->priv->window_policy;
    quit = (policy & MOO_APP_QUIT_ON_CLOSE_ALL_TERMINALS) ||
            ((policy & MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS) &&
             !moo_editor_get_active_window (app->priv->editor));

    if (quit)
        moo_app_quit (app);
}


static MooTermWindow *new_terminal (MooApp *app)
{
    MooTermWindow *term;

    term = g_object_new (MOO_TYPE_TERM_WINDOW, NULL);
    app->priv->terminals = g_slist_append (app->priv->terminals, term);

    g_signal_connect (term, "destroy",
                      G_CALLBACK (terminal_destroyed), app);

    if (app->priv->ui_xml)
        moo_ui_object_set_ui_xml (MOO_UI_OBJECT (term), app->priv->ui_xml);

    return term;
}


static void open_terminal (void)
{
    MooApp *app = moo_app_get_instance ();
    MooTermWindow *term;

    g_return_if_fail (app != NULL);

    if (app->priv->terminals)
        term = app->priv->terminals->data;
    else
        term = new_terminal (app);

    gtk_window_present (GTK_WINDOW (term));
}


MooTermWindow *moo_app_get_terminal (MooApp *app)
{
    MooTermWindow *term;

    g_return_val_if_fail (MOO_IS_APP (app), NULL);

    if (app->priv->terminals)
    {
        return app->priv->terminals->data;
    }
    else
    {
        term = new_terminal (app);
        gtk_window_present (GTK_WINDOW (term));
        return term;
    }
}


static void install_editor_actions  (void)
{
    GObjectClass *klass = g_type_class_ref (MOO_TYPE_EDIT_WINDOW);

    g_return_if_fail (klass != NULL);

#ifdef USE_PYTHON
    moo_ui_object_class_new_action (klass, "ExecuteSelection",
                                    "name", "Execute Selection",
                                    "label", "_Execute Selection",
                                    "tooltip", "Execute Selection",
                                    "icon-stock-id", GTK_STOCK_EXECUTE,
                                    "accel", "<shift><alt>Return",
                                    "closure::callback", execute_selection,
                                    NULL);
#endif /* !USE_PYTHON */

    moo_ui_object_class_new_action (klass, "Terminal",
                                    "name", "Terminal",
                                    "label", "_Terminal",
                                    "tooltip", "Terminal",
                                    "icon-stock-id", MOO_STOCK_TERMINAL,
                                    "closure::callback", open_terminal,
                                    NULL);

    g_type_class_unref (klass);
}


static void new_editor (MooApp *app)
{
    g_return_if_fail (app != NULL);
    gtk_window_present (GTK_WINDOW (moo_editor_new_window (app->priv->editor)));
}

static void open_in_editor (MooTermWindow *terminal)
{
    MooApp *app = moo_app_get_instance ();
    g_return_if_fail (app != NULL);
    moo_editor_open (app->priv->editor, NULL, GTK_WIDGET (terminal), NULL);
}


static void install_terminal_actions (void)
{
    GObjectClass *klass = g_type_class_ref (MOO_TYPE_TERM_WINDOW);

    g_return_if_fail (klass != NULL);

    moo_ui_object_class_new_action (klass, "NewEditor",
                                    "name", "New Editor",
                                    "label", "_New Editor",
                                    "tooltip", "New Editor",
                                    "icon-stock-id", GTK_STOCK_EDIT,
                                    "accel", "<Alt>E",
                                    "closure::callback", new_editor,
                                    "closure::proxy-func", moo_app_get_instance,
                                    NULL);

    moo_ui_object_class_new_action (klass, "OpenInEditor",
                                    "name", "Open In Editor",
                                    "label", "_Open In Editor",
                                    "tooltip", "Open In Editor",
                                    "icon-stock-id", GTK_STOCK_OPEN,
                                    "accel", "<Alt>O",
                                    "closure::callback", open_in_editor,
                                    NULL);

    g_type_class_unref (klass);
}


static void     all_editors_closed      (MooApp         *app)
{
    MooAppWindowPolicy policy = app->priv->window_policy;
    gboolean quit = (policy & MOO_APP_QUIT_ON_CLOSE_ALL_EDITORS) ||
            ((policy & MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS) && !app->priv->terminals);

    if (quit)
        moo_app_quit (app);
}


MooUIXML        *moo_app_get_ui_xml             (MooApp     *app)
{
    g_return_val_if_fail (MOO_IS_APP (app), NULL);

    if (!app->priv->ui_xml)
        app->priv->ui_xml = moo_ui_xml_new ();

    return app->priv->ui_xml;
}


void             moo_app_set_ui_xml             (MooApp     *app,
                                                 MooUIXML   *xml)
{
    GSList *l;

    g_return_if_fail (MOO_IS_APP (app));

    if (app->priv->ui_xml == xml)
        return;

    if (app->priv->ui_xml)
        g_object_unref (app->priv->ui_xml);
    app->priv->ui_xml = xml;
    if (xml)
        g_object_ref (app->priv->ui_xml);

    if (app->priv->editor)
        moo_editor_set_ui_xml (app->priv->editor, xml);
    for (l = app->priv->terminals; l != NULL; l = l->next)
        moo_ui_object_set_ui_xml (MOO_UI_OBJECT (l->data), xml);
}


GType            moo_app_window_policy_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        static const GFlagsValue values[] = {
            { MOO_APP_ONE_EDITOR, (char*)"MOO_APP_ONE_EDITOR", (char*)"one-editor" },
            { MOO_APP_MANY_EDITORS, (char*)"MOO_APP_MANY_EDITORS", (char*)"many-editors" },
            { MOO_APP_USE_EDITOR, (char*)"MOO_APP_USE_EDITOR", (char*)"use-editor" },
            { MOO_APP_ONE_TERMINAL, (char*)"MOO_APP_ONE_TERMINAL", (char*)"one-terminal" },
            { MOO_APP_MANY_TERMINALS, (char*)"MOO_APP_MANY_TERMINALS", (char*)"many-terminals" },
            { MOO_APP_USE_TERMINAL, (char*)"MOO_APP_USE_TERMINAL", (char*)"use-terminal" },
            { MOO_APP_QUIT_ON_CLOSE_ALL_EDITORS, (char*)"MOO_APP_QUIT_ON_CLOSE_ALL_EDITORS", (char*)"quit-on-close-all-editors" },
            { MOO_APP_QUIT_ON_CLOSE_ALL_TERMINALS, (char*)"MOO_APP_QUIT_ON_CLOSE_ALL_TERMINALS", (char*)"quit-on-close-all-terminals" },
            { MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS, (char*)"MOO_APP_QUIT_ON_CLOSE_ALL_WINDOWS", (char*)"quit-on-close-all-windows" },
            { 0, NULL, NULL }
        };
        type = g_flags_register_static ("MooAppWindowPolicy", values);
    }

    return type;
}


static MooAppInfo  *moo_app_info_copy   (MooAppInfo *info)
{
    MooAppInfo *copy;

    g_return_val_if_fail (info != NULL, NULL);

    copy = g_new (MooAppInfo, 1);

    copy->short_name = g_strdup (info->short_name);
    copy->full_name = g_strdup (info->full_name);
    copy->description = g_strdup (info->description);
    copy->version = g_strdup (info->version);
    copy->website = g_strdup (info->website);
    copy->website_label = g_strdup (info->website_label);
    copy->app_dir = g_strdup (info->app_dir);
    copy->rc_file = g_strdup (info->rc_file);

    return copy;
}


static void         moo_app_info_free   (MooAppInfo *info)
{
    if (info)
    {
        g_free (info->short_name);
        g_free (info->full_name);
        g_free (info->description);
        g_free (info->version);
        g_free (info->website);
        g_free (info->website_label);
        g_free (info->app_dir);
        g_free (info->rc_file);
        g_free (info);
    }
}


GType            moo_app_info_get_type          (void)
{
    static GType type = 0;
    if (!type)
        g_boxed_type_register_static ("MooAppInfo",
                                      (GBoxedCopyFunc) moo_app_info_copy,
                                      (GBoxedFreeFunc) moo_app_info_free);
    return type;
}


#ifdef USE_PYTHON
static void     execute_selection       (MooEditWindow  *window)
{
    MooEdit *edit;
    char *text;

    edit = moo_edit_window_get_active_doc (window);

    g_return_if_fail (edit != NULL);

    text = moo_edit_get_selection (edit);
    if (!text)
        text = moo_edit_get_text (edit);

    if (text)
    {
        moo_app_python_run_string (moo_app_get_instance (), text);
        g_free (text);
    }
}
#endif
