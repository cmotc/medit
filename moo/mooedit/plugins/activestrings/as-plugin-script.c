/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 *   as-plugin-script.c
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

#include "as-plugin-script.h"


static void as_plugin_context_init_api  (ASPluginContext    *ctx);


G_DEFINE_TYPE (ASPluginContext, _as_plugin_context, AS_TYPE_CONTEXT)


static void
_as_plugin_context_init (ASPluginContext *ctx)
{
    as_plugin_context_init_api (ctx);
}


static void
_as_plugin_context_class_init (G_GNUC_UNUSED ASPluginContextClass *klass)
{
}


ASContext *
_as_plugin_context_new (void)
{
    return g_object_new (AS_TYPE_PLUGIN_CONTEXT, NULL);
}


static void
as_plugin_context_setup (ASPluginContext *ctx,
                         MooEdit         *doc,
                         char            *match,
                         char           **parens,
                         guint            n_parens)
{
    guint i;
    ASValue *val;

    val = as_value_string (match);
    as_context_assign_positional (AS_CONTEXT (ctx), 0, val);
    as_value_unref (val);

    for (i = 0; i < n_parens; ++i)
    {
        val = as_value_string (parens[i]);
        as_context_assign_positional (AS_CONTEXT (ctx), i + 1, val);
        as_value_unref (val);
    }

    ctx->doc = g_object_ref (doc);
}


static void
as_plugin_context_clear (ASPluginContext *ctx,
                         guint            n_parens)
{
    guint i;

    for (i = 0; i < n_parens + 1; ++i)
        as_context_assign_positional (AS_CONTEXT (ctx), i, NULL);

    g_object_ref (ctx->doc);
    ctx->doc = NULL;
}


gboolean
_as_plugin_context_exec (ASContext      *ctx,
                         ASNode         *script,
                         MooEdit        *doc,
                         GtkTextIter    *insert,
                         char           *match,
                         char          **parens,
                         guint           n_parens)
{
    ASValue *val;
    gboolean success;

    g_return_val_if_fail (AS_IS_PLUGIN_CONTEXT (ctx), FALSE);
    g_return_val_if_fail (AS_IS_NODE (script), FALSE);
    g_return_val_if_fail (MOO_IS_EDIT (doc), FALSE);
    g_return_val_if_fail (insert != NULL, FALSE);
    g_return_val_if_fail (match != NULL, FALSE);
    g_return_val_if_fail (!n_parens || parens, FALSE);

    as_plugin_context_setup (AS_PLUGIN_CONTEXT (ctx),
                             doc, match, parens, n_parens);

    val = as_node_eval (script, ctx);
    success = val != NULL;

    if (val)
        as_value_unref (val);

    if (!success)
    {
        g_print ("%s\n", as_context_get_error_msg (ctx));
        as_context_clear_error (ctx);
    }

    as_plugin_context_clear (AS_PLUGIN_CONTEXT (ctx), n_parens);

    return success;
}


enum {
    FUNC_BS,
    FUNC_DEL,
    FUNC_INS,
    FUNC_UP,
    FUNC_DOWN,
    FUNC_LEFT,
    FUNC_RIGHT,
    FUNC_SEL,
    N_BUILTIN_FUNCS
};


static const char *builtin_func_names[N_BUILTIN_FUNCS] = {
    "bs", "del", "ins", "up", "down", "left", "right", "sel"
};

static ASFunc *builtin_funcs[N_BUILTIN_FUNCS];


static gboolean
check_one_arg (ASValue          **args,
               guint              n_args,
               ASPluginContext   *ctx,
               gboolean           nonnegative,
               int               *dest,
               int                default_val)
{
    int val;

    if (n_args > 1)
    {
        as_context_set_error (AS_CONTEXT (ctx), AS_ERROR_TYPE,
                              "number of args must be zero or one");
        return FALSE;
    }

    if (!n_args)
    {
        *dest = default_val;
        return TRUE;
    }

    if (!as_value_get_int (args[0], &val))
    {
        as_context_set_error (AS_CONTEXT (ctx), AS_ERROR_TYPE,
                              "argument must be integer");
        return FALSE;
    }

    if (nonnegative && val < 0)
    {
        as_context_set_error (AS_CONTEXT (ctx), AS_ERROR_VALUE,
                              "argument must be non-negative");
        return FALSE;
    }

    *dest = val;
    return TRUE;
}


static ASValue *
cfunc_bs (ASValue          **args,
          guint              n_args,
          ASPluginContext   *ctx)
{
    int n;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;

    if (!check_one_arg (args, n_args, ctx, TRUE, &n, 1))
        return NULL;

    if (!n)
        return as_value_none ();

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ctx->doc));

    if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
        gtk_text_buffer_delete (buffer, &start, &end);
        n--;
    }

    if (n)
    {
        gtk_text_iter_backward_chars (&start, n);
        gtk_text_buffer_delete (buffer, &start, &end);
    }

    return as_value_none ();
}


static ASValue *
cfunc_del (ASValue          **args,
           guint              n_args,
           ASPluginContext   *ctx)
{
    int n;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;

    if (!check_one_arg (args, n_args, ctx, TRUE, &n, 1))
        return NULL;

    if (!n)
        return as_value_none ();

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ctx->doc));

    if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
        gtk_text_buffer_delete (buffer, &start, &end);
        n--;
    }

    if (n)
    {
        gtk_text_iter_forward_chars (&end, n);
        gtk_text_buffer_delete (buffer, &start, &end);
    }

    return as_value_none ();
}


