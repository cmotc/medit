/*
 *   moocommand-exe-unix.c
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

#include "config.h"
#include "mooedit/moocommand-exe.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooedittools-glade.h"
#include "mooedit/moocmdview.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooglade.h"
#include "mooutils/mooutils-fs.h"
#include <gtk/gtk.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


#define MOO_COMMAND_EXE_MAX_INPUT       (MOO_COMMAND_EXE_INPUT_DOC + 1)
#define MOO_COMMAND_EXE_MAX_OUTPUT      (MOO_COMMAND_EXE_OUTPUT_NEW_DOC + 1)
#define MOO_COMMAND_EXE_INPUT_DEFAULT   MOO_COMMAND_EXE_INPUT_NONE
#define MOO_COMMAND_EXE_OUTPUT_DEFAULT  MOO_COMMAND_EXE_OUTPUT_NONE

struct _MooCommandExePrivate {
    char *cmd_line;
    MooCommandExeInput input;
    MooCommandExeOutput output;
};


G_DEFINE_TYPE (MooCommandExe, moo_command_exe, MOO_TYPE_COMMAND)

typedef MooCommandType MooCommandTypeExe;
typedef MooCommandTypeClass MooCommandTypeExeClass;
GType _moo_command_type_exe_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (MooCommandTypeExe, _moo_command_type_exe, MOO_TYPE_COMMAND_TYPE)


static void
moo_command_exe_init (MooCommandExe *cmd)
{
    cmd->priv = G_TYPE_INSTANCE_GET_PRIVATE (cmd,
                                             MOO_TYPE_COMMAND_EXE,
                                             MooCommandExePrivate);
}


static void
moo_command_exe_finalize (GObject *object)
{
    MooCommandExe *cmd = MOO_COMMAND_EXE (object);

    g_free (cmd->priv->cmd_line);

    G_OBJECT_CLASS (moo_command_exe_parent_class)->finalize (object);
}


static char *
get_lines (GtkTextView *doc)
{
    GtkTextBuffer *buffer;
    GtkTextIter start, end;

    buffer = gtk_text_view_get_buffer (doc);
    gtk_text_buffer_get_selection_bounds (buffer, &start, &end);

    gtk_text_iter_set_line_offset (&start, 0);

    if (!gtk_text_iter_starts_line (&end) || gtk_text_iter_equal (&start, &end))
        gtk_text_iter_forward_line (&end);

    gtk_text_buffer_select_range (buffer, &end, &start);

    return gtk_text_buffer_get_slice (buffer, &start, &end, TRUE);
}

static char *
get_input (MooCommandExe     *cmd,
           MooCommandContext *ctx)
{
    GtkTextView *doc = moo_command_context_get_doc (ctx);

    g_return_val_if_fail (cmd->priv->input == MOO_COMMAND_EXE_INPUT_NONE || doc != NULL, NULL);

    switch (cmd->priv->input)
    {
        case MOO_COMMAND_EXE_INPUT_NONE:
            return NULL;
        case MOO_COMMAND_EXE_INPUT_LINES:
            return get_lines (doc);
        case MOO_COMMAND_EXE_INPUT_SELECTION:
            return moo_text_view_get_selection (doc);
        case MOO_COMMAND_EXE_INPUT_DOC:
            return moo_text_view_get_text (doc);
    }

    g_return_val_if_reached (NULL);
}


static char *
save_temp (const char *data,
           gssize      length)
{
    int fd;
    GError *error = NULL;
    char *filename;

    fd = g_file_open_tmp ("medit-tmp-XXXXXX", &filename, &error);

    if (fd < 0)
    {
        g_critical ("%s: %s", G_STRLOC, error->message);
        g_error_free (error);
        return NULL;
    }

    close (fd);

    /* XXX */
    if (!_moo_save_file_utf8 (filename, data, length, &error))
    {
        g_critical ("%s: %s", G_STRLOC, error->message);
        g_error_free (error);
        g_free (filename);
        return NULL;
    }

    return filename;
}

