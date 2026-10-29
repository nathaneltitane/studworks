#!/bin/bash

# build a studworks appimage that carries its own qt libraries
# usage: packaging/appimage.sh [build directory]

set -e

directory_script="${BASH_SOURCE[0]%/*}"

# run from its own directory
if [ "${directory_script}" = "${BASH_SOURCE[0]}" ]
then
	directory_script="."
fi

directory_source="$(cd "${directory_script}/.." && pwd)"
directory_build="${1:-${directory_source}/build/appimage}"

package_architecture="$(uname -m)"

source "${directory_source}/packaging/bundle.sh"

# lay out the appdir root
cp "${directory_source}/packaging/AppRun" "${directory_package}/AppRun"
chmod 755 "${directory_package}/AppRun"
cp "${directory_source}/platform/studworks.desktop" "${directory_package}/studworks.desktop"
cp "${directory_source}/resources/icons/256x256/apps/studworks.png" "${directory_package}/studworks.png"
ln -sf studworks.png "${directory_package}/.DirIcon"

# download appimagetool
file_tool="${directory_build}/appimagetool-${package_architecture}.AppImage"

if [ ! -s "${file_tool}" ]
then
	curl -fsSL "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${package_architecture}.AppImage" -o "${file_tool}"
	chmod 755 "${file_tool}"
fi

file_package="${directory_build}/studworks-linux-${package_version}-${package_architecture}.appimage"

ARCH="${package_architecture}" VERSION="${package_version}" APPIMAGE_EXTRACT_AND_RUN=1 "${file_tool}" --no-appstream "${directory_package}" "${file_package}"

printf "%s\n" "${file_package}"
