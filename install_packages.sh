#!/bin/sh

set -e

PREFIX=$(sed -rne "s/^.*INSTALL_PATH=(.+)/\\1/p" < ~/.scbuild)/lib/

cp -r fontman/fontman.scu fontman/liberation garbage/garbage.scu libconfig/libconfig.scu scui/scui.scu scui/uranium celnet/celnet.scu vectoracious quantum/quantum.scu $PREFIX