static char *
make_cmd (MooCommandExe     *cmd,
          MooCommandContext *ctx)
{
    char *input;
    char *cmd_line = NULL;
    gsize input_len;

    input = get_input (cmd, ctx);
    input_len = input ? strlen (input) : 0;

    if (!input)
        return g_strdup (cmd->priv->cmd_line);

    if (input_len < 2048)
    {
        char *quoted;

        quoted = g_shell_quote (input);
        g_return_val_if_fail (quoted != NULL, NULL);

        cmd_line = g_strdup_printf ("echo -n %s | ( %s )", quoted, cmd->priv->cmd_line);
        g_free (quoted);
    }
    else
    {
        char *tmp_file = save_temp (input, input_len);

        if (tmp_file)
        {
            cmd_line = g_strdup_printf ("( %s ) < '%s' ; rm '%s'",
                                        cmd->priv->cmd_line, tmp_file, tmp_file);
            g_free (tmp_file);
        }
    }

    g_free (input);
    return cmd_line;
}


static void
run_in_pane (MooCommandExe     *cmd,
             MooCommandContext *ctx,
             const char        *working_dir,
             char             **envp)
{
    GtkTextView *doc;
    MooEditWindow *window;
    char *cmd_line;
    GtkWidget *output;

    doc = moo_command_context_get_doc (ctx);
    window = moo_command_context_get_window (ctx);

    g_return_if_fail (cmd->priv->input == MOO_COMMAND_EXE_INPUT_NONE || doc != NULL);
    g_return_if_fail (MOO_IS_EDIT_WINDOW (window));

    output = moo_edit_window_get_output (window);
    g_return_if_fail (output != NULL);

    /* XXX */
    if (!moo_cmd_view_running (MOO_CMD_VIEW (output)))
    {
        cmd_line = make_cmd (cmd, ctx);
        g_return_if_fail (cmd_line != NULL);

        moo_line_view_clear (MOO_LINE_VIEW (output));
        moo_edit_window_present_output (window);
        moo_cmd_view_run_command_full (MOO_CMD_VIEW (output),
                                       cmd_line, cmd->priv->cmd_line,
                                       working_dir, envp, "Command");

        g_free (cmd_line);
    }
}


static void
insert_text (MooTextView *view,
             const char  *text)
{
    GtkTextBuffer *buffer;
    GtkTextIter start, end;

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

    gtk_text_buffer_begin_user_action (buffer);

    if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
        gtk_text_buffer_delete (buffer, &start, &end);

    gtk_text_buffer_insert (buffer, &start, text, -1);

    gtk_text_buffer_end_user_action (buffer);
}


static void
set_variable (const char   *name,
              const GValue *gval,
              gpointer      array)
{
    char *freeme = NULL;
    const char *value = NULL;

    switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (gval)))
    {
        case G_TYPE_CHAR:
            freeme = g_strdup_printf ("%c", g_value_get_char (gval));
            break;
        case G_TYPE_UCHAR:
            freeme = g_strdup_printf ("%c", g_value_get_uchar (gval));
            break;
        case G_TYPE_INT:
            freeme = g_strdup_printf ("%d", g_value_get_int (gval));
            break;
        case G_TYPE_UINT:
            freeme = g_strdup_printf ("%u", g_value_get_uint (gval));
            break;
        case G_TYPE_LONG:
            freeme = g_strdup_printf ("%ld", g_value_get_long (gval));
            break;
        case G_TYPE_ULONG:
            freeme = g_strdup_printf ("%lu", g_value_get_ulong (gval));
            break;
        case G_TYPE_INT64:
            freeme = g_strdup_printf ("%" G_GINT64_FORMAT, g_value_get_int64 (gval));
            break;
        case G_TYPE_UINT64:
            freeme = g_strdup_printf ("%" G_GUINT64_FORMAT, g_value_get_uint64 (gval));
            break;
        case G_TYPE_STRING:
            value = g_value_get_string (gval);
            break;
        default:
            g_message ("ignoring value of type %s", g_type_name (G_VALUE_TYPE (gval)));
    }

    if (freeme)
        value = freeme;

    if (value)
        g_ptr_array_add (array, g_strdup_printf ("%s=%s", name, value));

    g_free (freeme);
}

static void
create_environment (MooCommandContext *ctx,
                    char             **working_dir,
                    char            ***envp)
{
    GPtrArray *array;
    MooEdit *doc;

    array = g_ptr_array_new ();
    moo_command_context_foreach (ctx, set_variable, array);

    if (array->len)
    {
        g_ptr_array_add (array, NULL);
        *envp = (char**) g_ptr_array_free (array, FALSE);
    }
    else
    {
        g_ptr_array_free (array, TRUE);
        *envp = NULL;
    }

    doc = moo_command_context_get_doc (ctx);

    if (doc && moo_edit_get_filename (doc))
        *working_dir = g_path_get_dirname (moo_edit_get_filename (doc));
    else
        *working_dir = NULL;
}


