#!/bin/sh

set -e

PREFIX=$(grep ^INSTALL_PATH < ~/.scbuild | cut -b 14-)/lib/
if test -z "$PREFIX"; then
   PREFIX=$(grep ^REAL_INSTALL_PATH < ~/.scbuild | cut -b 19-)/lib/
fi
if test -z "$PREFIX"; then
    echo "No INSTALL_PATH or REAL_INSTALL_PATH is indicated in your ~/.scbuild"
    exit 1
fi

cp -r fontman/fontman.scu fontman/liberation garbage/garbage.scu libconfig/libconfig.scu scui/scui.scu scui/uranium celnet/celnet.scu vectoracious quantum/quantum.scu $PREFIX
