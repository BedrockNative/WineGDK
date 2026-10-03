#!/usr/bin/env bash
set -euo pipefail

# Resolve paths from this script so it also works from another directory.
source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd -- "$source_dir"

./configure --prefix="$source_dir/build" --enable-archs=x86_64,i386 --disable-tests
make -j"$(nproc)"
make -j"$(nproc)" install

printf '\nWine installed in %s/build\n' "$source_dir"