static gboolean
run_command (MooCommandExe     *cmd,
             MooCommandContext *ctx,
             const char        *working_dir,
             char             **envp,
             char             **output)
{
    GError *error = NULL;
    gboolean result;
    const char *argv[4] = {"/bin/sh", "-c", NULL, NULL};
    char *cmd_line;

    cmd_line = make_cmd (cmd, ctx);

    if (!cmd_line)
        return FALSE;

    argv[2] = cmd_line;
    result = g_spawn_sync (working_dir, (char**) argv, (char**) envp, 0,
                           NULL, NULL, output, NULL, NULL, &error);

    if (!result)
    {
        g_message ("could not run command: %s", error->message);
        g_error_free (error);
    }

    g_free (cmd_line);
    return result;
}


static void
moo_command_exe_run (MooCommand        *cmd_base,
                     MooCommandContext *ctx)
{
    MooEdit *doc;
    MooCommandExe *cmd = MOO_COMMAND_EXE (cmd_base);
    char **envp;
    char *output = NULL;
    char *working_dir;

    create_environment (ctx, &working_dir, &envp);

    if (cmd->priv->output == MOO_COMMAND_EXE_OUTPUT_PANE)
    {
        run_in_pane (cmd, ctx, working_dir, envp);
        goto out;
    }

    if (cmd->priv->output == MOO_COMMAND_EXE_OUTPUT_NONE)
    {
        run_command (cmd, ctx, working_dir, envp, NULL);
        goto out;
    }

    if (!run_command (cmd, ctx, working_dir, envp, &output))
        goto out;

    if (cmd->priv->output == MOO_COMMAND_EXE_OUTPUT_INSERT)
    {
        doc = moo_command_context_get_doc (ctx);
        g_return_if_fail (MOO_IS_EDIT (doc));
    }
    else
    {
        MooEditWindow *window = moo_command_context_get_window (ctx);
        doc = moo_editor_new_doc (moo_editor_instance (), window);
        g_return_if_fail (MOO_IS_EDIT (doc));
    }

    insert_text (MOO_TEXT_VIEW (doc), output);

out:
    g_free (working_dir);
    g_strfreev (envp);
    g_free (output);
}


static gboolean
moo_command_exe_check_sensitive (MooCommand *cmd_base,
                                 gpointer    doc,
                                 gpointer    window)
{
    MooCommandExe *cmd = MOO_COMMAND_EXE (cmd_base);
    MooCommandOptions options;

    options = cmd_base->options;

    switch (cmd->priv->input)
    {
        case MOO_COMMAND_EXE_INPUT_NONE:
            break;
        case MOO_COMMAND_EXE_INPUT_LINES:
        case MOO_COMMAND_EXE_INPUT_SELECTION:
        case MOO_COMMAND_EXE_INPUT_DOC:
            options |= MOO_COMMAND_NEED_DOC;
            break;
    }

    switch (cmd->priv->output)
    {
        case MOO_COMMAND_EXE_OUTPUT_NONE:
        case MOO_COMMAND_EXE_OUTPUT_NEW_DOC:
            break;
        case MOO_COMMAND_EXE_OUTPUT_PANE:
            options |= MOO_COMMAND_NEED_WINDOW;
            break;
        case MOO_COMMAND_EXE_OUTPUT_INSERT:
            options |= MOO_COMMAND_NEED_DOC;
            break;
    }

    moo_command_set_options (cmd_base, options);

    return MOO_COMMAND_CLASS (moo_command_exe_parent_class)->check_sensitive (cmd_base, doc, window);
}


static void
moo_command_exe_class_init (MooCommandExeClass *klass)
{
    MooCommandType *type;

    G_OBJECT_CLASS(klass)->finalize = moo_command_exe_finalize;
    MOO_COMMAND_CLASS(klass)->run = moo_command_exe_run;
    MOO_COMMAND_CLASS(klass)->check_sensitive = moo_command_exe_check_sensitive;

    g_type_class_add_private (klass, sizeof (MooCommandExePrivate));

    type = g_object_new (_moo_command_type_exe_get_type (), NULL);
    moo_command_type_register ("exe", _("Shell command"), type);
    g_object_unref (type);
}


static void
_moo_command_type_exe_init (G_GNUC_UNUSED MooCommandTypeExe *type)
{
}


