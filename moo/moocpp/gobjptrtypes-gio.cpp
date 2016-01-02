/*
 *   moocpp/gobjptrtypes-gio.cpp
 *
 *   Copyright (C) 2004-2016 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#include "moocpp/gobjptrtypes-gio.h"

using namespace moo;

using FilePtr = gobjptr<GFile>;

static void test()
{
    {
    FilePtr x;
    x.g_file();
    }

    {
    FilePtr x;
    x.new_for_path("");
    FilePtr f = FilePtr::new_for_path("");
    f->set("foo", 1, nullptr);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// mg_gobjptr_methods<GFile>
//

const FilePtr& mg_gobjptr_methods<GFile>::self() const
{
    return static_cast<const FilePtr&>(*this);
}

GFile* mg_gobjptr_methods<GFile>::g_file() const
{
    return self().get();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// mg_gobjptr_accessor<GFile>
//

const FilePtr& mg_gobjptr_accessor<GFile>::self() const
{
    return FilePtr::from_accessor(*this);
}

GFile* mg_gobjptr_accessor<GFile>::g() const
{
    return self().get();
}

FilePtr mg_gobjptr_accessor<GFile>::dup() const
{
    return FilePtr::wrap_new(g_file_dup(g()));
}

bool mg_gobjptr_accessor<GFile>::equal(GFile* file2) const
{
    return g_file_equal(g(), file2);
}

char* mg_gobjptr_accessor<GFile>::get_basename() const
{
    return g_file_get_basename(g());
}

char* mg_gobjptr_accessor<GFile>::get_path() const
{
    return g_file_get_path(g());
}

char* mg_gobjptr_accessor<GFile>::get_uri() const
{
    return g_file_get_uri(g());
}

char* mg_gobjptr_accessor<GFile>::get_parse_name() const
{
    return g_file_get_parse_name(g());
}

FilePtr mg_gobjptr_accessor<GFile>::get_parent() const
{
    return FilePtr::wrap_new(g_file_get_parent(g()));
}

bool mg_gobjptr_accessor<GFile>::has_parent(GFile* parent) const
{
    return g_file_has_parent(g(), parent);
}

FilePtr mg_gobjptr_accessor<GFile>::get_child(const char* name) const
{
    return FilePtr::wrap_new(g_file_get_child(g(), name));
}

FilePtr mg_gobjptr_accessor<GFile>::get_child_for_display_name(const char* display_name, GError** error) const
{
    return FilePtr::wrap_new(g_file_get_child_for_display_name(g(), display_name, error));
}

bool mg_gobjptr_accessor<GFile>::has_prefix(GFile* prefix) const
{
    return g_file_has_prefix(g(), prefix);
}

char* mg_gobjptr_accessor<GFile>::get_relative_path(GFile* descendant) const
{
    return g_file_get_relative_path(g(), descendant);
}

FilePtr mg_gobjptr_accessor<GFile>::resolve_relative_path(const char *relative_path) const
{
    return FilePtr::wrap_new(g_file_resolve_relative_path(g(), relative_path));
}

bool mg_gobjptr_accessor<GFile>::is_native() const
{
    return g_file_is_native(g());
}

bool mg_gobjptr_accessor<GFile>::has_uri_scheme(const char *uri_scheme) const
{
    return g_file_has_uri_scheme(g(), uri_scheme);
}

char* mg_gobjptr_accessor<GFile>::get_uri_scheme() const
{
    return g_file_get_uri_scheme(g());
}

bool mg_gobjptr_accessor<GFile>::query_exists(GCancellable* cancellable) const
{
    return g_file_query_exists(g(), cancellable);
}

GFileType mg_gobjptr_accessor<GFile>::query_file_type(GFileQueryInfoFlags flags, GCancellable* cancellable) const
{
    return g_file_query_file_type(g(), flags, cancellable);
}

GFileInfo* mg_gobjptr_accessor<GFile>::query_info(const char *attributes, GFileQueryInfoFlags flags, GCancellable* cancellable, GError** error) const
{
    return g_file_query_info(g(), attributes, flags, cancellable, error);
}

GFileInfo* mg_gobjptr_accessor<GFile>::query_filesystem_info(const char *attributes, GCancellable* cancellable, GError** error) const
{
    return g_file_query_filesystem_info(g(), attributes, cancellable, error);
}

GFileEnumerator* mg_gobjptr_accessor<GFile>::enumerate_children(const char *attributes, GFileQueryInfoFlags flags, GCancellable* cancellable, GError** error) const
{
    return g_file_enumerate_children(g(), attributes, flags, cancellable, error);
}

FilePtr mg_gobjptr_accessor<GFile>::set_display_name(const char* display_name, GCancellable* cancellable, GError** error) const
{
    return FilePtr::wrap_new(g_file_set_display_name(g(), display_name, cancellable, error));
}

bool mg_gobjptr_accessor<GFile>::delete_(GCancellable* cancellable, GError** error) const
{
    return g_file_delete(g(), cancellable, error);
}

bool mg_gobjptr_accessor<GFile>::trash(GCancellable* cancellable, GError** error) const
{
    return g_file_trash(g(), cancellable, error);
}

bool mg_gobjptr_accessor<GFile>::copy(GFile* destination, GFileCopyFlags flags, GCancellable* cancellable, GFileProgressCallback progress_callback, gpointer progress_callback_data, GError** error) const
{
    return g_file_copy(g(), destination, flags, cancellable, progress_callback, progress_callback_data, error);
}

bool mg_gobjptr_accessor<GFile>::move(GFile* destination, GFileCopyFlags flags, GCancellable* cancellable, GFileProgressCallback progress_callback, gpointer progress_callback_data, GError** error) const
{
    return g_file_move(g(), destination, flags, cancellable, progress_callback, progress_callback_data, error);
}

bool mg_gobjptr_accessor<GFile>::make_directory(GCancellable* cancellable, GError** error) const
{
    return g_file_make_directory(g(), cancellable, error);
}

bool mg_gobjptr_accessor<GFile>::make_directory_with_parents(GCancellable* cancellable, GError** error) const
{
    return g_file_make_directory_with_parents(g(), cancellable, error);
}

bool mg_gobjptr_accessor<GFile>::make_symbolic_link(const char *symlink_value, GCancellable* cancellable, GError** error) const
{
    return g_file_make_symbolic_link(g(), symlink_value, cancellable, error);
}

bool mg_gobjptr_accessor<GFile>::load_contents(GCancellable* cancellable, char** contents, gsize* length, char** etag_out, GError** error) const
{
    return g_file_load_contents(g(), cancellable, contents, length, etag_out, error);
}

bool mg_gobjptr_accessor<GFile>::replace_contents(const char* contents, gsize length, const char* etag, gboolean make_backup, GFileCreateFlags flags, char** new_etag, GCancellable* cancellable, GError** error) const
{
    return g_file_replace_contents(g(), contents, length, etag, make_backup, flags, new_etag, cancellable, error);
}
