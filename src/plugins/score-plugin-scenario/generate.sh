#!/bin/bash

function make_file
{
	TEMPLATE="#pragma once
#include <QNamedObject>

class CLASSNAME : public
{
	Q_OBJECT
	
	public:
	
		virtual ~CLASSNAME() = default;
		
	private:
	
};
"
	FILEPATH="$1"
	FILENAME=$(basename "$FILEPATH")
	FILETYPE=$(echo $FILENAME | cut -d'.' -f 2)
	CLASSNAME=$(echo $FILENAME | cut -d'.' -f 1)
	
	if [[ "$FILETYPE" = "cpp" ]]; then
		echo "#include \"$CLASSNAME.hpp\"" > $FILEPATH
	fi
	
	#if [[ "$FILETYPE" = "hpp" ]]; then
	#	echo "$TEMPLATE" | sed s/CLASSNAME/"$CLASSNAME"/ > $FILEPATH
	#fi
	
}

export -f make_file

find . -type f -exec bash -c 'make_file "$0"' {} \;
