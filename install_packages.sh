#!/bin/sh

set -e

PREFIX=$(grep ^INSTALL_PATH < ~/.scbuild | cut -b 14-)/lib/

cp -r fontman/fontman.scu fontman/liberation garbage/garbage.scu libconfig/libconfig.scu scui/scui.scu scui/uranium celnet/celnet.scu vectoracious quantum/quantum.scu $PREFIX
