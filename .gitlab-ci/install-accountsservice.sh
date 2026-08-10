#! /bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

# accountsservice version 26.27.3
commit="4e254d9624cc48224c9920b9509df0b144f7be0c"

tmp_source_dir="$(mktemp --tmpdir --directory accountservice-XXXXXX)"

git clone https://gitlab.freedesktop.org/accountsservice/accountsservice.git \
    --depth 1 \
    --no-tags \
    --revision "$commit" \
    "${tmp_source_dir}"

pushd "${tmp_source_dir}"

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

popd
rm -rf "${tmp_source_dir}"
