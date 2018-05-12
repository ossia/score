import os, sys
from itertools import tee
import clang.cindex
import copy
from glob import glob

def node_children(node):
    return (c for c in node.get_children() if c.location.file.name == '/tmp/foo.hpp')

def print_node(node):
    text = node.spelling or node.displayname
    kind = str(node.kind)[str(node.kind).index('.')+1:]
    return '{} {}'.format(kind, text)

def replace_at(oldStr, start, end, replacement):
    return oldStr[0:start] + replacement + oldStr[end:]

class verdigris_converter:
    # Replacements are inserted in a dict, and are then executed in reverse order
    # to preserve positions
    file_path = ""
    source_file = ""
    replacements = { }
    signal_positions = []
    slot_positions = []
    props_positions = {}

    def add_replacement_at(self, start, end, s):
        self.replacements.setdefault(start, [])
        self.replacements[start].append(lambda src : replace_at(src, start, end, s))

    def add_replacement(self, c, s):
        self.add_replacement_at(c.extent.start.offset, c.extent.end.offset, s)
        # self.replacements[c.extent.start.offset] = lambda src : replace_at(src, c.extent.start.offset, c.extent.end.offset, s)

    def replace_qobject(self, c):
        if(c.kind.is_declaration() and c.spelling == 'staticMetaObject'):
            # print(c.kind, c.spelling)
            self.add_replacement(c, "W_OBJECT(" + c.semantic_parent.spelling + ")")

    def replace_qproperty(self, c):
        if(c.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION):
          if(c.spelling == 'Q_PROPERTY'):
            prop = self.source_file[c.extent.start.offset+11:c.extent.end.offset-1]
            props = prop.split()
            p_type = props[0]
            p_name = props[1]
            p_read = ""
            p_write = ""
            p_notify = ""
            p_reset = ""
            p_member = ""
            p_final = False
            i = 2
            while i < len(props):
                if props[i] == "READ":
                    p_read = props[i+1]
                    i = i + 2
                elif props[i] == "WRITE":
                    p_write = props[i+1]
                    i = i + 2
                elif props[i] == "NOTIFY":
                    p_notify = props[i+1]
                    i = i + 2
                elif props[i] == "RESET":
                    p_reset = props[i+1]
                    i = i + 2
                elif props[i] == "MEMBER":
                    p_member = props[i+1]
                    i = i + 2
                elif props[i] == "FINAL":
                    p_final = True
                    i = i + 1
                else:
                    i = i + 1

            p_line = "\nW_PROPERTY(" + p_type + ", " + p_name;
            if p_read != "":
                p_line = p_line + " READ " + p_read;
            if p_write != "":
                p_line = p_line + " WRITE " + p_write;
            if p_notify != "":
                p_line = p_line + " NOTIFY " + p_notify;
            if p_reset != "":
                p_line = p_line + " RESET " + p_reset;
            if p_member != "":
                p_line = p_line + " MEMBER " + p_member;
            if p_final:
                p_line = p_line + "; W_Final"
            p_line = p_line + ")\n";

            self.props_positions[c.location.line] = [ c, p_line ]

        elif c.kind == clang.cindex.CursorKind.CLASS_DECL:
            for prop_line in self.props_positions.keys():
                if(prop_line > c.extent.start.line and prop_line < c.extent.end.line):
                    prop_cursor, newline = self.props_positions[prop_line]
                    self.add_replacement(prop_cursor, "")
                    print("REPLACING at : ", c.extent.end.offset - 1, newline)
                    self.add_replacement_at(c.extent.end.offset - 1, c.extent.end.offset - 1, newline)


    def replace_qsignal(self, c, it):
        if(c.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION):
            if c.spelling == 'Q_SIGNALS' or c.spelling == 'signals':
                self.signal_positions.append(c.location.line)

        elif c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL and c.location.line in self.signal_positions:
            while 1:
                try:
                    next_c = next(it)
                except StopIteration:
                    break

                if next_c.kind == clang.cindex.CursorKind.CXX_METHOD:
                    signal = self.source_file[next_c.extent.start.offset:next_c.extent.end.offset]
                    sig_name = next_c.spelling;

                    s_line = "W_SIGNAL(" + sig_name;
                    count = 0
                    for sig_p in next_c.get_children():
                        count = count + 1
                        if(len(sig_p.spelling) > 0):
                          s_line = s_line + ", " + sig_p.spelling;
                        else:
                          print("A signal argument does not have a name: ", self.file_path, ":", next_c.location.line)
                          signal = replace_at(signal, sig_p.extent.end.offset - next_c.extent.start.offset, sig_p.extent.end.offset - next_c.extent.start.offset, " arg_" + str(count))
                          s_line = s_line + ", " + "arg_" + str(count);
                    s_line = s_line + ")"
                    print(signal, s_line)

                    self.add_replacement(next_c, signal + " " + s_line)
                    print(signal + " " + s_line)
                if next_c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL:
                    break


    def replace_qslot(self, c, it):
        if(c.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION):
            if(c.spelling == 'Q_SLOTS' or c.spelling == 'slots'):
                self.slot_positions.append(c.location.line)
        elif c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL and c.location.line in self.slot_positions:
            while 1:
                try:
                    next_c = next(it)
                except StopIteration:
                    break

                if next_c.kind == clang.cindex.CursorKind.CXX_METHOD:
                    slot = self.source_file[next_c.extent.start.offset:next_c.extent.end.offset]
                    slt_name = next_c.spelling;

                    s_line = "W_SLOT(" + slt_name + ")";
                    # print(slot, s_line)

                    self.add_replacement(next_c, slot + "; " + s_line)
                if next_c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL:
                    break

    def replace_qinvokable(self, c):
        return 0

    def recurse(self, node):
        cld = node.get_children()
        while 1:
            try:
                c = next(cld)
            except StopIteration:
                break

            if(c.location.file is not None):
                if(c.location.file.name == self.file_path):

                    # print(c.location.file.name, self.file_path)
                    self.replace_qobject(c)
                    self.replace_qproperty(c)
                    self.replace_qsignal(c, copy.copy(cld))
                    self.replace_qslot(c, copy.copy(cld))
                    self.replace_qinvokable(c)

            self.recurse(c)

    def process(self, tu, source_file):
        # print(source_file)

        self.source_file = source_file
        self.file_path = tu.spelling
        self.replacements = { }
        self.signal_positions = []
        self.slot_positions = []

        self.recurse(tu.cursor)

        for key in sorted(self.replacements.keys(), reverse=True):
            for repl in self.replacements[key]:
                self.source_file = repl(self.source_file)


