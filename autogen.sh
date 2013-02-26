#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="gnome-initial-setup"
ACLOCAL_FLAGS="-I libgd $ACLOCAL_FLAGS"

(test -f $srcdir/configure.ac \
  && test -f $srcdir/gnome-initial-setup.doap) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome-initial-setup directory"
    exit 1
}

which gnome-autogen.sh || {
    echo "You need to install gnome-common"
    exit 1
}

git submodule update --init --recursive

cd egg-list-box
sh autogen.sh --no-configure
cd ..

REQUIRED_AUTOMAKE_VERSION=1.7
. gnome-autogen.sh
