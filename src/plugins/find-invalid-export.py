import glob
import re

root_dir = './'

for filename in glob.iglob(root_dir + '**/*', recursive=True):
  if "cpp" in filename or "hpp" in filename:
    current_export = filename.split('/')[1].replace('-', '_').upper() + "_EXPORT"
    with open(filename, 'r') as reader:
      file = reader.read()
      res = re.findall( r'(SCORE_(LIB|PLUGIN)_[A-Z]+_EXPORT)', file) 
      for export in res: 
        if export[0] != current_export:
          print(filename + ": expected " + current_export + " ; got : " + export[0])
