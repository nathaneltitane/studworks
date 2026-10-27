#!/bin/bash

# build a studworks rpm that carries its own qt libraries
# usage: packaging/rpm.sh [build directory]

set -e

directory_source="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd)"
directory_build="${1:-${directory_source}/build/rpm}"

package_architecture="$(uname -m)"

# qt tools live outside the path on rpm systems
directory_qt="$(rpm --eval '%{_libdir}')/qt5/bin"

if [ -d "${directory_qt}" ]
then
	PATH="${directory_qt}:${PATH}"
fi

source "${directory_source}/packaging/bundle.sh"

# bundled libraries are not required from the system
list_bundled=()

for file_library in "${directory_root}"/lib/*
do
	name_library="${file_library##*/}"
	list_bundled+=("${name_library//+/[+]}")
done

IFS="|"
text_bundled="${list_bundled[*]}"
unset IFS

# rpm versions take no hyphens
directory_rpm="${directory_build}/rpmbuild"
mkdir -p "${directory_rpm}"

rpmbuild -bb "${directory_source}/packaging/studworks.spec" \
	--define "_topdir ${directory_rpm}" \
	--define "sw_version ${package_version//-/}" \
	--define "sw_root ${directory_package}" \
	--define "sw_bundled ${text_bundled}"

file_package="${directory_build}/studworks-${package_version}-${package_architecture}.rpm"

mv "${directory_rpm}/RPMS/${package_architecture}/studworks-${package_version//-/}-1.${package_architecture}.rpm" "${file_package}"

printf "%s\n" "${file_package}"
