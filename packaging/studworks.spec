# packages the tree staged by packaging/rpm.sh, which defines sw_version sw_root and sw_bundled

%global debug_package %{nil}
%global _build_id_links none

# require only what the system provides
%global __provides_exclude_from ^/opt/studworks/.*$
%global __requires_exclude ^(%{sw_bundled})

# the kde thumbnail plugin only loads where kde already provides its frameworks
%global __requires_exclude_from ^%{_libdir}/qt5/plugins/kf5/.*$

Name: studworks
Version: %{sw_version}
Release: 1
Summary: Create virtual LEGO® models
License: GPL-3.0-only
URL: https://github.com/nathaneltitane/studworks
Packager: Nathanel Titane <nathanel.titane@gmail.com>
Recommends: povray

%description
StudWorks is a CAD application for creating virtual LEGO® models with
the LDraw parts library, which comes bundled with the package.

%install
cp -a %{sw_root}/. %{buildroot}/

%files
/opt/studworks
%{_bindir}/studworks
%{_datadir}/applications/studworks.desktop
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/icons/hicolor/*/mimetypes/*
%{_datadir}/mime/packages/studworks.xml
%{_datadir}/metainfo/io.github.nathaneltitane.StudWorks.appdata.xml
%{_datadir}/thumbnailers/studworks.thumbnailer
%{?sw_thumbnailer}
%{_datadir}/doc/studworks
%{_mandir}/man1/studworks.1*
