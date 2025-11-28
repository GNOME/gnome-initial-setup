#! /bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

# This is from 0.14.alpha tag
# use the commit to be more reusable
commit="5d1b799a97f7c986269529baff73f44b193338d4"

tmp_source_dir="$(mktemp --tmpdir --directory malcontent-XXXXXX)"

git clone https://gitlab.freedesktop.org/pwithnall/malcontent.git \
    --depth 1 \
    --no-tags \
    --revision "$commit" \
    "${tmp_source_dir}"

pushd "${tmp_source_dir}"

meson setup _build -Dprefix=/usr

meson compile -C _build
meson install -C _build

popd
rm -rf "${tmp_source_dir}"
