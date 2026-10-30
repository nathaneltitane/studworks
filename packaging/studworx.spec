# packages the tree staged by packaging/rpm.sh, which defines sw_version sw_root and sw_bundled

%global debug_package %{nil}
%global _build_id_links none

# require only what the system provides
%global __provides_exclude_from ^/opt/studworx/.*$
%global __requires_exclude ^(%{sw_bundled})

# the kde thumbnail plugin only loads where kde already provides its frameworks
%global __requires_exclude_from ^%{_libdir}/qt5/plugins/kf5/.*$

Name: studworx
Version: %{sw_version}
Release: 1
Summary: LDraw / LEGO® model builder
License: GPL-3.0-only
URL: https://github.com/nathaneltitane/studworx
Packager: Nathanel Titane <nathanel.titane@gmail.com>
Recommends: povray

%description
StudWorx is a CAD application for creating virtual LEGO® models with
the LDraw parts library, which comes bundled with the package.

%install
cp -a %{sw_root}/. %{buildroot}/

%files
/opt/studworx
%{_bindir}/studworx
%{_datadir}/applications/studworx.desktop
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/icons/hicolor/*/mimetypes/*
%{_datadir}/mime/packages/studworx.xml
%{_datadir}/metainfo/io.github.nathaneltitane.StudWorx.appdata.xml
%{_datadir}/thumbnailers/studworx.thumbnailer
%{?sw_thumbnailer}
%{_datadir}/doc/studworx
%{_mandir}/man1/studworx.1*
