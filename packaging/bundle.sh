#!/bin/bash

# stage a studworks tree that carries its own qt libraries, sourced by the package scripts
# sets directory_root directory_package list_files and package_version

directory_source="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd)"
directory_prefix="/opt/studworks"

file_library="https://library.ldraw.org/library/updates/complete.zip"

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
	libcups.so libssl.so libcrypto.so
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
	curl -fsSL "${file_library}" -o "${directory_source}/library.bin"
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
	APPDATA_DIR=/usr/share/metainfo \
	THUMBNAILER_DIR=/usr/share/thumbnailers

make -j "$(nproc)"
make install INSTALL_ROOT="${directory_package}"

# kde thumbnail plugin, built against the system frameworks that load it
if [ -f "$(qmake -query QT_INSTALL_ARCHDATA)/mkspecs/modules/qt_KIOGui.pri" ]
then
	mkdir -p "${directory_build}/thumbnailer"
	cd "${directory_build}/thumbnailer"

	qmake "${directory_source}/thumbnailer/thumbnailer.pro" \
		CONFIG+=release CONFIG-=debug \
		BIN_DIR="${directory_prefix}/bin"

	make -j "$(nproc)"
	make install INSTALL_ROOT="${directory_package}"

	cd "${directory_build}"
fi

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
		list_files+=("${directory_root}/lib/${name_library}")
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
