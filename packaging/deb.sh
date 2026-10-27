#!/bin/bash

# build a studworks deb that carries its own qt libraries
# usage: packaging/deb.sh [build directory]

set -e

directory_source="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd)"
directory_build="${1:-${directory_source}/build/deb}"

package_architecture="$(dpkg --print-architecture)"

source "${directory_source}/packaging/bundle.sh"

# depend on the system libraries the package links, the bundled ones resolve to the package itself
directory_shlibs="$(mktemp -d "${directory_build}/shlibs.XXXXXX")"
mkdir -p "${directory_shlibs}/debian"
printf "%s\n" "Source: studworks" "" "Package: studworks" "Architecture: any" > "${directory_shlibs}/debian/control"

for file_library in "${directory_root}"/lib/*
do
	name_library="${file_library##*/}"
	printf "%s %s studworks\n" "${name_library%%.so.*}" "${name_library#*.so.}" >> "${directory_shlibs}/debian/shlibs.local"
done

text_depends="$(cd "${directory_shlibs}" && dpkg-shlibdeps -O --ignore-missing-info -e "${list_files[@]}" 2> /dev/null)"
text_depends="${text_depends#shlibs:Depends=}"

# drop the package itself and the abi package that only the bundled qt needs
list_depends=()

while read -r -d "," text_depend
do
	if [[ ${text_depend} != studworks* && ${text_depend} != qtbase-abi-* ]]
	then
		list_depends+=("${text_depend}")
	fi
done <<< "${text_depends//, /,},"

text_depends="$(IFS=","; printf "%s" "${list_depends[*]}")"
text_depends="${text_depends//,/, }"

if [ -z "${text_depends}" ]
then
	printf "%s\n" "dependencies not found"
	exit 1
fi

# write control file
size_installed="$(du -sk "${directory_package}")"
size_installed="${size_installed%%[[:space:]]*}"

mkdir -p "${directory_package}/DEBIAN"

cat > "${directory_package}/DEBIAN/control" << CONTROL
Package: studworks
Version: ${package_version}
Architecture: ${package_architecture}
Maintainer: Nathanel Titane <nathanel.titane@gmail.com>
Installed-Size: ${size_installed}
Depends: ${text_depends}
Recommends: povray
Section: graphics
Priority: optional
Homepage: https://github.com/nathaneltitane/studworks
Description: create virtual LEGO® models
 StudWorks is a CAD application for creating virtual LEGO® models with
 the LDraw parts library, which comes bundled with the package.
CONTROL

file_package="${directory_build}/studworks_${package_version}_${package_architecture}.deb"
chmod 755 "${directory_package}"

dpkg-deb --root-owner-group --build "${directory_package}" "${file_package}"

printf "%s\n" "${file_package}"
