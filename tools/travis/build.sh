#!/bin/bash -eux
set -o pipefail
export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '
# Note : to make the tests work under travis, they have to be changed in order not to require QApplication but only QCoreApplication
#    - LD_LIBRARY_PATH=/usr/lib64 make ExperimentalTest

export CTEST_OUTPUT_ON_FAILURE=1
export CONFIG_FOLDER="$(pwd)/tools/travis/configs"
source "$(pwd)/tools/travis/configs/$CONF.sh"
