#!/bin/bash
set +eux
QT_ENV_SCRIPT=$(find /opt -name 'qt*-env.sh')
source $QT_ENV_SCRIPT
set -eux

cninja dynamic-release
