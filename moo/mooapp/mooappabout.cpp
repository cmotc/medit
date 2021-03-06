/*
 *   mooapp/mooappabout.c
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#include "mooappabout.h"
#include "mooapp.h"
#include "mooapp-credits.h"
#include "mooapp-info.h"
#include "moohtml.h"
#include "moolinklabel.h"
#include "mooutils/moostock.h"
#include "mooutils/mooutils-misc.h"
#include "mooutils/mooglade.h"
#include "mooutils/mooi18n.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moohelp.h"
#ifdef MOO_ENABLE_HELP
#include "moo-help-sections.h"
#endif
#include "mooapp/mooappabout-dialog-gxml.h"
#include "mooapp/mooappabout-license-gxml.h"
#include "mooapp/mooappabout-credits-gxml.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <libxml/xmlversion.h>

using namespace moo;

static GtkDialog* about_dialog;
static GtkDialog* credits_dialog;

#undef MOO_USE_HTML
#define MOO_USE_HTML 1

#ifndef MOO_USE_HTML
#define MooHtml GtkTextView
#undef MOO_TYPE_HTML
#define MOO_TYPE_HTML GTK_TYPE_TEXT_VIEW
#endif

static void
set_translator_credits (CreditsDialogXml *gxml)
{
    const char *credits, *credits_markup;

    /* Translators: this goes into About box, under Translated by tab,
       do not ignore it, markup isn't always used. It should be something
       like "Some Guy <someguy@domain.net>", with lines separated by \n */
    credits = _("translator-credits");
    /* Translators: this goes into About box, under Translated by tab,
       this must be valid html markup, e.g.
       "Some Guy <a href=\"mailto:someguy@domain.net\">&lt;someguy@domain.net&gt;</a>"
       Lines must be separated by <br>, like "First guy<br>Second Guy" */
    credits_markup = _("translator-credits-markup");

    if (!strcmp (credits, "translator-credits"))
    {
        gtk_widget_hide (GTK_WIDGET (gxml->page_translator_credits));
        return;
    }

#ifdef MOO_USE_HTML
    if (strcmp (credits_markup, "translator-credits-markup") != 0)
        _moo_html_load_memory (GTK_TEXT_VIEW (gxml->translated_by),
                               credits_markup, -1, NULL, NULL);
    else
#endif
    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gxml->translated_by));
        gtk_text_buffer_insert_at_cursor (buffer, credits, -1);
    }
}

static void
show_credits (void)
{
    CreditsDialogXml *gxml;
    GtkTextBuffer *buffer;

    if (credits_dialog)
    {
        if (about_dialog)
            moo_window_set_parent (GTK_WIDGET (credits_dialog), GTK_WIDGET (about_dialog));
        gtk_window_present (GTK_WINDOW (credits_dialog));
        return;
    }

#ifdef MOO_USE_HTML
    moo_glade_xml_register_type (MOO_TYPE_HTML);
    gxml = credits_dialog_xml_new ();
#else
    gxml = credits_dialog_xml_new_empty ();
    moo_glade_xml_map_class (gxml->xml, "MooHtml", MOO_TYPE_HTML);
    credits_dialog_xml_build (gxml);
#endif

    credits_dialog = gxml->CreditsDialog;
    g_return_if_fail (credits_dialog != NULL);
    g_object_add_weak_pointer (G_OBJECT (credits_dialog), (void**)&credits_dialog);
    g_signal_connect (credits_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

#ifdef MOO_USE_HTML
    _moo_html_load_memory (GTK_TEXT_VIEW (gxml->written_by),
                           "Yevgen Muntyan <a href=\"mailto://" MOO_EMAIL
                                    "\">&lt;" MOO_EMAIL "&gt;</a>",
                           -1, NULL, NULL);
#else
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gxml->written_by));
    gtk_text_buffer_insert_at_cursor (buffer,
                                      "Yevgen Muntyan <" MOO_EMAIL ">", -1);
#endif

    set_translator_credits (gxml);

    buffer = gtk_text_view_get_buffer (gxml->thanks);
    gtk_text_buffer_insert_at_cursor (buffer, MOO_APP_CREDITS, -1);

    if (about_dialog)
        moo_window_set_parent (GTK_WIDGET (credits_dialog), GTK_WIDGET (about_dialog));
    gtk_window_present (GTK_WINDOW (credits_dialog));
}


static void
license_clicked (void)
{
#ifdef MOO_ENABLE_HELP
    moo_help_open_id (HELP_SECTION_APP_LICENSE, NULL);
#endif
}