static gboolean
parse_input (const char *string,
             int        *input)
{
    *input = MOO_COMMAND_EXE_INPUT_DEFAULT;

    if (!string || !string[0])
        return TRUE;

    if (!strcmp (string, "none"))
        *input = MOO_COMMAND_EXE_INPUT_NONE;
    else if (!strcmp (string, "lines"))
        *input = MOO_COMMAND_EXE_INPUT_LINES;
    else if (!strcmp (string, "selection"))
        *input = MOO_COMMAND_EXE_INPUT_SELECTION;
    else if (!strcmp (string, "doc"))
        *input = MOO_COMMAND_EXE_INPUT_DOC;
    else
    {
        g_warning ("unknown input type %s", string);
        return FALSE;
    }

    return TRUE;
}

static gboolean
parse_output (const char *string,
              int        *output)
{
    *output = MOO_COMMAND_EXE_OUTPUT_DEFAULT;

    if (!string || !string[0])
        return TRUE;

    if (!strcmp (string, "none"))
        *output = MOO_COMMAND_EXE_OUTPUT_NONE;
    else if (!strcmp (string, "pane"))
        *output = MOO_COMMAND_EXE_OUTPUT_PANE;
    else if (!strcmp (string, "insert"))
        *output = MOO_COMMAND_EXE_OUTPUT_INSERT;
    else if (!strcmp (string, "new-doc") ||
             !strcmp (string, "new_doc") ||
             !strcmp (string, "new"))
        *output = MOO_COMMAND_EXE_OUTPUT_NEW_DOC;
    else
    {
        g_warning ("unknown output type %s", string);
        return FALSE;
    }

    return TRUE;
}

static MooCommand *
exe_type_create_command (G_GNUC_UNUSED MooCommandType *type,
                         MooCommandData *data,
                         const char     *options)
{
    MooCommand *cmd;
    const char *cmd_line;
    int input, output;

    cmd_line = moo_command_data_get (data, "cmd-line");
    g_return_val_if_fail (cmd_line && *cmd_line, NULL);

    if (!parse_input (moo_command_data_get (data, "input"), &input))
        g_return_val_if_reached (NULL);
    if (!parse_output (moo_command_data_get (data, "output"), &output))
        g_return_val_if_reached (NULL);

    cmd = moo_command_exe_new (cmd_line, moo_command_options_parse (options), input, output);
    g_return_val_if_fail (cmd != NULL, NULL);

    return cmd;
}


static void
init_combo (GtkComboBox *combo,
            const char **items,
            guint        n_items)
{
    GtkListStore *store;
    GtkCellRenderer *cell;
    guint i;

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 0, NULL);

    store = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));

    for (i = 0; i < n_items; ++i)
    {
        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, 0, _(items[i]), -1);
    }

    g_object_unref (store);
}

static GtkWidget *
exe_type_create_widget (G_GNUC_UNUSED MooCommandType *type)
{
    GtkWidget *page;
    MooGladeXML *xml;
    MooTextView *textview;
    MooLangMgr *mgr;
    MooLang *lang;

    static const char *input_names[4] = {N_("None"), N_("Selected lines"), N_("Selection"), N_("Whole document")};
    static const char *output_names[4] = {N_("None"), N_("Output pane"), N_("Insert into the document"), N_("New document")};

    xml = moo_glade_xml_new_empty (GETTEXT_PACKAGE);
    moo_glade_xml_map_id (xml, "textview", MOO_TYPE_TEXT_VIEW);
    moo_glade_xml_parse_memory (xml, MOO_EDIT_TOOLS_GLADE_XML, -1, "exe_page", NULL);
    page = moo_glade_xml_get_widget (xml, "exe_page");
    g_return_val_if_fail (page != NULL, NULL);

    textview = moo_glade_xml_get_widget (xml, "textview");
    moo_text_view_set_font_from_string (textview, "Monospace");
    mgr = moo_editor_get_lang_mgr (moo_editor_instance ());
    lang = moo_lang_mgr_get_lang (mgr, "bash");
    moo_text_view_set_lang (textview, lang);

    init_combo (moo_glade_xml_get_widget (xml, "input"), input_names, G_N_ELEMENTS (input_names));
    init_combo (moo_glade_xml_get_widget (xml, "output"), output_names, G_N_ELEMENTS (output_names));

    g_object_set_data_full (G_OBJECT (page), "moo-glade-xml", xml, g_object_unref);
    return page;
}


