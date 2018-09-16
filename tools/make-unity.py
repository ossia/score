import json
from pprint import pprint
from collections import OrderedDict

with open('compile_commands.json') as f:
    data = json.load(f)

includes = set("")
defines = set("")
files = []

for cmd in data:
    files.append(cmd["file"])
    
    arr = cmd["command"].split(' ')    
    for i in range(0, len(arr) - 1):
        if arr[i] == '-isystem':
            includes.add(arr[i+1])
        elif arr[i] == '-I':
            includes.add(arr[i+1])
        elif arr[i][0:2] == '-I':
            includes.add(arr[i][2:])

        elif arr[i][0:2] == '-D':
            defines.add(arr[i][2:])


files = [x for x in files if '_unity' not in x]
files = [x for x in files if 'Tests' not in x]
files = [x for x in files if 'tests' not in x]
files = [x for x in files if 'Examples' not in x]
files = [x for x in files if 'examples' not in x]
qrcs = [x for x in files if 'qrc_' in x]
files = [x for x in files if 'qrc_' not in x]
files = [x for x in files if 'vstpuppet' not in x]
files = OrderedDict.fromkeys(files)
build = "#!/bin/bash -eux\n"

# Main source file
src = ""
for cpp in files:
    src = src + '#include "{}"\n'.format(cpp)

with open('unity.cpp', 'w') as f:
    f.write(src)

# Compiler command line
str = "/usr/bin/clang++ -O3 -march=native -fvisibility=hidden -fvisibility-inlines-hidden -fPIC -std=c++17"
for include in includes:
    str += ' -I' + include
for define in defines:
    str += ' -D' + define

qrc_compiled = []
for qrc in qrcs: 
    filename = qrc.split('/')[-1].split('.')[0]
    build += str + ' -c ' + qrc + ' -o ' + filename + '.o' + '\n'
    qrc_compiled.append(filename + '.o')

build += str + ' -c unity.cpp -o score.o'


with open('build.sh', 'w') as f:
    f.write(build + '\n')
