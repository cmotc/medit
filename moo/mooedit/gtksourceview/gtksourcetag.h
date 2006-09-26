/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcetag.h
 *
 *  Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SOURCE_TAG_H__
#define __GTK_SOURCE_TAG_H__

#include <gtk/gtktexttag.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_TAG             (gtk_source_tag_get_type ())
#define GTK_SOURCE_TAG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_TAG, GtkSourceTag))
#define GTK_SOURCE_TAG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_TAG, GtkSourceTagClass))
#define GTK_IS_SOURCE_TAG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_TAG))
#define GTK_IS_SOURCE_TAG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_TAG))
#define GTK_SOURCE_TAG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_TAG, GtkSourceTagClass))

typedef struct _GtkSourceTag       GtkSourceTag;
typedef struct _GtkSourceTagClass  GtkSourceTagClass;

struct _GtkSourceTag
{
	GtkTextTag base;
};

struct _GtkSourceTagClass
{
	GtkTextTagClass base_class;
};

GType gtk_source_tag_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif  /* __GTK_SOURCE_TAG_H__ */