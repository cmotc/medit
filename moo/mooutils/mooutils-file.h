#pragma once

#include <gio/gio.h>
#include <mooutils/mooarray.h>

G_BEGIN_DECLS

MOO_DECLARE_OBJECT_ARRAY_FULL (MooFileArray, moo_file_array, GFile)

G_INLINE_FUNC void
moo_file_free (GFile *file)
{
    if (file)
        g_object_unref (file);
}

gboolean     moo_file_fnmatch           (GFile*       file,
                                         const char*  glob);

G_END_DECLS

#ifdef __cplusplus

#include <moocpp/moocpp.h>

moo::gstr   moo_file_get_display_name   (moo::g::File file);

#endif // __cplusplus
