#!/bin/sh
#
# How to build a solaris package, by Andreas Spångberg:
# ./configure
# make
# cd solaris
# ./buildpkg.sh
# pkgadd -d ./ASkobodl.pkg
#

ARCH=`uname -p`
INSTDIR=`pwd`/package
cd ..
make install DESTDIR=$INSTDIR
cd $INSTDIR

cat > pkginfo << EOPKG
PKG="ASkobodl"
NAME=`grep PACKAGE ../../aconfig.h | awk -e '{print $3}'`
ARCH="$ARCH"
VERSION=`grep VERSION ../../aconfig.h | awk -e '{print $3}'`
CATEGORY="games"
VENDOR="David Olofson"
EMAIL="david@olofson.net"
PSTAMP="Andreas Spångberg"
BASEDIR="/usr/local"
CLASSES="none"
EOPKG

echo "i pkginfo=./pkginfo" > prototype
find . -print | pkgproto ./usr/local= >> prototype

cd ..
pkgmk -d . -f package/prototype -o
rm -rf $INSTDIR
echo 1 | pkgtrans -os . ASkobodl.pkg
rm -rf ./ASkobodl
