# About StudWorks

StudWorks is a free CAD program for creating virtual LEGO® models with the
LDraw parts library, built on LeoCAD. It is released under the GNU General
Public License version 2 and runs on Linux, Windows and macOS.

Models keep LeoCAD's file format and open in either program. LeoCAD's
settings, part palettes and minifigure templates are copied on the first start.


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

Installing the latest video card drivers is recommended, since StudWorks uses
OpenGL for hardware accelerated rendering.


# Online Resources

- Project page:
  https://github.com/nathaneltitane/studworks

- Bug reports and feature requests:
  https://github.com/nathaneltitane/studworks/issues


# Legal Disclaimer

LEGO® is a trademark of the LEGO® Group, which does not sponsor, authorize or endorse this project.
