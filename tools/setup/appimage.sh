qmake PREFIX=/usr -r
make clean
make -j4
sudo apt-get -y install checkinstall
rm ./*.spec # Otherwise the next line fails
sudo checkinstall --pkgname=app --pkgversion="1" --pkgrelease="1" --backup=no --fstrans=no --default --deldoc
mkdir appdir ; cd appdir
dpkg -x ../app_1-1_amd64.deb . ; find .
cp ../qt/studworks.desktop .
sed -i -e 's|\.svg||g' studworks.desktop # Workaround
cp ../resources/studworks.png .
mkdir usr/share/studworks
cp ../library.bin usr/share/studworks/library.bin
cd ..
wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt*.AppImage
unset QTDIR; unset QT_PLUGIN_PATH ; unset LD_LIBRARY_PATH
./linuxdeployqt*.AppImage ./appdir/usr/bin/studworks -bundle-non-qt-libs
./linuxdeployqt*.AppImage ./appdir/usr/bin/studworks -appimage
