VERSION = 0.5

QT += core gui widgets network svg
CONFIG += c++11
TARGET = azclient
TEMPLATE = app

SOURCES += main.cpp \
    ConnectionWindow.cpp \
    VpnApi.cpp \
    LogWindow.cpp \
    StatusIcon.cpp \
    OpenVpnRunner.cpp \
    PowerNotifier.cpp

HEADERS += ConnectionWindow.h \
    VpnApi.h \
    customization.h \
    LogWindow.h \
    StatusIcon.h \
    OpenVpnRunner.h \
    platform-constants.h \
    PowerNotifier.h

TRANSLATIONS += locale/sv.ts locale/nl.ts locale/fr.ts
qtPrepareTool(LRELEASE, lrelease)
system("$$LRELEASE" "$$_PRO_FILE_")
RESOURCES += locale/locales.qrc

RESOURCES += resources/resources.qrc
DEFINES += VERSION=\\\"$$VERSION\\\"

win32 {
	QMAKE_LFLAGS += \"/MANIFESTUAC:level=\'requireAdministrator\' uiAccess=\'false\'\"
	DESTDIR = bin
	RC_ICONS = resources/icons/win/app.ico
	CONFIG += embed_manifest_exe
	extrabin.commands = xcopy \"$$replace(_PRO_FILE_PWD_, /, $$QMAKE_DIR_SEP)\\dist\\win\\bin\\*\" \"$$DESTDIR\\\" /Y /S
	QMAKE_EXTRA_TARGETS += extrabin
	PRE_TARGETDEPS += extrabin
	installer.commands = "cd \"$$replace(_PRO_FILE_PWD_, /, $$QMAKE_DIR_SEP)\dist\win\" && set VERSION=$${VERSION}&& set QTDIR=$$[QT_INSTALL_PREFIX]&& makeinstaller.bat"
	QMAKE_EXTRA_TARGETS += installer
}

macx {
	QMAKE_LFLAGS += -framework IOKit -framework Cocoa
	ICON = resources/icons/mac/app.icns
	BUNDLE_TARGET = "./$${TARGET}.app"
	extrabin.files = $$system(echo $$_PRO_FILE_PWD_/dist/mac/bin/*)
	extrabin.path = "Contents/MacOS"
	QMAKE_BUNDLE_DATA += extrabin
	QMAKE_INFO_PLIST = dist/mac/Info.plist
	installer.commands = "cd \"$$_PRO_FILE_PWD_/dist/mac\" && VERSION=$$VERSION QTDIR=$$[QT_INSTALL_PREFIX] ./makedmg.sh"
	QMAKE_EXTRA_TARGETS += installer
}

linux {
	install.commands = "cd \"$$_PRO_FILE_PWD_/dist/linux\" && ./install.sh"
	QMAKE_EXTRA_TARGETS += install
}
