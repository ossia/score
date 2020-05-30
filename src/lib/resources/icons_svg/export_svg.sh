#!/bin/bash
svg_dir=$1
png_dir=$2
if [ "$#" -ne 3 ]; then
    w=$3
    h=$4
else
    w=$3
    h=$3
fi

for d in $svg_dir ; do
        for f in `find $d -type f -name "*.svg"` ; do
                filename=$(basename $f)
                extension="${filename##*.}"
                filename="${filename%.*}"
                
                echo "Now Processing File: ${filename}"
                inkscape --export-filename="$png_dir/$filename".png -w $w -h $h "$f"
                inkscape --export-filename="$png_dir/$filename"@2x.png -w $((2*$w)) -h  $((2*$h)) "$f"
        done
done
