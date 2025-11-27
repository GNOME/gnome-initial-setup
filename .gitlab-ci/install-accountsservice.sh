#! /bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

# Current tip of main
commit="4a74a4c9aa0b9235f829d35098e891231e9f2b36"

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
