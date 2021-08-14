#!/bin/bash -eux
svg_dir=$1
png_dir=$2
if [ "$#" -ne 3 ]; then
    w=$3
    h=$4
else
    w=$3
    h=$3
fi

if [[ -d "$svg_dir" ]]; then
echo "DIR $svg_dir"
        for f in `find $svg_dir -type f -name "*.svg"` ; do
                filename=$(basename $f)
                extension="${filename##*.}"
                filename="${filename%.*}"
                
                echo "Now Processing File: ${filename}"
                inkscape --export-filename="$png_dir/$filename".png -w $w -h $h "$f"
                inkscape --export-filename="$png_dir/$filename"@2x.png -w $((2*$w)) -h  $((2*$h)) "$f"
        done
elif [[ -f "$svg_dir" ]]; then
echo "FILE $svg_dir"
                filename=$(basename "$svg_dir")
                extension="${filename##*.}"
                filename="${filename%.*}"

                echo "Now Processing File: ${filename}"
                inkscape --export-filename="$png_dir/$filename".png -w $w -h $h "$svg_dir"
                inkscape --export-filename="$png_dir/$filename"@2x.png -w $((2*$w)) -h  $((2*$h)) "$svg_dir"
fi

