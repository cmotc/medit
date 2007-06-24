#!/bin/sh
# -%- strip: true; indent-width: 2 -%-

if test -z "$1"; then
	echo "usage: $0 <filename>"
	exit 1
fi

base=`basename "$1"`
srcdir=`dirname "$1"`
custom_cmd=

echo "/* This file is autogenerated from $base */"
case "$base" in
#   gtksourcelanguage.c)
#     echo '#include "mooedit/moolang.h"'
#     custom_cmd="-e s/g_object_new.*GTK_TYPE_SOURCE_LANGUAGE/g_object_new(MOO_TYPE_LANG/g"
#     ;;
  gtksourcelanguage-parser-1.c)
    echo '#include <glib/gmappedfile.h>'
    ;;
  gtksourcestyle.c)
    echo '#include "mooutils/moocompat.h"'
    ;;
  gtksourcestylescheme.c)
    echo '#include "mooutils/moocompat.h"'
    ;;
esac
echo "#line 1 \"$srcdir/$base\""

sed -e 's/#include \"gtksourcecontextengine.h\"/#include \"gtksourcecontextengine-mangled.h\"/'					\
    -e 's/#include \"gtksourceengine.h\"/#include \"gtksourceengine-mangled.h\"/'						\
    -e 's/#include \"gtksourceiter.h\"/#include \"gtksourceiter-mangled.h\"/'							\
    -e 's/#include \"gtksourcelanguage.h\"/#include \"gtksourcelanguage-mangled.h\"/'						\
    -e 's/#include \"gtksourcelanguage-private.h\"/#include \"gtksourcelanguage-private-mangled.h\"/'				\
    -e 's/#include \"gtksourcelanguagemanager.h\"/#include \"gtksourcelanguagemanager-mangled.h\"/'				\
    -e 's/#include \"gtksourcestyle.h\"/#include \"gtksourcestyle-mangled.h\"/'							\
    -e 's/#include \"gtksourcestylemanager.h\"/#include \"gtksourcestylemanager-mangled.h\"/'					\
    -e 's/#include \"gtksourcestylescheme.h\"/#include \"gtksourcestylescheme-mangled.h\"/'					\
    -e 's/#include \"gtksourcestyle-private.h\"/#include \"gtksourcestyle-private-mangled.h\"/'					\
    -e 's/#include \"gtktextregion.h\"/#include \"gtktextregion-mangled.h\"/'							\
    -e 's/#include \"gtksourceview-utils.h\"/#include \"gtksourceview-utils-mangled.h\"/'					\
																\
    -e 's@#include <gtksourceview/gtksourcecontextengine.h>@#include <gtksourceview/gtksourcecontextengine-mangled.h>@'		\
    -e 's@#include <gtksourceview/gtksourceengine.h>@#include <gtksourceview/gtksourceengine-mangled.h>@'			\
    -e 's@#include <gtksourceview/gtksourceiter.h>@#include <gtksourceview/gtksourceiter-mangled.h>@'				\
    -e 's@#include <gtksourceview/gtksourcelanguage.h>@#include <gtksourceview/gtksourcelanguage-mangled.h>@'			\
    -e 's@#include <gtksourceview/gtksourcelanguage-private.h>@#include <gtksourceview/gtksourcelanguage-private-mangled.h>@'	\
    -e 's@#include <gtksourceview/gtksourcelanguagemanager.h>@#include <gtksourceview/gtksourcelanguagemanager-mangled.h>@'	\
    -e 's@#include <gtksourceview/gtksourcestyle.h>@#include <gtksourceview/gtksourcestyle-mangled.h>@'				\
    -e 's@#include <gtksourceview/gtksourcestylemanager.h>@#include <gtksourceview/gtksourcestylemanager-mangled.h>@'		\
    -e 's@#include <gtksourceview/gtksourcestylescheme.h>@#include <gtksourceview/gtksourcestylescheme-mangled.h>@'		\
    -e 's@#include <gtksourceview/gtktextregion.h>@#include <gtksourceview/gtktextregion-mangled.h>@'				\
																\
    -e 's@#include \"gtksourcebuffer.h\"@@'											\
    -e 's@#include \"gtksourceview.h\"@@'											\
																\
    -e 's/GtkSource/MooGtkSource/g'												\
    -e 's/GtkTextRegion/MooGtkTextRegion/g'											\
    -e 's/_gtk_source/gtk_source/g'												\
    -e 's/gtk_source/_moo_gtk_source/g'												\
    -e 's/_gtk_text_region/gtk_text_region/g'											\
    -e 's/gtk_text_region/_moo_gtk_text_region/g'										\
																\
    $custom_cmd "$1" || exit $?
