#! /bin/bash

set -eu

commit="4a74a4c9aa0b9235f829d35098e891231e9f2b36"

git clone https://gitlab.freedesktop.org/accountsservice/accountsservice.git \
    --depth 1 \
    --revision "$commit" \
    /tmp/accountsservice

cd /tmp/accountsservice

optional_set_libdir=""
os_id="$(grep '^ID=' /etc/os-release)"
if [[ "$os_id" == "ID=org.gnome.os" ]]; then
    optional_set_libdir="--libdir=lib/$(gcc -print-multiarch)"
fi

meson setup _build \
    -Dprefix=/usr \
    $optional_set_libdir \
    -Dcreate_homed=true
meson compile -C _build
meson install -C _build

cd ..
rm -rf /tmp/accountsservice/
