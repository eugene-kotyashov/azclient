#!/bin/sh
set -ex
output="AzireVPN-${VERSION}.dmg"
cd "$(dirname "$0")"
rm -rf *dmg
mkdir dmg
cp -R ../../azclient.app dmg/AzireVPN.app
"${QTDIR}/bin/macdeployqt" dmg/AzireVPN.app -codesign="Nessla AB"
hdiutil create -megabytes 120 -fs "HFS+" -format UDZO -volname "AzireVPN" -srcfolder dmg "$output"
rm -rf dmg
