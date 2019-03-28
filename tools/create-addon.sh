#!/bin/bash
shopt -s globstar

if [[ "$#" -ne 1 ]]; then
    echo "Usage: ./create-addon.sh MyAddonName"
    exit 1
fi

if [[ $(echo "$1" | head -c 1) == "-" ]]; then
    echo "Usage: ./create-addon.sh MyAddonName"
    exit 1
fi

ADDON="$1"
ADDON_LC=$(echo $ADDON | perl -ne 'print lc')

RENAME=$(command -v perl-rename)
if [[ ! -x "$RENAME" ]] ; then
  RENAME=$(command -v rename)
fi

RENAME_KIND=$($RENAME --help | grep PERLEXPR)
if [[ "$RENAME_KIND" == "" ]]; then
  echo "Install perl-rename (sometimes called just 'rename')"
  exit 1
fi

SED=/usr/bin/sed
if [[ "$OSTYPE" == "darwin"* ]]; then
  if ! [[ -x "$(command -v gsed)" ]]; then 
    echo "Install gnu-sed"
    exit 1
  fi
  SED=/usr/bin/gsed
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../src && pwd )"
if [[ -x "$DIR/addons/score-addon-$ADDON" ]]; then
    echo "Addon score-addon-$ADDON already exists, choose another name"
    exit 1
fi
cp -rf "$DIR/addon-skeleton" "$DIR/addons/score-addon-$ADDON"
ADDON_DIR="$DIR/addons/score-addon-$ADDON"
mv "$ADDON_DIR/Skeleton" "$ADDON_DIR/$ADDON"
$RENAME "s/skeleton/$ADDON_LC/" $ADDON_DIR/**/*.{hpp,cpp,txt} 
$RENAME "s/Skeleton/$ADDON/" $ADDON_DIR/**/*.{hpp,cpp,txt} 
$SED -i "s/skeleton/$ADDON_LC/g" $ADDON_DIR/**/*.{hpp,cpp,txt}
$SED -i "s/Skeleton/$ADDON/g" $ADDON_DIR/**/*.{hpp,cpp,txt}
echo "A new and wonderful score add-on" > $ADDON_DIR/README.md

perl -pi -e 'chomp(my $uidgen = `uuidgen`);s|00000000-0000-0000-0000-000000000000|$uidgen|gi' $ADDON_DIR/**/*.{hpp,cpp}

echo "Addon score-addon-$ADDON successfully initialized"
