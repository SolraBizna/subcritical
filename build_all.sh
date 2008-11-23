#!/bin/sh

. packages.sh

lua scbuild.lua all install || exit 1
for package in $PACKAGES; do
  echo cd $package
  cd $package
  lua ../scbuild.lua all install || exit 1
  echo cd ..
  cd ..
done
