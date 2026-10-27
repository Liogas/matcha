#!/bin/sh
# Crée une bibliothèque à partir de libs/_template.
# Usage : libs/new-lib.sh <nom> <namespace>     ex. : libs/new-lib.sh cpp-core core
set -eu

name=${1:?usage: $0 <nom> <namespace>}
ns=${2:?usage: $0 <nom> <namespace>}

case $name in cpp-*) ;; *) echo "le nom doit commencer par cpp-" >&2; exit 1 ;; esac
macro=$(printf '%s' "$name" | tr 'a-z-' 'A-Z_')        # cpp-core -> CPP_CORE

libs=$(dirname "$0")
dest=$libs/$name
[ ! -e "$dest" ] || { echo "$dest existe déjà" >&2; exit 1; }

cp -r "$libs/_template" "$dest"
mv "$dest/cmake/cpp-templateConfig.cmake.in" "$dest/cmake/${name}Config.cmake.in"
mv "$dest/include/tmpl" "$dest/include/$ns"

find "$dest" -type f -exec sed -i \
    -e "s/cpp-template/$name/g" \
    -e "s/CPP_TEMPLATE/$macro/g" \
    -e "s/tmpl/$ns/g" \
    {} +

echo "Créé : $dest"
echo "Reste à faire : ajouter add_subdirectory($name) dans $libs/CMakeLists.txt"