#pragma once

#include <gtk/gtk.h>
#include <mooutils/mooarray.h>
#include <mooutils/moolist.h>
#include <mooutils/mootype-macros.h>
#include <mooedit/mooedit-enums.h>

G_BEGIN_DECLS

#define MOO_DECLARE_C_CLASS(Object, object)             \
    typedef struct Object Object;                       \
    GType object##_get_type(void) G_GNUC_CONST;

MOO_DECLARE_C_CLASS(MooEdit, moo_edit);
MOO_DECLARE_C_CLASS(MooEditor, moo_editor);
MOO_DECLARE_C_CLASS(MooEditView, moo_edit_view);
MOO_DECLARE_C_CLASS(MooEditWindow, moo_edit_window);
MOO_DECLARE_C_CLASS(MooEditor, moo_editor);
MOO_DECLARE_C_CLASS(MooEditTab, moo_edit_tab);
MOO_DECLARE_C_CLASS(MooTextView, moo_text_view);
MOO_DECLARE_C_CLASS(MooOpenInfo, moo_open_info);
MOO_DECLARE_C_CLASS(MooSaveInfo, moo_save_info);
MOO_DECLARE_C_CLASS(MooReloadInfo, moo_reload_info);

MOO_DECLARE_OBJECT_ARRAY (MooEdit, moo_edit)
MOO_DECLARE_OBJECT_ARRAY (MooEditView, moo_edit_view)
MOO_DECLARE_OBJECT_ARRAY (MooEditTab, moo_edit_tab)
MOO_DECLARE_OBJECT_ARRAY (MooEditWindow, moo_edit_window)

MOO_DECLARE_PTR_ARRAY(MooOpenInfo, moo_open_info)

#define MOO_TYPE_LINE_END (moo_type_line_end ())
GType   moo_type_line_end   (void) G_GNUC_CONST;

#define MOO_EDIT_RELOAD_ERROR (moo_edit_reload_error_quark ())
#define MOO_EDIT_SAVE_ERROR (moo_edit_save_error_quark ())

MOO_DECLARE_QUARK (moo-edit-reload-error, moo_edit_reload_error_quark)
MOO_DECLARE_QUARK (moo-edit-save-error, moo_edit_save_error_quark)

G_END_DECLS

#ifdef __cplusplus

#include <moocpp/moocpp.h>

MOO_DEFINE_GOBJ_TYPE(MooTextView, GtkTextView, moo_text_view_get_type())
MOO_DEFINE_GOBJ_TYPE(MooEditTab, GtkWidget, moo_edit_tab_get_type());
MOO_DEFINE_GOBJ_TYPE(MooEditWindow, GtkWindow, moo_edit_window_get_type());
MOO_DEFINE_GOBJ_TYPE(MooEditView, MooTextView, moo_edit_view_get_type());

MOO_DECLARE_CUSTOM_GOBJ_TYPE(MooEdit);
MOO_DECLARE_CUSTOM_GOBJ_TYPE(MooEditView);

MOO_GOBJ_TYPEDEFS(TextView, MooTextView);
MOO_GOBJ_TYPEDEFS(Edit, MooEdit);
MOO_GOBJ_TYPEDEFS(EditTab, MooEditTab);
MOO_GOBJ_TYPEDEFS(EditView, MooEditView);
MOO_GOBJ_TYPEDEFS(EditWindow, MooEditWindow);

void g_object_unref(MooOpenInfo*) = delete;
void g_object_unref(MooReloadInfo*) = delete;
void g_object_unref(MooSaveInfo*) = delete;

void g_free(MooOpenInfo*) = delete;
void g_free(MooReloadInfo*) = delete;
void g_free(MooSaveInfo*) = delete;

MOO_DEFINE_NON_GOBJ_TYPE(MooOpenInfo);
MOO_DEFINE_NON_GOBJ_TYPE(MooReloadInfo);
MOO_DEFINE_NON_GOBJ_TYPE(MooSaveInfo);

#endif // __cplusplus
