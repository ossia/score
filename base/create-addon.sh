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
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [[ -f /etc/arch-release ]] ; then
  RENAME=$(command -v perl-rename)
else
  RENAME=$(command -v rename)
fi
if [[ "$RENAME" == "" ]]; then
  echo "Install perl-rename or rename"
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

cp -rf "$DIR/addon-skeleton" "$DIR/addons/score-addon-$ADDON"
ADDON_DIR="$DIR/addons/score-addon-$ADDON"
mv "$ADDON_DIR/Skeleton" "$ADDON_DIR/$ADDON"
$RENAME "s/skeleton/$ADDON_LC/" $ADDON_DIR/**/*.{hpp,cpp,txt} 
$RENAME "s/Skeleton/$ADDON/" $ADDON_DIR/**/*.{hpp,cpp,txt} 
$SED -i "s/skeleton/$ADDON_LC/g" $ADDON_DIR/**/*.{hpp,cpp,txt}
$SED -i "s/Skeleton/$ADDON/g" $ADDON_DIR/**/*.{hpp,cpp,txt}
perl -pi -e 'chomp(my $uidgen = `uuidgen`);s|00000000-0000-0000-0000-000000000000|$uidgen|gi' $ADDON_DIR/**/*.{hpp,cpp}
