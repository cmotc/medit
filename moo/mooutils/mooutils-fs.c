/*
 *   mooutils-fs.c
 *
 *   Copyright (C) 2004-2008 by Yevgen Muntyan <muntyan@tamu.edu>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License version 2.1 as published by the Free Software Foundation.
 *
 *   See COPYING file that comes with this distribution.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-debug.h"
#include "mooutils/mooutils-mem.h"
#include "mooutils/moocompat.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <glib/gstdio.h>

#ifdef __WIN32__
#include <windows.h>
#include <io.h>
#include <direct.h>
#endif

#ifndef S_IRWXU
#define S_IRWXU 0
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define BROKEN_NAME "<" "????" ">"


static int  _moo_rmdir  (const char *path);


/* XXX fix this */
gboolean
_moo_save_file_utf8 (const char *name,
                     const char *text,
                     gssize      len,
                     GError    **error)
{
    GIOChannel *file;
    GIOStatus status;
    gsize bytes_written;
    gsize real_len;

    file = g_io_channel_new_file (name, "w", error);

    if (!file)
        return FALSE;

    real_len = len < 0 ? strlen (text) : (gsize) len;

    status = g_io_channel_write_chars (file, text,
                                       len, &bytes_written,
                                       error);

    if (status != G_IO_STATUS_NORMAL || bytes_written != real_len)
    {
        /* glib #320668 */
        g_io_channel_flush (file, NULL);
        g_io_channel_shutdown (file, FALSE, NULL);
        g_io_channel_unref (file);
        return FALSE;
    }

    /* glib #320668 */
    g_io_channel_flush (file, NULL);
    g_io_channel_shutdown (file, FALSE, NULL);
    g_io_channel_unref (file);
    return TRUE;
}


#ifndef __WIN32__
static gboolean
rm_fr (const char *path,
       GError    **error)
{
    GError *error_here = NULL;
    char **argv;
    char *child_err;
    int status;

    argv = g_new0 (char*, 5);
    argv[0] = g_strdup ("/bin/rm");
    argv[1] = g_strdup ("-fr");
    argv[2] = g_strdup ("--");
    argv[3] = g_strdup (path);

    if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL,
                       NULL, NULL, NULL, &child_err, &status, &error_here))
    {
        g_set_error (error, MOO_FILE_ERROR, MOO_FILE_ERROR_FAILED,
                     "Could not run 'rm' command: %s",
                     error_here->message);
        g_error_free (error_here);
        g_strfreev (argv);
        return FALSE;
    }

    g_strfreev (argv);

    if (!WIFEXITED (status) || WEXITSTATUS (status))
    {
        g_set_error (error, MOO_FILE_ERROR,
                     MOO_FILE_ERROR_FAILED,
                     "'rm' command failed: %s",
                     child_err ? child_err : "");
        g_free (child_err);
        return FALSE;
    }
    else
    {
        g_free (child_err);
        return TRUE;
    }
}
#endif /* __WIN32__ */


#ifdef __WIN32__
static gboolean
rm_r (const char *path,
      GError    **error)
{
    GDir *dir;
    const char *file;
    gboolean success = TRUE;

    g_return_val_if_fail (path != NULL, FALSE);

    dir = g_dir_open (path, 0, error);

    if (!dir)
        return FALSE;

    while (success && (file = g_dir_read_name (dir)))
    {
        char *file_path = g_build_filename (path, file, NULL);

        errno = 0;

        if (_moo_remove (file_path))
        {
            int err = errno;

            switch (err)
            {
                case ENOTEMPTY:
                case EEXIST:
                    if (!rm_r (file_path, error))
                        success = FALSE;
                    break;

                default:
                    success = FALSE;
                    g_set_error (error, MOO_FILE_ERROR,
                                 _moo_file_error_from_errno (err),
                                 "Could not remove '%s': %s", file_path,
                                 g_strerror (err));
            }
        }

        g_free (file_path);
    }

    g_dir_close (dir);

    if (success)
    {
        errno = 0;

        if (_moo_remove (path) != 0)
        {
            int err = errno;
            success = FALSE;
            g_set_error (error, MOO_FILE_ERROR,
                         _moo_file_error_from_errno (err),
                         "Could not remove '%s': %s", path,
                         g_strerror (err));
        }
    }

    return success;
}
#endif


