#!/bin/sh
# Run this to generate all the initial makefiles, etc.

REQUIRED_AUTOMAKE_VERSION=1.7
USE_GNOME2_MACROS=1

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="gnome-initial-setup"

(test -f $srcdir/configure.ac \
  && test -f $srcdir/gnome-initial-setup.doap) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome-settings-daemon directory"
    exit 1
}

# Fetch submodules if needed
if test ! -f egg-list-box/COPYING;
then
  echo "+ Setting up submodules"
  git submodule init
fi
git submodule update

cd egg-list-box
sh autogen.sh --no-configure
cd ..

which gnome-autogen.sh || {
    echo "You need to install gnome-common"
    exit 1
}

. gnome-autogen.sh
