import re

errors = [
    r'(?P<file>[^:]+):(?P<line>\d+):(\d+:)?\s*error\s*:.*',
    r'(?P<file>[^:]+):(?P<line>\d+):.*'
]
warnings = [
    r'(?P<file>[^:]+):(?P<line>\d+):(\d+:)?\s*warning\s*:.*'
]

errors = [re.compile(e) for e in errors]
warnings = [re.compile(e) for e in warnings]


class ErrorInfo(object):
    def __init__(self, file, line, type='error'):
        object.__init__(self)
        self.file = file
        self.line = line - 1
        self.type = type

def parse_make_error(line):
    for p in errors:
        m = p.match(line)
        if not m:
            continue
        return ErrorInfo(m.group('file'), int(m.group('line')))
    for p in warnings:
        m = p.match(line)
        if not m:
            continue
        return ErrorInfo(m.group('file'), int(m.group('line')), 'warning')
    return None
