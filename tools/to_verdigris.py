import os, sys
from itertools import tee
import clang.cindex
import copy

from glob import glob
result = [y for x in os.walk("/home/jcelerier/score/") for y in glob(os.path.join(x[0], '*.hpp'))]

clang.cindex.Config.set_library_file('/usr/lib/libclang.so')
index = clang.cindex.Index.create()

# print(result[0])
with open('/tmp/foo.hpp', 'r') as content_file:
    source_file = content_file.read()

translation_unit = index.parse(
    '/tmp/foo.hpp',
    ['-x', 'c++', '-std=c++17', '-D__CODE_GENERATOR__', '-I/usr/include/qt', '-I/usr/include/qt/QtCore'],
    options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

def node_children(node):
    return (c for c in node.get_children() if c.location.file.name == '/tmp/foo.hpp')

def print_node(node):
    text = node.spelling or node.displayname
    kind = str(node.kind)[str(node.kind).index('.')+1:]
    return '{} {}'.format(kind, text)

def replace_at(oldStr, start, end, replacement):
    return oldStr[0:start] + replacement + oldStr[end:]

k = 0

# Replacements are inserted in a dict, and are then executed in reverse order
# to preserve positions
replacements = { }
def add_replacement(c, s):
    replacements[c.extent.start.offset] = lambda src : replace_at(src, c.extent.start.offset, c.extent.end.offset, s)

def replace_qobject(c):
    global source_file
    if(c.kind.is_declaration() and c.spelling == 'staticMetaObject'):
        add_replacement(c, "W_OBJECT(" + c.semantic_parent.spelling + ")")

def replace_qproperty(c):
    global source_file
    if(c.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION):
      if(c.spelling == 'Q_PROPERTY'):
        prop = source_file[c.extent.start.offset+11:c.extent.end.offset-1]
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

        p_line = "W_PROPERTY(" + p_type + ", " + p_name;
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
        p_line = p_line + ")";

        add_replacement(c, p_line)


signal_positions = []
slot_positions = []

    #print(k * ' ' + print_node(c))
def replace_qsignal(c, it):
    global source_file
    if(c.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION):
        if c.spelling == 'Q_SIGNALS' or c.spelling == 'signals':
            signal_positions.append(c.location.line)

    elif c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL and c.location.line in signal_positions:
        while 1:
            try:
                next_c = next(it)
            except StopIteration:
                break

            if next_c.kind == clang.cindex.CursorKind.CXX_METHOD:
                signal = source_file[next_c.extent.start.offset:next_c.extent.end.offset]
                sig_name = next_c.spelling;

                s_line = "W_SIGNAL(" + sig_name;
                for sig_p in next_c.get_children():
                    s_line = s_line + ", " + sig_p.spelling;
                s_line = s_line + ")"
                print(signal, s_line)

                add_replacement(next_c, signal + " " + s_line)
                print(signal + " " + s_line)
            if next_c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL:
                break


def replace_qslot(c, it):
    global source_file
    if(c.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION):
        if(c.spelling == 'Q_SLOTS' or c.spelling == 'slots'):
            slot_positions.append(c.location.line)
    elif c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL and c.location.line in slot_positions:
        while 1:
            try:
                next_c = next(it)
            except StopIteration:
                break

            if next_c.kind == clang.cindex.CursorKind.CXX_METHOD:
                slot = source_file[next_c.extent.start.offset:next_c.extent.end.offset]
                slt_name = next_c.spelling;

                s_line = "W_SLOT(" + slt_name + ")";
                print(slot, s_line)

                add_replacement(next_c, slot + " " + s_line)
            if next_c.kind == clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL:
                break

def replace_qinvokable(c):
    global source_file

def recurse(node):
    global k
    k = k + 1
    cld = node.get_children()
    while 1:
        try:
            c = next(cld)
        except StopIteration:
            break

        if(c.location.file is not None):
            if(c.location.file.name == '/tmp/foo.hpp'):
                replace_qobject(c)
                replace_qproperty(c)

                replace_qsignal(c, copy.copy(cld))


                replace_qslot(c, copy.copy(cld))

                replace_qinvokable(c)
        recurse(c)
    k = k - 1

recurse(translation_unit.cursor)

for key in sorted(replacements.keys(), reverse=True):
    source_file = replacements[key](source_file)

print(source_file)