gboolean
_moo_remove_dir (const char *path,
                 gboolean    recursive,
                 GError    **error)
{
    g_return_val_if_fail (path != NULL, FALSE);

    if (!recursive)
    {
        errno = 0;

        if (_moo_rmdir (path) != 0)
        {
            int err = errno;
            char *path_utf8 = g_filename_display_name (path);
            g_set_error (error, MOO_FILE_ERROR,
                         _moo_file_error_from_errno (err),
                         "Could not remove '%s': %s",
                         path_utf8, g_strerror (err));
            g_free (path_utf8);
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }

#ifndef __WIN32__
    return rm_fr (path, error);
#else
    return rm_r (path, error);
#endif
}


int
_moo_mkdir_with_parents (const char *path)
{
    return g_mkdir_with_parents (path, S_IRWXU);
}


gboolean
_moo_create_dir (const char *path,
                 GError    **error)
{
    struct stat buf;
    char *utf8_path;

    g_return_val_if_fail (path != NULL, FALSE);

    errno = 0;

    if (g_stat (path, &buf) != 0 && errno != ENOENT)
    {
        int err_code = errno;
        utf8_path = g_filename_display_name (path);

        g_set_error (error,
                     MOO_FILE_ERROR,
                     _moo_file_error_from_errno (err_code),
                     "Could not create directory '%s': %s",
                     utf8_path, g_strerror (err_code));

        g_free (utf8_path);
        return FALSE;
    }

    if (errno != 0)
    {
        errno = 0;

        if (_moo_mkdir (path) == -1)
        {
            int err_code = errno;
            utf8_path = g_filename_display_name (path);

            g_set_error (error,
                         MOO_FILE_ERROR,
                         _moo_file_error_from_errno (err_code),
                         "Could not create directory '%s': %s",
                         utf8_path, g_strerror (err_code));

            g_free (utf8_path);
            return FALSE;
        }

        return TRUE;
    }

    if (S_ISDIR (buf.st_mode))
        return TRUE;

    utf8_path = g_filename_display_name (path);
    g_set_error (error, MOO_FILE_ERROR,
                 MOO_FILE_ERROR_ALREADY_EXISTS,
                 "Could not create directory '%s': %s",
                 utf8_path, g_strerror (EEXIST));
    g_free (utf8_path);

    return FALSE;
}


gboolean
_moo_rename_file (const char *path,
                  const char *new_path,
                  GError    **error)
{
    g_return_val_if_fail (path != NULL, FALSE);
    g_return_val_if_fail (new_path != NULL, FALSE);

    errno = 0;

    if (_moo_rename (path, new_path) != 0)
    {
        int err_code = errno;
        char *utf8_path = g_filename_display_name (path);
        char *utf8_new_path = g_filename_display_name (new_path);

        g_set_error (error,
                     MOO_FILE_ERROR,
                     _moo_file_error_from_errno (err_code),
                     "Could not rename file '%s' to '%s': %s",
                     utf8_path, utf8_new_path, g_strerror (err_code));

        g_free (utf8_path);
        g_free (utf8_new_path);
        return FALSE;
    }

    return TRUE;
}


GQuark
_moo_file_error_quark (void)
{
    static GQuark quark = 0;

    if (quark == 0)
        quark = g_quark_from_static_string ("moo-file-error-quark");

    return quark;
}


MooFileError
_moo_file_error_from_errno (int code)
{
    switch (code)
    {
        case EACCES:
        case EPERM:
            return MOO_FILE_ERROR_ACCESS_DENIED;
        case EEXIST:
            return MOO_FILE_ERROR_ALREADY_EXISTS;
#ifndef __WIN32__
        case ELOOP:
#endif
        case ENAMETOOLONG:
            return MOO_FILE_ERROR_BAD_FILENAME;
        case ENOENT:
            return MOO_FILE_ERROR_NONEXISTENT;
        case ENOTDIR:
            return MOO_FILE_ERROR_NOT_FOLDER;
        case EROFS:
            return MOO_FILE_ERROR_READONLY;
        case EXDEV:
            return MOO_FILE_ERROR_DIFFERENT_FS;
    }

    return MOO_FILE_ERROR_FAILED;
}


char **
moo_filenames_from_locale (char **files)
{
    guint i;
    char **conv;

    if (!files)
        return NULL;

    conv = g_new0 (char*, g_strv_length (files) + 1);

    for (i = 0; files && *files; ++files)
    {
        conv[i] = moo_filename_from_locale (*files);

        if (!conv[i])
            g_warning ("%s: could not convert '%s' to UTF8", G_STRLOC, *files);
        else
            ++i;
    }

    return conv;
}

char *
moo_filename_from_locale (const char *file)
{
    g_return_val_if_fail (file != NULL, NULL);
#ifdef __WIN32__
    return g_locale_to_utf8 (file, -1, NULL, NULL, NULL);
#else
    return g_strdup (file);
#endif
}


#if 0
static int
_moo_chdir (const char *path)
{
#ifdef __WIN32__
    CCALL_1 (_chdir, _wchdir, path);
#else
    return chdir (path);
#endif
}

// char *
// _moo_normalize_file_path (const char *filename)
// {
//     char *freeme = NULL;
//     char *working_dir, *basename, *dirname;
//     char *real_filename = NULL;
//
//     g_return_val_if_fail (filename != NULL, NULL);
//
//     working_dir = g_get_current_dir ();
//     g_return_val_if_fail (working_dir != NULL, g_strdup (filename));
//
//     if (!g_path_is_absolute (filename))
//     {
//         freeme = g_build_filename (working_dir, filename, NULL);
//         filename = freeme;
//     }
//
//     /* It's an error to call this function for a directory, so if
//      * it's the case, just return what we got. */
//     if (g_file_test (filename, G_FILE_TEST_IS_DIR) ||
//         /* and this totally screws up dirname/basename */
//         g_str_has_suffix (filename, G_DIR_SEPARATOR_S))
//     {
//         if (!freeme)
//             freeme = g_strdup (filename);
//         g_free (working_dir);
//         return freeme;
//     }
//
//     dirname = g_path_get_dirname (filename);
//     basename = g_path_get_basename (filename);
//     g_return_val_if_fail (dirname && basename, g_strdup (filename));
//
//     errno = 0;
//
//     if (_moo_chdir (dirname) != 0)
//     {
//         int err = errno;
//         g_warning ("%s: %s", G_STRLOC, g_strerror (err));
//     }
//     else
//     {
//         char *real_dirname = g_get_current_dir ();
//         real_filename = g_build_filename (real_dirname, basename, NULL);
//         g_free (real_dirname);
//
//         errno = 0;
//
//         if (_moo_chdir (working_dir) != 0)
//         {
//             int err = errno;
//             g_warning ("%s: %s", G_STRLOC, g_strerror (err));
//         }
//     }
//
//     if (!real_filename)
//         real_filename = g_strdup (filename);
//
//     g_free (freeme);
//     g_free (dirname);
//     g_free (basename);
//     g_free (working_dir);
//
//     return real_filename;
// }
#endif


#ifndef __WIN32__

static char *
normalize_path_string (const char *path)
{
    GPtrArray *comps;
    gboolean first_slash;
    char **pieces, **p;
    char *normpath;

    g_return_val_if_fail (path != NULL, NULL);

    first_slash = (path[0] == G_DIR_SEPARATOR);

    pieces = g_strsplit (path, G_DIR_SEPARATOR_S, 0);
    g_return_val_if_fail (pieces != NULL, NULL);

    comps = g_ptr_array_new ();

    for (p = pieces; *p != NULL; ++p)
    {
        char *s = *p;
        gboolean push = TRUE;
        gboolean pop = FALSE;

        if (!strcmp (s, "") || !strcmp (s, "."))
        {
            push = FALSE;
        }
        else if (!strcmp (s, ".."))
        {
            if (!comps->len && first_slash)
            {
                push = FALSE;
            }
            else if (comps->len)
            {
                push = FALSE;
                pop = TRUE;
            }
        }

        if (pop)
        {
            g_free (comps->pdata[comps->len - 1]);
            g_ptr_array_remove_index (comps, comps->len - 1);
        }

        if (push)
            g_ptr_array_add (comps, g_strdup (s));
    }

    g_ptr_array_add (comps, NULL);

    if (comps->len == 1)
    {
        if (first_slash)
            normpath = g_strdup (G_DIR_SEPARATOR_S);
        else
            normpath = g_strdup (".");
    }
    else
    {
        char *tmp = g_strjoinv (G_DIR_SEPARATOR_S, (char**) comps->pdata);

        if (first_slash)
        {
            guint len = strlen (tmp);
            normpath = g_renew (char, tmp, len + 2);
            memmove (normpath + 1, normpath, len + 1);
            normpath[0] = G_DIR_SEPARATOR;
        }
        else
        {
            normpath = tmp;
        }
    }

    g_strfreev (pieces);
    g_strfreev ((char**) comps->pdata);
    g_ptr_array_free (comps, FALSE);

    return normpath;
}

static char *
normalize_full_path_unix (const char *path)
{
    guint len;
    char *normpath;

    g_return_val_if_fail (path != NULL, NULL);

    normpath = normalize_path_string (path);
    g_return_val_if_fail (normpath != NULL, NULL);

    len = strlen (normpath);
    g_return_val_if_fail (len > 0, normpath);

    if (len > 1 && normpath[len-1] == G_DIR_SEPARATOR)
        normpath[len-1] = 0;

    return normpath;
}

#endif /* !__WIN32__ */

#if defined(__WIN32__) || defined(MOO_ENABLE_TESTS)

static char *
normalize_full_path_win32 (const char *fullpath)
{
    char *prefix;
    char *path;
    guint slashes;

#ifndef __WIN32__
    /* for tests */
    if (!fullpath || !fullpath[0])
        return g_strdup (fullpath);
#endif

    if (fullpath[0] && fullpath[1] == ':')
    {
        prefix = g_strndup (fullpath, 3);
        prefix[2] = '\\';

        if (fullpath[2] == '\\')
            path = g_strdup (fullpath + 3);
        else
            path = g_strdup (fullpath + 2);
    }
    else
    {
        prefix = NULL;
        path = g_strdup (fullpath);
    }

    g_strdelimit (path, "/", '\\');

    for (slashes = 0; path[slashes] == '\\'; ++slashes) ;

    if (!prefix && slashes)
        prefix = g_strndup (path, slashes);

    if (slashes)
        memmove (path, path + slashes, strlen (path) + 1 - slashes);

    g_assert (prefix || *path);
    g_assert (*path != '\\');

    if (*path)
    {
        char **comps;
        guint n_comps, i;

        comps = g_strsplit (path, "\\", 0);
        n_comps = g_strv_length (comps);

        for (i = 0; i < n_comps; )
        {
            if (strcmp (comps[i], "") == 0 || strcmp (comps[i], ".") == 0 ||
                (i == 0 && strcmp (comps[i], "..") == 0))
            {
                g_free (comps[i]);
                MOO_ELMMOVE (comps + i, comps + i + 1, n_comps - i);
                n_comps -= 1;
            }
            else if (strcmp (comps[i], "..") == 0)
            {
                g_free (comps[i-1]);
                g_free (comps[i]);
                MOO_ELMMOVE (comps + i - 1, comps + i + 1, n_comps - i);
                n_comps -= 2;
                i -= 1;
            }
            else
            {
                i += 1;
            }
        }

        if (n_comps > 0)
        {
            char *tmp = g_strjoinv ("\\", comps);
            g_free (path);
            path = tmp;
        }
        else
        {
            path[0] = 0;
        }

        g_strfreev (comps);
    }

    if (*path)
    {
        if (prefix)
        {
            char *tmp = g_strconcat (prefix, path, NULL);
            g_free (path);
            path = tmp;
        }
    }
    else if (prefix)
    {
        g_free (path);
        path = prefix;
        prefix = NULL;
    }
    else
    {
        g_free (path);
        path = g_strdup (".");
    }

    g_free (prefix);
    return path;
}

#endif

static char *
normalize_path (const char *filename)
{
    char *freeme = NULL;
    char *norm_filename;

    g_assert (filename && filename[0]);

    if (!g_path_is_absolute (filename)
#ifdef __WIN32__
        /* 'C:' is an absolute path even if glib doesn't like it */
        && filename[1] != ':'
#endif
        )
    {
        char *working_dir = g_get_current_dir ();
        g_return_val_if_fail (working_dir != NULL, g_strdup (filename));
        freeme = g_build_filename (working_dir, filename, NULL);
        filename = freeme;
    }

#ifdef __WIN32__
    norm_filename = normalize_full_path_win32 (filename);
#else
    norm_filename = normalize_full_path_unix (filename);
#endif

    g_free (freeme);
    return norm_filename;
}

char *
_moo_normalize_file_path (const char *filename)
{
    g_return_val_if_fail (filename != NULL, NULL);
    /* empty filename is an error, but we don't want to crash here */
    g_return_val_if_fail (filename[0] != 0, g_strdup (""));
    return normalize_path (filename);
}

#if 0
char *
_moo_normalize_dir_path (const char *filename)
{
    return normalize_path (filename, TRUE);
}
#endif


#ifdef MOO_ENABLE_TESTS

#include <moo-tests.h>

static void
test_normalize_path_one (const char *path,
                         const char *expected,
                         char *(*func) (const char*),
                         const char *func_name)
{
    char *result;

    result = func (path);

    if (!moo_test_str_equal (result, expected))
        moo_test_failed ("%s(%s): expected %s, got %s",
                         func_name,
                         moo_test_str_format (path),
                         moo_test_str_format (expected),
                         moo_test_str_format (result));
    else
        moo_test_passed ("%s(%s) == %s",
                         func_name,
                         moo_test_str_format (path),
                         moo_test_str_format (expected));

    g_free (result);
}

static GPtrArray *
make_cases (gboolean unix_paths)
{
    guint i;
    char *current_dir;
    GPtrArray *paths = g_ptr_array_new ();

    const char *abs_files_common[] = {
        NULL, NULL,
        "", "",
    };

    const char *abs_files_unix[] = {
        "/usr", "/usr",
        "/usr/", "/usr",
        "///usr////", "/usr",
        "/", "/",
        "//", "/",
        "/usr/../bin/../usr", "/usr",
        "/usr/bin/../bin/.", "/usr/bin",
        "/..", "/",
        "/../../../", "/",
        "/../whatever/..", "/",
        "/../whatever/.././././", "/",
    };

    const char *abs_files_win32[] = {
        "C:", "C:\\",
        "C:\\", "C:\\",
        "C:\\foobar", "C:\\foobar",
        "C:\\foobar\\", "C:\\foobar",
        "C:\\foobar\\\\\\", "C:\\foobar",
        "C:/", "C:\\",
        "C:/foobar", "C:\\foobar",
        "C:/foobar//", "C:\\foobar",
        "C:/foobar///", "C:\\foobar",
        "\\\\.host\\dir\\root", "\\\\.host\\dir\\root",
        "\\\\.host\\dir\\root\\", "\\\\.host\\dir\\root",
        "\\\\.host\\dir\\root\\..\\..\\", "\\\\.host",
    };

    const char *rel_files_common[] = {
        ".", NULL,
        "././././/", NULL,
        "foobar", "foobar",
        "foobar/", "foobar",
        "foobar/..", NULL,
        "foobar/./..", NULL,
        "foobar/../", NULL,
        "foobar/./../", NULL,
    };

#ifndef __WIN32__
    const char *rel_files_unix[] = {
        "foobar/com", "foobar/com",
    };
#endif

#ifdef __WIN32__
    const char *rel_files_win32[] = {
        "foobar/com", "foobar\\com",
        ".\\.\\.\\.\\\\", NULL,
        "foobar\\", "foobar",
        "foobar\\..", NULL,
        "foobar\\.\\..", NULL,
        "foobar\\..\\", NULL,
        "foobar\\.\\..\\", NULL,
    };
#endif

    for (i = 0; i < G_N_ELEMENTS (abs_files_common); i += 2)
    {
        g_ptr_array_add (paths, g_strdup (abs_files_common[i]));
        g_ptr_array_add (paths, g_strdup (abs_files_common[i+1]));
    }

    if (unix_paths)
        for (i = 0; i < G_N_ELEMENTS (abs_files_unix); i += 2)
        {
            g_ptr_array_add (paths, g_strdup (abs_files_unix[i]));
            g_ptr_array_add (paths, g_strdup (abs_files_unix[i+1]));
        }
    else
        for (i = 0; i < G_N_ELEMENTS (abs_files_win32); i += 2)
        {
            g_ptr_array_add (paths, g_strdup (abs_files_win32[i]));
            g_ptr_array_add (paths, g_strdup (abs_files_win32[i+1]));
        }

    current_dir = g_get_current_dir ();

#ifndef __WIN32__
    if (unix_paths)
#endif
        for (i = 0; i < G_N_ELEMENTS (rel_files_common); i += 2)
        {
            g_ptr_array_add (paths, g_strdup (rel_files_common[i]));
            if (rel_files_common[i+1])
                g_ptr_array_add (paths, g_build_filename (current_dir, rel_files_common[i+1], NULL));
            else
                g_ptr_array_add (paths, g_strdup (current_dir));
        }

#ifndef __WIN32__
    if (unix_paths)
        for (i = 0; i < G_N_ELEMENTS (rel_files_unix); i += 2)
        {
            g_ptr_array_add (paths, g_strdup (rel_files_unix[i]));
            if (rel_files_unix[i+1])
                g_ptr_array_add (paths, g_build_filename (current_dir, rel_files_unix[i+1], NULL));
            else
                g_ptr_array_add (paths, g_strdup (current_dir));
        }
#endif

#ifdef __WIN32__
    for (i = 0; i < G_N_ELEMENTS (rel_files_win32); i += 2)
    {
        g_ptr_array_add (paths, g_strdup (rel_files_win32[i]));
        if (rel_files_win32[i+1])
            g_ptr_array_add (paths, g_build_filename (current_dir, rel_files_win32[i+1], NULL));
        else
            g_ptr_array_add (paths, g_strdup (current_dir));
    }
#endif

#ifndef __WIN32__
    if (unix_paths)
#endif
    {
        char *parent_dir = g_path_get_dirname (current_dir);
        g_ptr_array_add (paths, g_strdup (".."));
        g_ptr_array_add (paths, g_strdup (parent_dir));
        g_ptr_array_add (paths, g_strdup ("../"));
        g_ptr_array_add (paths, g_strdup (parent_dir));
        g_ptr_array_add (paths, g_strdup ("../."));
        g_ptr_array_add (paths, g_strdup (parent_dir));
        g_ptr_array_add (paths, g_strdup (".././"));
        g_ptr_array_add (paths, g_strdup (parent_dir));
#ifdef __WIN32__
        g_ptr_array_add (paths, g_strdup ("..\\"));
        g_ptr_array_add (paths, g_strdup (parent_dir));
        g_ptr_array_add (paths, g_strdup ("..\\."));
        g_ptr_array_add (paths, g_strdup (parent_dir));
        g_ptr_array_add (paths, g_strdup ("..\\.\\"));
        g_ptr_array_add (paths, g_strdup (parent_dir));
#endif
        g_free (parent_dir);
    }

    g_free (current_dir);
    return paths;
}

static void
test_normalize_file_path (void)
{
    GPtrArray *paths;
    guint i;

#ifndef __WIN32__
    paths = make_cases (TRUE);
#else
    paths = make_cases (FALSE);
#endif

    for (i = 0; i < paths->len; i += 2)
    {
        test_normalize_path_one (paths->pdata[i], paths->pdata[i+1],
                                 _moo_normalize_file_path,
                                 "_moo_normalize_file_path");
        g_free (paths->pdata[i]);
        g_free (paths->pdata[i+1]);
    }

    g_ptr_array_free (paths, TRUE);
}

#ifndef __WIN32__
static void
test_normalize_file_path_win32 (void)
{
    GPtrArray *paths;
    guint i;

    paths = make_cases (FALSE);

    for (i = 0; i < paths->len; i += 2)
    {
        test_normalize_path_one (paths->pdata[i], paths->pdata[i+1],
                                 normalize_full_path_win32,
                                 "normalize_full_path_win32");
        g_free (paths->pdata[i]);
        g_free (paths->pdata[i+1]);
    }

    g_ptr_array_free (paths, TRUE);
}
#endif

void
moo_test_mooutils_fs (void)
{
    CU_pSuite suite;

//     if (!(suite = CU_add_suite ("mooutils/mooutils-fs.c", init_suite, clean_suite)))
    if (!(suite = CU_add_suite ("mooutils/mooutils-fs.c", NULL, NULL)))
        g_error ("CU_add_suite() failed");

    if (!CU_add_test (suite, "test of _moo_normalize_file_path()", test_normalize_file_path))
        g_error ("CU_add_test() failed");
#ifndef __WIN32__
    if (!CU_add_test (suite, "test of normalize_full_path_win32()", test_normalize_file_path_win32))
        g_error ("CU_add_test() failed");
#endif
}

#endif /* MOO_ENABLE_TESTS */


/**********************************************************************/
/* MSLU for poor
 */

#ifdef __WIN32__
static gpointer
convert_filename (const char *filename,
                  gboolean   *use_wide_char_api)
{
    if (G_WIN32_HAVE_WIDECHAR_API ())
    {
        *use_wide_char_api = TRUE;
        return g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
    }
    else
    {
        *use_wide_char_api = FALSE;
        return g_locale_from_utf8 (filename, -1, NULL,
                                   NULL, NULL);
    }
}
#endif


#define CCALL_1(_AFunc, _WFunc, path)                           \
G_STMT_START {                                                  \
    gboolean use_wide_char_api;                                 \
    gpointer converted;                                         \
    int retval;                                                 \
    int save_errno;                                             \
                                                                \
    converted = convert_filename (path, &use_wide_char_api);    \
                                                                \
    if (!converted)                                             \
    {                                                           \
        errno = EINVAL;                                         \
        return -1;                                              \
    }                                                           \
                                                                \
    errno = 0;                                                  \
                                                                \
    if (use_wide_char_api)                                      \
        retval = _WFunc (converted);                            \
    else                                                        \
        retval = _AFunc (converted);                            \
                                                                \
    save_errno = errno;                                         \
    g_free (converted);                                         \
    errno = save_errno;                                         \
                                                                \
    return retval;                                              \
} G_STMT_END


int
_moo_unlink (const char *path)
{
#ifdef __WIN32__
    CCALL_1 (unlink, _wunlink, path);
#else
    return unlink (path);
#endif
}


static int
_moo_rmdir (const char *path)
{
#ifdef __WIN32__
    CCALL_1 (rmdir, _wrmdir, path);
#else
    return rmdir (path);
#endif
}


int
_moo_mkdir (const char *path)
{
#ifdef __WIN32__
    CCALL_1 (mkdir, _wmkdir, path);
#else
    return mkdir (path, S_IRWXU);
#endif
}


int
_moo_remove (const char *path)
{
#ifdef __WIN32__
    gboolean use_wide_char_api;
    gpointer converted;
    int retval;
    int save_errno;

    converted = convert_filename (path, &use_wide_char_api);

    if (!converted)
    {
        errno = EINVAL;
        return -1;
    }

    errno = 0;

    if (use_wide_char_api)
        retval = _wremove (converted);
    else
        retval = remove (converted);

    if (retval && errno == ENOENT)
    {
        if (use_wide_char_api)
            retval = _wrmdir (converted);
        else
            retval = rmdir (converted);
    }

    save_errno = errno;
    g_free (converted);
    errno = save_errno;

    return retval;
#else
    return remove (path);
#endif
}


gpointer
_moo_fopen (const char *path,
            const char *mode)
{
#ifdef __WIN32__
    gboolean use_wide_char_api;
    gpointer path_conv, mode_conv;
    FILE *retval;
    int save_errno;

    if (G_WIN32_HAVE_WIDECHAR_API ())
    {
        use_wide_char_api = TRUE;
        path_conv = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
        mode_conv = g_utf8_to_utf16 (mode, -1, NULL, NULL, NULL);
    }
    else
    {
        use_wide_char_api = FALSE;
        path_conv = g_locale_from_utf8 (path, -1, NULL, NULL, NULL);
        mode_conv = g_locale_from_utf8 (mode, -1, NULL, NULL, NULL);
    }

    if (!path_conv || !mode_conv)
    {
        g_free (path_conv);
        g_free (mode_conv);
        errno = EINVAL;
        return NULL;
    }

    errno = 0;

    if (use_wide_char_api)
        retval = _wfopen (path_conv, mode_conv);
    else
        retval = fopen (path_conv, mode_conv);

    save_errno = errno;
    g_free (path_conv);
    g_free (mode_conv);
    errno = save_errno;

    return retval;
#else
    return fopen (path, mode);
#endif
}


int
_moo_rename (const char *old_name,
             const char *new_name)
{
#ifdef __WIN32__
    gboolean use_wide_char_api;
    gpointer old_conv, new_conv;
    int retval, save_errno;

    if (G_WIN32_HAVE_WIDECHAR_API ())
    {
        use_wide_char_api = TRUE;
        old_conv = g_utf8_to_utf16 (old_name, -1, NULL, NULL, NULL);
        new_conv = g_utf8_to_utf16 (new_name, -1, NULL, NULL, NULL);
    }
    else
    {
        use_wide_char_api = FALSE;
        old_conv = g_locale_from_utf8 (old_name, -1, NULL, NULL, NULL);
        new_conv = g_locale_from_utf8 (new_name, -1, NULL, NULL, NULL);
    }

    if (!old_conv || !new_conv)
    {
        g_free (old_conv);
        g_free (new_conv);
        errno = EINVAL;
        return -1;
    }

    errno = 0;

    if (use_wide_char_api)
        retval = _wrename (old_conv, new_conv);
    else
        retval = rename (old_conv, new_conv);

    save_errno = errno;
    g_free (old_conv);
    g_free (new_conv);
    errno = save_errno;

    return retval;
#else
    return rename (old_name, new_name);
#endif
}


/***********************************************************************/
/* Glob matching
 */

#if 0 && defined(HAVE_FNMATCH_H)
#include <fnmatch.h>
#else
#define MOO_GLOB_REGEX
#include <glib/gregex.h>
#endif

typedef struct _MooGlob {
#ifdef MOO_GLOB_REGEX
    GRegex *re;
#else
    char *pattern;
#endif
} MooGlob;

#ifdef MOO_GLOB_REGEX
static char *
glob_to_re (const char *pattern)
{
    GString *string;
    const char *p, *piece, *bracket;
    char *escaped;

    g_return_val_if_fail (pattern != NULL, NULL);

    p = piece = pattern;
    string = g_string_new (NULL);

    while (*p)
    {
        switch (*p)
        {
            case '*':
                if (p != piece)
                    g_string_append_len (string, piece, p - piece);
                g_string_append (string, ".*");
                piece = ++p;
                break;

            case '?':
                if (p != piece)
                    g_string_append_len (string, piece, p - piece);
                g_string_append_c (string, '.');
                piece = ++p;
                break;

            case '[':
                if (!(bracket = strchr (p + 1, ']')))
                {
                    g_warning ("in %s: unmatched '['", pattern);
                    goto error;
                }

                if (p != piece)
                    g_string_append_len (string, piece, p - piece);

                g_string_append_c (string, '[');
                if (p[1] == '^')
                {
                    g_string_append_c (string, '^');
                    escaped = g_regex_escape_string (p + 2, bracket - p - 2);
                }
                else
                {
                    escaped = g_regex_escape_string (p + 1, bracket - p - 1);
                }
                g_string_append (string, escaped);
                g_free (escaped);
                g_string_append_c (string, ']');
                piece = p = bracket + 1;
                break;

            case '\\':
            case '|':
            case '(':
            case ')':
            case ']':
            case '{':
            case '}':
            case '^':
            case '$':
            case '+':
            case '.':
                if (p != piece)
                    g_string_append_len (string, piece, p - piece);
                g_string_append_c (string, '\\');
                g_string_append_c (string, *p);
                piece = ++p;
                break;

            default:
                p = g_utf8_next_char (p);
        }
    }

    if (*piece)
        g_string_append (string, piece);

    g_string_append_c (string, '$');

    if (0)
        _moo_message ("converted '%s' to '%s'\n", pattern, string->str);

    return g_string_free (string, FALSE);

error:
    g_string_free (string, TRUE);
    return NULL;
}


static MooGlob *
_moo_glob_new (const char *pattern)
{
    MooGlob *gl;
    GRegex *re;
    char *re_pattern;
    GRegexCompileFlags flags = 0;
    GError *error = NULL;

    g_return_val_if_fail (pattern != NULL, NULL);

#ifdef __WIN32__
    flags = G_REGEX_CASELESS;
#endif

    if (!(re_pattern = glob_to_re (pattern)))
        return NULL;

    re = g_regex_new (re_pattern, flags, 0, &error);

    g_free (re_pattern);

    if (error)
    {
        g_warning ("%s: %s", G_STRLOC, error->message);
        g_error_free (error);
    }

    if (!re)
        return NULL;

    gl = g_new0 (MooGlob, 1);
    gl->re = re;

    return gl;
}


static gboolean
_moo_glob_match (MooGlob    *glob,
                 const char *filename_utf8)
{
    g_return_val_if_fail (glob != NULL, FALSE);
    g_return_val_if_fail (filename_utf8 != NULL, FALSE);
    g_return_val_if_fail (g_utf8_validate (filename_utf8, -1, NULL), FALSE);

    return g_regex_match (glob->re, filename_utf8, 0, NULL);
}
#endif


static void
_moo_glob_free (MooGlob *glob)
{
    if (glob)
    {
#ifdef MOO_GLOB_REGEX
        g_regex_unref (glob->re);
#else
        g_free (glob->pattern);
#endif
        g_free (glob);
    }
}


gboolean
_moo_glob_match_simple (const char *pattern,
                        const char *filename)
{
    MooGlob *gl;
    gboolean result = FALSE;

    g_return_val_if_fail (pattern != NULL, FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    if ((gl = _moo_glob_new (pattern)))
        result = _moo_glob_match (gl, filename);

    if (result && 0)
        _moo_message ("'%s' matched '%s'", filename, pattern);

    _moo_glob_free (gl);
    return result;
}