static void
get_cursor (MooEdit *doc,
            int     *line,
            int     *col)
{
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (doc));
    gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                      gtk_text_buffer_get_insert (buffer));

    *line = gtk_text_iter_get_line (&iter);
    *col = gtk_text_iter_get_line_offset (&iter);
}


static ASValue *
cfunc_up (ASValue          **args,
          guint              n_args,
          ASPluginContext   *ctx)
{
    int line, col, n;

    if (!check_one_arg (args, n_args, ctx, FALSE, &n, 1))
        return NULL;

    if (!n)
        return as_value_none ();

    get_cursor (ctx->doc, &line, &col);

    if (line > 0)
        moo_text_view_move_cursor (MOO_TEXT_VIEW (ctx->doc),
                                   MAX (line - n, 0), col,
                                   FALSE);

    return as_value_none ();
}


static ASValue *
cfunc_down (ASValue          **args,
            guint              n_args,
            ASPluginContext   *ctx)
{
    int line, col, n, line_count;
    GtkTextBuffer *buffer;

    if (!check_one_arg (args, n_args, ctx, FALSE, &n, 1))
        return NULL;

    if (!n)
        return as_value_none ();

    get_cursor (ctx->doc, &line, &col);
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ctx->doc));
    line_count = gtk_text_buffer_get_line_count (buffer);

    moo_text_view_move_cursor (MOO_TEXT_VIEW (ctx->doc),
                               MIN (line + n, line_count - 1), col,
                               FALSE);

    return as_value_none ();
}


static ASValue *
cfunc_sel (ASValue           *arg,
           ASPluginContext   *ctx)
{
    int n;
    GtkTextBuffer *buffer;
    GtkTextIter start, end;

    if (!as_value_get_int (arg, &n))
        return as_context_set_error (AS_CONTEXT (ctx), AS_ERROR_TYPE,
                                     "argument must be integer");

    if (!n)
        return as_context_set_error (AS_CONTEXT (ctx), AS_ERROR_TYPE,
                                     "argument must be non zero integer");

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ctx->doc));
    gtk_text_buffer_get_iter_at_mark (buffer, &start,
                                      gtk_text_buffer_get_insert (buffer));
    end = start;
    gtk_text_iter_forward_chars (&end, n);

    gtk_text_buffer_select_range (buffer, &end, &start);

    return as_value_none ();
}


static void
move_cursor (MooEdit *doc,
             int      howmany)
{
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (doc));
    gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                      gtk_text_buffer_get_insert (buffer));

    gtk_text_iter_forward_chars (&iter, howmany);
    gtk_text_buffer_place_cursor (buffer, &iter);
}


static ASValue *
cfunc_left (ASValue          **args,
            guint              n_args,
            ASPluginContext   *ctx)
{
    int n;

    if (!check_one_arg (args, n_args, ctx, FALSE, &n, 1))
        return NULL;

    move_cursor (ctx->doc, -n);
    return as_value_none ();
}


static ASValue *
cfunc_right (ASValue          **args,
             guint              n_args,
             ASPluginContext   *ctx)
{
    int n;

    if (!check_one_arg (args, n_args, ctx, FALSE, &n, 1))
        return NULL;

    move_cursor (ctx->doc, n);
    return as_value_none ();
}


static ASValue *
cfunc_ins (ASValue          **args,
           guint              n_args,
           ASPluginContext   *ctx)
{
    guint i;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;

    if (!n_args)
        return as_value_none ();

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ctx->doc));

    if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
        gtk_text_buffer_delete (buffer, &start, &end);

    for (i = 0; i < n_args; ++i)
    {
        char *s = as_value_print (args[i]);
        gtk_text_buffer_insert (buffer, &start, s, -1);
        g_free (s);
    }

    return as_value_none ();
}


static void
init_api (void)
{
    if (builtin_funcs[0])
        return;

    builtin_funcs[FUNC_BS] = as_cfunc_new_var ((ASCFunc_Var) cfunc_bs);
    builtin_funcs[FUNC_DEL] = as_cfunc_new_var ((ASCFunc_Var) cfunc_del);
    builtin_funcs[FUNC_INS] = as_cfunc_new_var ((ASCFunc_Var) cfunc_ins);
    builtin_funcs[FUNC_UP] = as_cfunc_new_var ((ASCFunc_Var) cfunc_up);
    builtin_funcs[FUNC_DOWN] = as_cfunc_new_var ((ASCFunc_Var) cfunc_down);
    builtin_funcs[FUNC_LEFT] = as_cfunc_new_var ((ASCFunc_Var) cfunc_left);
    builtin_funcs[FUNC_RIGHT] = as_cfunc_new_var ((ASCFunc_Var) cfunc_right);
    builtin_funcs[FUNC_SEL] = as_cfunc_new_1 ((ASCFunc_1) cfunc_sel);
}


static void
as_plugin_context_init_api (ASPluginContext *ctx)
{
    guint i;

    init_api ();

    for (i = 0; i < N_BUILTIN_FUNCS; ++i)
        as_context_set_func (AS_CONTEXT (ctx),
                             builtin_func_names[i],
                             builtin_funcs[i]);
}
