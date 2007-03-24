if __name__ == '__main__':
    import sys
    import os.path
    dir = os.path.dirname(__file__)
    sys.path.insert(0, os.path.join(dir, '../../..'))
    sys.path.insert(0, os.path.join(dir, '../..'))
    sys.path.insert(0, os.path.join(dir, '..'))

import gobject
import os.path
import moo
from moo.utils import _

import mprj.optdialog
from mprj.config.view import *
from pyproj.config import *

class RunOptionsPage(mprj.optdialog.ConfigPage):
    __label__ = _('Run options')
    __types__ = {'vars' : DictView,
                 'exe' : Entry,
                 'args' : Entry}

    def do_init(self):
        self.xml.w_vars.set_dict(self.config.run.vars)
        self.xml.w_exe.set_setting(self.config.run['exe'])
        self.xml.w_args.set_setting(self.config.run['args'])

class Dialog(moo.utils.PrefsDialog):
    def __init__(self, project, title=_('Project Options')):
        moo.utils.PrefsDialog.__init__(self, title)
        self.project = project
        self.config_copy = project.config.copy()
#         self.append_page(mprj.simple.ConfigPage(self.config_copy))
        glade_file = os.path.join(os.path.dirname(__file__), 'options.glade')
        self.append_page(RunOptionsPage('page_run', self.config_copy, glade_file))

    def do_apply(self):
        moo.utils.PrefsDialog.do_apply(self)
        self.project.config.copy_from(self.config_copy)
        self.project.save_config()
        print '============================='
        print self.project.config.dump_xml()
        print '============================='

gobject.type_register(RunOptionsPage)
gobject.type_register(Dialog)

if __name__ == '__main__':
    import gtk
    from pyproj.config import PyConfig, _sample_file
    from mprj.config import File
    from python import PyProject

    editor = moo.edit.create_editor_instance()
    file = File(_sample_file, '/tmp/fake/file')
    config = PyConfig(file)
    project = PyProject(None, config, file)
    dialog = Dialog(project)
    dialog.connect('destroy', gtk.main_quit)
    dialog.run()
    gtk.main()
