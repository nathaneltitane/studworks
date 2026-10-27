#!/bin/bash

# build a studworks windows installer from a release build, run in git bash after nmake
# usage: packaging/windows.sh [build directory]

set -e

directory_source="$(cd "${BASH_SOURCE[0]%/*}/.." && pwd)"
directory_build="${1:-${directory_source}/build/windows}"

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

# stage the installed files
mkdir -p "${directory_build}"
directory_build="$(cd "${directory_build}" && pwd)"
directory_package="$(mktemp -d "${directory_build}/package.XXXXXX")"

cp "${directory_source}/build/release/StudWorks.exe" "${directory_package}/"
cp "${directory_source}/documentation/README.md" "${directory_package}/readme.txt"

for file_extra in library.bin povconsole32-sse2.exe
do
	if [ -f "${directory_source}/${file_extra}" ]
	then
		cp "${directory_source}/${file_extra}" "${directory_package}/"
	fi
done

windeployqt --release "${directory_package}/StudWorks.exe"
cp "$(cygpath -u "${VCToolsRedistDir}")vc_redist.x64.exe" "${directory_package}/"

# list every staged file and folder for the uninstaller, contents before their folder
file_uninstall="${directory_build}/uninstall.nsh"
: > "${file_uninstall}"

while IFS= read -r path_entry
do
	path_relative="${path_entry#"${directory_package}/"}"
	path_relative="${path_relative//\//\\}"

	if [ -d "${path_entry}" ]
	then
		printf "  RMDir \"\$INSTDIR\\%s\"\n" "${path_relative}" >> "${file_uninstall}"
	else
		printf "  Delete \"\$INSTDIR\\%s\"\n" "${path_relative}" >> "${file_uninstall}"
	fi
done < <(find "${directory_package}" -mindepth 1 -depth)

# build the installer
cp "${directory_source}/packaging/studworks.nsi" "${directory_build}/"

file_package="${directory_build}/studworks-windows-${package_version}-x64.exe"

(cd "${directory_build}" && VERSION="${package_version}" "/c/Program Files (x86)/NSIS/makensis.exe" -V4 "-DDIRECTORY_PACKAGE=${directory_package##*/}" "-XOutFile ${file_package##*/}" studworks.nsi)

printf "%s\n" "${file_package}"
