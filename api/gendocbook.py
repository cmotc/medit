#! /usr/bin/env python

import sys
import optparse

from mpi.module import Module
from mpi.docbookwriter import Writer

op = optparse.OptionParser()
op.add_option("--python", action="store_true")
op.add_option("--lua", action="store_true")
op.add_option("--template", action="store")
op.add_option("-i", "--import", action="append", dest="import_modules")
(opts, args) = op.parse_args()

assert len(args) == 1
assert bool(opts.python) + bool(opts.lua) == 1
if opts.python:
    mode = 'python'
elif opts.lua:
    mode = 'lua'

import_modules = []
if opts.import_modules:
    for filename in opts.import_modules:
        import_modules.append(Module.from_xml(filename))

mod = Module.from_xml(args[0])
for im in import_modules:
    mod.import_module(im)
Writer(mode, opts.template, sys.stdout).write(mod)
