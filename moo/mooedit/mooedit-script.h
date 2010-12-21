#ifndef MOO_EDIT_SCRIPT_H
#define MOO_EDIT_SCRIPT_H

#include <mooedit/mooedit.h>

G_BEGIN_DECLS

gboolean     moo_edit_can_undo                  (MooEdit        *doc);
gboolean     moo_edit_can_redo                  (MooEdit        *doc);
gboolean     moo_edit_undo                      (MooEdit        *doc);
gboolean     moo_edit_redo                      (MooEdit        *doc);
void         moo_edit_begin_non_undoable_action (MooEdit        *doc);
void         moo_edit_end_non_undoable_action   (MooEdit        *doc);

GtkTextIter *moo_edit_get_start_pos             (MooEdit        *doc);
GtkTextIter *moo_edit_get_end_pos               (MooEdit        *doc);
GtkTextIter *moo_edit_get_cursor_pos            (MooEdit        *doc);
void         moo_edit_set_cursor_pos            (MooEdit        *doc,
                                                 GtkTextIter    *pos);
GtkTextIter *moo_edit_get_selection_start_pos   (MooEdit        *doc);
GtkTextIter *moo_edit_get_selection_end_pos     (MooEdit        *doc);
void         moo_edit_set_selection             (MooEdit        *doc,
                                                 GtkTextIter    *start,
                                                 GtkTextIter    *end);

int          moo_edit_get_char_count            (MooEdit        *doc);
int          moo_edit_get_line_count            (MooEdit        *doc);

char       **moo_edit_get_selected_lines        (MooEdit        *doc);
void         moo_edit_replace_selected_lines    (MooEdit        *doc,
                                                 char          **replacement);

char        *moo_edit_get_text                  (MooEdit        *doc);

char        *moo_edit_get_selected_text         (MooEdit        *doc);
void         moo_edit_replace_selected_text     (MooEdit        *doc,
                                                 const char     *replacement);

gboolean     moo_edit_has_selection             (MooEdit        *doc);

void         moo_edit_insert_text               (MooEdit        *doc,
                                                 const char     *text);

G_END_DECLS

#endif /* MOO_EDIT_SCRIPT_H */
