import gtk
import gobject
import pango
import os

import moo
from moo.utils import _
import medit.runpython

MASTER_FILE = 'PYTHON_MASTER_FILE'

try:
    import pyconsole
    have_pyconsole = True
except ImportError:
    have_pyconsole = False

PLUGIN_ID = "Python"

class Plugin(moo.edit.Plugin):
    def do_init(self):
        editor = moo.edit.editor_instance()
        xml = editor.get_ui_xml()

        if xml is None:
            # standalone python module, not in medit
            return False

        self.ui_merge_id = xml.new_merge_id()

        if have_pyconsole:
            moo.utils.window_class_add_action(moo.edit.EditWindow, "PythonConsole",
                                              display_name=_("Python Console"),
                                              label=_("Python Console"),
                                              callback=self.show_console)
            xml.add_item(self.ui_merge_id, "ToolsMenu",
                         "PythonConsole", "PythonConsole", -1)

        """ Run file """
        moo.utils.window_class_add_action(moo.edit.EditWindow, "RunFile",
                                          display_name=_("Run File"),
                                          label=_("Run File"),
                                          stock_id=moo.utils.STOCK_EXECUTE,
                                          default_accel="<shift>F9",
                                          callback=self.run_file)
        moo.edit.window_set_action_filter("RunFile", moo.edit.ACTION_CHECK_SENSITIVE, "langs:python")
        xml.add_item(self.ui_merge_id, "ToolsMenu",
                     "RunFile", "RunFile", -1)

        return True

    def do_deinit(self):
        editor = moo.edit.editor_instance()
        xml = editor.get_ui_xml()
        xml.remove_ui(self.ui_merge_id)
        moo.utils.window_class_remove_action(moo.edit.EditWindow, "PythonConsole")
        moo.utils.window_class_remove_action(moo.edit.EditWindow, "RunFile")
        moo.utils.window_class_remove_action(moo.edit.EditWindow, "ReloadPythonPlugins")

    def show_console(self, window):
        window = gtk.Window()
        swin = gtk.ScrolledWindow()
        swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_ALWAYS)
        window.add(swin)

        dic = {}
        dic['editor'] = moo.edit.editor_instance()
        dic['window'] = moo.edit.editor_instance().get_active_window()
        dic['doc'] = moo.edit.editor_instance().get_active_doc()
        console_type = pyconsole.ConsoleType(moo.edit.TextView)
        console = console_type(use_rlcompleter=False, start_script=
                               "import moo\nimport gtk\n", locals=dic)
        console.set_property("highlight-current-line", False)
        editor = moo.edit.editor_instance()
        console.set_lang_by_id("python-console")
        console.modify_font(pango.FontDescription("Monospace"))

        swin.add(console)
        window.set_default_size(400,300)
        window.set_title("pythony")
        window.show_all()

    def run_file(self, window):
        filename = os.environ.get(MASTER_FILE)

        if filename is None:
            doc = window.get_active_doc()

            if not doc:
                return
            if not doc.get_filename() or doc.get_status() & moo.edit.EDIT_MODIFIED:
                if not doc.save():
                    return

            filename = doc.get_filename()

        r = medit.runpython.Runner(window)
        r.run(filename)

    def do_detach_win(self, window):
        window.remove_pane(PLUGIN_ID)


gobject.type_register(Plugin)
__plugin__ = Plugin