static void
exe_type_load_data (G_GNUC_UNUSED MooCommandType *type,
                    GtkWidget      *page,
                    MooCommandData *data)
{
    MooGladeXML *xml;
    GtkTextView *textview;
    GtkTextBuffer *buffer;
    const char *cmd_line;
    int index;

    g_return_if_fail (data != NULL);

    xml = g_object_get_data (G_OBJECT (page), "moo-glade-xml");
    g_return_if_fail (xml != NULL);

    textview = moo_glade_xml_get_widget (xml, "textview");
    buffer = gtk_text_view_get_buffer (textview);

    cmd_line = moo_command_data_get (data, "cmd-line");
    gtk_text_buffer_set_text (buffer, cmd_line ? cmd_line : "", -1);

    parse_input (moo_command_data_get (data, "input"), &index);
    gtk_combo_box_set_active (moo_glade_xml_get_widget (xml, "input"), index);

    parse_output (moo_command_data_get (data, "output"), &index);
    gtk_combo_box_set_active (moo_glade_xml_get_widget (xml, "output"), index);
}


static gboolean
exe_type_save_data (G_GNUC_UNUSED MooCommandType *type,
                    GtkWidget      *page,
                    MooCommandData *data)
{
    MooGladeXML *xml;
    GtkTextView *textview;
    const char *cmd_line;
    char *new_cmd_line;
    gboolean changed = FALSE;
    int index, old_index;
    const char *input_strings[4] = {"none", "lines", "selection", "doc"};
    const char *output_strings[4] = {"none", "pane", "insert", "new-doc"};

    xml = g_object_get_data (G_OBJECT (page), "moo-glade-xml");
    textview = moo_glade_xml_get_widget (xml, "textview");
    g_assert (GTK_IS_TEXT_VIEW (textview));

    new_cmd_line = moo_text_view_get_text (textview);
    cmd_line = moo_command_data_get (data, "cmd-line");

    if (strcmp (cmd_line ? cmd_line : "", new_cmd_line ? new_cmd_line : "") != 0)
    {
        moo_command_data_set (data, "cmd-line", new_cmd_line);
        changed = TRUE;
    }

    index = gtk_combo_box_get_active (moo_glade_xml_get_widget (xml, "input"));
    parse_input (moo_command_data_get (data, "input"), &old_index);
    g_assert (0 <= index && index < MOO_COMMAND_EXE_MAX_INPUT);
    if (index != old_index)
    {
        moo_command_data_set (data, "input", input_strings[index]);
        changed = TRUE;
    }

    index = gtk_combo_box_get_active (moo_glade_xml_get_widget (xml, "output"));
    parse_output (moo_command_data_get (data, "output"), &old_index);
    g_assert (0 <= index && index < MOO_COMMAND_EXE_MAX_OUTPUT);
    if (index != old_index)
    {
        moo_command_data_set (data, "output", output_strings[index]);
        changed = TRUE;
    }

    g_free (new_cmd_line);
    return changed;
}


static gboolean
exe_type_data_equal (G_GNUC_UNUSED MooCommandType *type,
                     MooCommandData *data1,
                     MooCommandData *data2)
{
    guint i;
    const char *keys[] = {"cmd-line", "input", "output"};

    for (i = 0; i < G_N_ELEMENTS (keys); ++i)
    {
        const char *val1 = moo_command_data_get (data1, keys[i]);
        const char *val2 = moo_command_data_get (data2, keys[i]);
        if (strcmp (val1 ? val1 : "", val2 ? val2 : "") != 0)
            return FALSE;
    }

    return TRUE;
}


static void
_moo_command_type_exe_class_init (MooCommandTypeExeClass *klass)
{
    klass->create_command = exe_type_create_command;
    klass->create_widget = exe_type_create_widget;
    klass->load_data = exe_type_load_data;
    klass->save_data = exe_type_save_data;
    klass->data_equal = exe_type_data_equal;
}


MooCommand *
moo_command_exe_new (const char         *cmd_line,
                     MooCommandOptions   options,
                     MooCommandExeInput  input,
                     MooCommandExeInput  output)
{
    MooCommandExe *cmd;

    g_return_val_if_fail (cmd_line && cmd_line[0], NULL);
    g_return_val_if_fail (input < MOO_COMMAND_EXE_MAX_INPUT, NULL);
    g_return_val_if_fail (output < MOO_COMMAND_EXE_MAX_OUTPUT, NULL);

    cmd = g_object_new (MOO_TYPE_COMMAND_EXE, "options", options, NULL);

    cmd->priv->cmd_line = g_strdup (cmd_line);
    cmd->priv->input = input;
    cmd->priv->output = output;

    return MOO_COMMAND (cmd);
}
