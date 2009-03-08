#!/bin/bash

. packages.sh

lua scbuild.lua clean
for package in $PACKAGES; do
  echo cd $package
  cd $package
  lua ../scbuild.lua clean
  echo cd ..
  cd ..
done