if len(sys.argv) < 2:
    print("Usage: python to_verdigris.py /path/to/convert")
    index = clang.cindex.Index.create()
    with open('/tmp/foo.hpp', 'r') as content_file:
        source_file = content_file.read()

    tu = index.parse(
        '/tmp/foo.hpp',
        ['-x', 'c++', '-std=c++17', '-D__CODE_GENERATOR__', '-I/usr/include/qt', '-I/usr/include/qt/QtCore'],
        options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

    conv = verdigris_converter();
    conv.process(tu, source_file)
    print(conv.source_file)
    exit(1)
else:
    index = clang.cindex.Index.create()
    compdb = clang.cindex.CompilationDatabase.fromDirectory(sys.argv[1])

    for cmd in compdb.getAllCompileCommands():
        arg = cmd.arguments
        print(cmd.filename)
        next(arg)
        args = ['-x', 'c++']

        for c in arg:
            args.append(c)
        args = args[:len(args) - 4]

        if cmd.filename[-3:] != 'hpp':
            continue
        with open(cmd.filename, 'r') as content_file:
            source_file = content_file.read()

        if("Q_OBJECT" not in source_file):
            continue

        # print(args)

        tu = index.parse(cmd.filename, args, options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

        conv = verdigris_converter();
        conv.process(tu, source_file)

        with open(cmd.filename, 'w') as f:
            f.write(conv.source_file)
