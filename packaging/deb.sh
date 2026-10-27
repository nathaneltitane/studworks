#!/bin/bash

# build a studworks deb that carries its own qt libraries
# usage: packaging/deb.sh [build directory]

set -e

directory_source="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd)"
directory_build="${1:-${directory_source}/build/deb}"
directory_prefix="/opt/studworks"

file_library="https://library.ldraw.org/library/updates/complete.zip"

package_architecture="$(dpkg --print-architecture)"

# version from the release date
while read -r line_header
do
	if [[ ${line_header} =~ SW_VERSION_TEXT\ \"([0-9-]+)\" ]]
	then
		package_version="${BASH_REMATCH[1]}"
	fi
done < "${directory_source}/source/application/sw_global.h"

if [ -z "${package_version}" ]
then
	printf "%s\n" "version not found"
	exit 1
fi

# libraries the system provides
list_libraries_system=(
	ld-linux libc.so libm.so libdl.so libpthread.so librt.so libresolv.so libutil.so
	libstdc++.so libgcc_s.so
	libGL.so libGLX.so libGLdispatch.so libEGL.so libOpenGL.so libdrm.so libgbm.so
	libX11.so libX11-xcb.so libxcb.so libXext.so libXau.so libXdmcp.so
	libfontconfig.so libfreetype.so libz.so libdbus-1.so
	libglib-2.0.so libgthread-2.0.so libgobject-2.0.so libgmodule-2.0.so
	libcups.so
)

# qt plugins to carry
list_plugins=(
	platforms/libqxcb.so
	platforms/libqoffscreen.so
	platforms/libqminimal.so
	xcbglintegrations/libqxcb-glx-integration.so
	xcbglintegrations/libqxcb-egl-integration.so
	iconengines/libqsvgicon.so
	platforminputcontexts/libcomposeplatforminputcontextplugin.so
	platforminputcontexts/libibusplatforminputcontextplugin.so
	printsupport/libcupsprintersupport.so
	platformthemes/libqxdgdesktopportal.so
)

# download parts library
if [ ! -s "${directory_source}/library.bin" ]
then
	wget -q "${file_library}" -O "${directory_source}/library.bin"
fi

# build and install
mkdir -p "${directory_build}"
directory_build="$(cd "${directory_build}" && pwd)"
directory_package="$(mktemp -d "${directory_build}/package.XXXXXX")"
directory_root="${directory_package}${directory_prefix}"

cd "${directory_build}"

qmake "${directory_source}/studworks.pro" \
	CONFIG+=release CONFIG-=debug \
	BUNDLE_LIBRARIES=1 \
	INSTALL_PREFIX="${directory_prefix}" \
	BIN_DIR="${directory_prefix}/bin" \
	DOCS_DIR=/usr/share/doc/studworks \
	ICON_DIR=/usr/share/icons \
	MAN_DIR=/usr/share/man/man1 \
	DESKTOP_DIR=/usr/share/applications \
	MIME_DIR=/usr/share/mime/packages \
	APPDATA_DIR=/usr/share/metainfo

make -j "$(nproc)"
make install INSTALL_ROOT="${directory_package}"

# copy qt plugins
directory_plugins_qt="$(qmake -query QT_INSTALL_PLUGINS)"
list_files=("${directory_root}/bin/studworks")

for plugin in "${list_plugins[@]}"
do
	if [ -f "${directory_plugins_qt}/${plugin}" ]
	then
		mkdir -p "${directory_root}/plugins/${plugin%/*}"
		cp "${directory_plugins_qt}/${plugin}" "${directory_root}/plugins/${plugin}"
		list_files+=("${directory_root}/plugins/${plugin}")
	fi
done

for plugin in "${directory_plugins_qt}"/imageformats/libq*.so
do
	if [ -f "${plugin}" ]
	then
		mkdir -p "${directory_root}/plugins/imageformats"
		cp "${plugin}" "${directory_root}/plugins/imageformats/"
		list_files+=("${directory_root}/plugins/imageformats/${plugin##*/}")
	fi
done

printf "%s\n" "[Paths]" "Prefix = .." "Plugins = plugins" > "${directory_root}/bin/qt.conf"

# use to list resolved dependencies as name path pairs
list_dependencies() {
	local line_ldd
	local name_library
	local path_library

	while read -r line_ldd
	do
		if [[ ${line_ldd} =~ ^([^[:space:]]+)\ =\>\ (/[^[:space:]]+) ]]
		then
			name_library="${BASH_REMATCH[1]}"
			path_library="${BASH_REMATCH[2]}"
			printf "%s %s\n" "${name_library}" "${path_library}"
		fi
	done < <(ldd "${@}" 2> /dev/null)
}

# collect dependencies
declare -A list_libraries_found
declare -A list_libraries_skipped

while read -r name_library path_library
do
	list_libraries_found["${name_library}"]="${path_library}"
done < <(list_dependencies "${list_files[@]}")

# skip system libraries and everything they pull in
for name_library in "${!list_libraries_found[@]}"
do
	for name_system in "${list_libraries_system[@]}"
	do
		if [[ ${name_library} == "${name_system}"* ]]
		then
			list_libraries_skipped["${name_library}"]=1

			while read -r name_dependency path_dependency
			do
				list_libraries_skipped["${name_dependency}"]=1
			done < <(list_dependencies "${list_libraries_found[${name_library}]}")
		fi
	done
done

# copy bundled libraries
mkdir -p "${directory_root}/lib"

for name_library in "${!list_libraries_found[@]}"
do
	if [ -z "${list_libraries_skipped[${name_library}]}" ]
	then
		cp -L "${list_libraries_found[${name_library}]}" "${directory_root}/lib/${name_library}"
	fi
done

# stop when qt was not bundled
if [ ! -f "${directory_root}/lib/libQt5Core.so.5" ]
then
	printf "%s\n" "qt libraries not found"
	exit 1
fi

# link the command
mkdir -p "${directory_package}/usr/bin"
ln -sf "../..${directory_prefix}/bin/studworks" "${directory_package}/usr/bin/studworks"

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
Depends: libc6 (>= 2.35), libstdc++6, libgcc-s1, libgl1, libegl1, libx11-6, libx11-xcb1, libxcb1, libfontconfig1, libfreetype6, libdbus-1-3, libglib2.0-0 | libglib2.0-0t64, zlib1g, libcups2 | libcups2t64
Recommends: povray
Section: graphics
Priority: optional
Homepage: https://github.com/nathaneltitane/studworks
Description: create virtual LEGO models
 StudWorks is a CAD application for creating virtual LEGO models with
 the LDraw parts library, which comes bundled with the package.
CONTROL

file_package="${directory_build}/studworks_${package_version}_${package_architecture}.deb"
chmod 755 "${directory_package}"

dpkg-deb --root-owner-group --build "${directory_package}" "${file_package}"

printf "%s\n" "${file_package}"
