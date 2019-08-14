#!/usr/bin/python

# rg --vimgrep 'include <Q.*>' > /tmp/include_log.txt

with open('/tmp/include_log.txt','r') as f:
    include_log = f.read()

include_log = include_log.splitlines()
line_log = []
for line in include_log:
    line = line.split(':')
    file = '/home/jcelerier/score/src/' + line[0]
    include = line[3][10:-1].strip()

    line_log.append([file, include])

cur_filename = ""
cur_file = ""
cur_file_with_includes = ""
for filename, include in line_log:
    # if filename != '/home/jcelerier/score/src/lib/core/presenter/AboutDialog.cpp':
    #     continue

    if filename != cur_filename:
        if cur_filename != "":
            with open(cur_filename, 'w') as f:
                f.write(cur_file_with_includes)

        cur_filename = filename        

        with open(filename, 'r') as f:
            cur_file = ""
            cur_file_with_includes = f.read()
            f.seek(0)
            for line in f:
                if not '#include' in line:
                    cur_file = cur_file + line

    if not (include in cur_file):
        fixed_file = ""
        full_include = '#include <' + include + '>'
        print(include + " not found in " + filename)
        for line in cur_file_with_includes.splitlines():
            if not full_include in line:
                fixed_file = fixed_file + line + "\n"
        cur_file_with_includes = fixed_file
        # print(cur_file_wiugth_includes)


with open(cur_filename, 'w') as f:
    f.write(cur_file_with_includes)