#!/bin/bash
svg_dir=$1
png_dir=$2
size=$3
for d in $svg_dir ; do
        for f in `find $d -type f -name "*.svg"` ; do
                filename=$(basename $f)
                extension="${filename##*.}"
                filename="${filename%.*}"
                
                echo "Now Processing File: ${filename}"
                inkscape --export-filename="$png_dir/$filename".png -w $size -h $size "$f"
                inkscape --export-filename="$png_dir/$filename"@2x.png -w $((2*$size)) -h  $((2*$size)) "$f"
        done
done
