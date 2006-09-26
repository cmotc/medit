/*
 *   moooutputfilter.h
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

#ifndef __MOO_OUTPUT_FILTER_H__
#define __MOO_OUTPUT_FILTER_H__

#include <mooedit/moolineview.h>

G_BEGIN_DECLS


#define MOO_TYPE_FILE_LINE_DATA             (moo_file_line_data_get_type ())

#define MOO_TYPE_OUTPUT_FILTER              (moo_output_filter_get_type ())
#define MOO_OUTPUT_FILTER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_OUTPUT_FILTER, MooOutputFilter))
#define MOO_OUTPUT_FILTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_OUTPUT_FILTER, MooOutputFilterClass))
#define MOO_IS_OUTPUT_FILTER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_OUTPUT_FILTER))
#define MOO_IS_OUTPUT_FILTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_OUTPUT_FILTER))
#define MOO_OUTPUT_FILTER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_OUTPUT_FILTER, MooOutputFilterClass))

typedef struct _MooOutputFilter         MooOutputFilter;
typedef struct _MooOutputFilterPrivate  MooOutputFilterPrivate;
typedef struct _MooOutputFilterClass    MooOutputFilterClass;
typedef struct _MooFileLineData         MooFileLineData;

struct _MooFileLineData {
    char *file;
    int line;
    int character;
};

struct _MooOutputFilter {
    GObject base;
    MooOutputFilterPrivate *priv;
};

struct _MooOutputFilterClass {
    GObjectClass base_class;

    void     (*attach)      (MooOutputFilter *filter);
    void     (*detach)      (MooOutputFilter *filter);

    void     (*activate)    (MooOutputFilter *filter,
                             int              line);

    void     (*cmd_start)   (MooOutputFilter *filter);
    gboolean (*cmd_exit)    (MooOutputFilter *filter,
                             int              status);
    gboolean (*stdout_line) (MooOutputFilter *filter,
                             const char      *line);
    gboolean (*stderr_line) (MooOutputFilter *filter,
                             const char      *line);
};


GType            moo_output_filter_get_type         (void) G_GNUC_CONST;
GType            moo_file_line_data_get_type        (void) G_GNUC_CONST;

MooFileLineData *moo_file_line_data_new             (const char         *file,
                                                     int                 line,
                                                     int                 character);
MooFileLineData *moo_file_line_data_copy            (MooFileLineData    *data);
void             moo_file_line_data_free            (MooFileLineData    *data);

MooOutputFilter *moo_output_filter_new              (void);

void             moo_output_filter_set_view         (MooOutputFilter    *filter,
                                                     MooLineView        *view);
MooLineView     *moo_output_filter_get_view         (MooOutputFilter    *filter);

gboolean         moo_output_filter_stdout_line      (MooOutputFilter    *filter,
                                                     const char         *line);
gboolean         moo_output_filter_stderr_line      (MooOutputFilter    *filter,
                                                     const char         *line);
void             moo_output_filter_cmd_start        (MooOutputFilter    *filter,
                                                     const char         *working_dir);
gboolean         moo_output_filter_cmd_exit         (MooOutputFilter    *filter,
                                                     int                 status);

const char      *moo_output_filter_get_working_dir  (MooOutputFilter    *filter);
void             moo_output_filter_set_window       (MooOutputFilter    *filter,
                                                     gpointer            window);
gpointer         moo_output_filter_get_window       (MooOutputFilter    *filter);

void             moo_output_filter_open_file        (MooOutputFilter    *filter,
                                                     MooFileLineData    *data);


G_END_DECLS

#endif /* __MOO_OUTPUT_FILTER_H__ */