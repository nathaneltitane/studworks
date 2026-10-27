# About StudWorks

StudWorks is a free CAD program for creating virtual LEGO® models with the
LDraw parts library, built on LeoCAD. It is released under the GNU General
Public License version 2 and runs on Linux, Windows and macOS.

Models keep LeoCAD's file format and open in either program. LeoCAD's
settings, part palettes and minifigure templates are copied on the first start.


# Installing

Packages are published for every release and carry its date as their
version: .deb, .rpm and AppImage for x86_64 and arm64, a Windows installer
and a universal macOS disk image. Download them from the latest release:

  https://github.com/nathaneltitane/studworks/releases/latest

or from the continuous build of the latest changes:

  https://github.com/nathaneltitane/studworks/releases/tag/continuous

The Linux packages carry their own Qt libraries and the LDraw parts library,
and only need the base system libraries. The macOS disk image is not
notarized: on first start, right click StudWorks and choose Open.


# Building

StudWorks is built from source. Install a C++ compiler, Qt 5 (widgets, OpenGL,
print support, network, xml and concurrent modules) and the zlib development
package, then run:

    git clone https://github.com/nathaneltitane/studworks
    mkdir studworks-build && cd studworks-build
    qmake ../studworks/studworks.pro
    make -j"$(nproc)"

Start build/release/studworks. If no parts library is found, download one from
https://library.ldraw.org/updates?latest and point Preferences to it, or set
the STUDWORKS_LIB environment variable.

To build a package, run packaging/deb.sh on Debian or Ubuntu,
packaging/rpm.sh on an RPM distribution or packaging/appimage.sh on any
Linux.

Installing the latest video card drivers is recommended, since StudWorks uses
OpenGL for hardware accelerated rendering.


# Online Resources

- Project page:
  https://github.com/nathaneltitane/studworks

- Bug reports and feature requests:
  https://github.com/nathaneltitane/studworks/issues


# Legal Disclaimer

LEGO® is a trademark of the LEGO® Group, which does not sponsor, authorize or endorse this project.