static gboolean
about_dialog_key_press (GtkWidget   *dialog,
                        GdkEventKey *event)
{
    if (event->keyval == GDK_Escape)
    {
        gtk_widget_hide (dialog);
        return TRUE;
    }

    if (event->keyval == GDK_s && (event->state & GDK_CONTROL_MASK))
    {
        moo::gstr info = App::get_system_info ();
        if (!info.empty())
            gtk_clipboard_set_text (gtk_widget_get_clipboard (dialog, GDK_SELECTION_CLIPBOARD), info, -1);
    }

    return FALSE;
}


static GtkDialog*
create_about_dialog (void)
{
    AboutDialogXml *gxml;
    GtkDialog *dialog;
    char *markup;
    GtkStockItem dummy;

    moo_glade_xml_register_type (MOO_TYPE_LINK_LABEL);
    gxml = about_dialog_xml_new ();

    dialog = gxml->AboutDialog;
    g_signal_connect (dialog, "key-press-event", G_CALLBACK (about_dialog_key_press), NULL);

    g_object_add_weak_pointer (G_OBJECT (dialog), (void**)&about_dialog);
    g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

    markup = g_markup_printf_escaped ("<span size=\"xx-large\"><b>%s %s</b></span>",
                                      MOO_APP_FULL_NAME, MOO_DISPLAY_VERSION);
    gtk_label_set_markup (gxml->name, markup);
    g_free (markup);

    gtk_label_set_text (gxml->description, MOO_APP_DESCRIPTION);

    markup = g_markup_printf_escaped ("<small>\302\251 %s</small>", MOO_COPYRIGHT);
    gtk_label_set_markup (gxml->copyright, markup);
    g_free (markup);

    _moo_link_label_set_url (gxml->url, MOO_APP_WEBSITE);
    _moo_link_label_set_text (gxml->url, MOO_APP_WEBSITE_LABEL);

    if (gtk_stock_lookup (MOO_APP_LOGO, &dummy))
        gtk_image_set_from_stock (gxml->logo, MOO_APP_LOGO,
                                  GTK_ICON_SIZE_DIALOG);
    else
        gtk_image_set_from_icon_name (gxml->logo, MOO_APP_LOGO,
                                      GTK_ICON_SIZE_DIALOG);

    g_signal_connect (gxml->credits_button, "clicked", G_CALLBACK (show_credits), NULL);
    g_signal_connect (gxml->license_button, "clicked", G_CALLBACK (license_clicked), NULL);
    g_signal_connect_swapped (gxml->close_button, "clicked", G_CALLBACK (gtk_widget_hide), dialog);

    return dialog;
}


void App::about_dialog (GtkWidget *parent)
{
    if (!::about_dialog)
        ::about_dialog = create_about_dialog ();

    if (parent)
        parent = gtk_widget_get_toplevel (parent);

    if (parent && GTK_IS_WINDOW (parent))
        moo_window_set_parent (GTK_WIDGET (::about_dialog), parent);

    gtk_window_present (GTK_WINDOW (::about_dialog));
}


gstr App::get_system_info ()
{
    char **dirs, **p;

    strbuilder text;

    text.append_printf ("%s-%s\n", MOO_APP_FULL_NAME, MOO_DISPLAY_VERSION);
    text.append_printf ("OS: %s\n", get_system_name ());
    text.append_printf ("GTK version: %u.%u.%u\n",
                        gtk_major_version,
                        gtk_minor_version,
                        gtk_micro_version);
    text.append_printf ("Built with GTK %d.%d.%d\n",
                        GTK_MAJOR_VERSION,
                        GTK_MINOR_VERSION,
                        GTK_MICRO_VERSION);

    text.append_printf ("libxml2: %s\n", LIBXML_DOTTED_VERSION);

    text.append ("Data dirs: ");
    dirs = moo_get_data_dirs ();
    for (p = dirs; p && *p; ++p)
        text.append_printf ("%s'%s'", p == dirs ? "" : ", ", *p);
    text.append ("\n");
    g_strfreev (dirs);

    text.append ("Lib dirs: ");
    dirs = moo_get_lib_dirs ();
    for (p = dirs; p && *p; ++p)
        text.append_printf ("%s'%s'", p == dirs ? "" : ", ", *p);
    text.append ("\n");
    g_strfreev (dirs);

#ifdef MOO_BROKEN_GTK_THEME
    text.append ("Broken gtk theme: yes\n");
#endif

    return text.release();
}
