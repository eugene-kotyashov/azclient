/*
 * Copyright (C) 2017 Nessla AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ConnectionWindow.h"
#include "LogWindow.h"
#include "StatusIcon.h"
#include "VpnApi.h"
#include "customization.h"
#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QFile>
#include <QStyle>
#include <QLocale>
#include <QTranslator>
#include <QSysInfo>
#include <QProcess>
#include <QDir>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName(NAME);
	app.setOrganizationName(ORGANIZATION);
	app.setApplicationVersion(VERSION);
	app.setWindowIcon(QIcon(":icons/app.svg"));
    app.setStyle("fusion");

	QTranslator *translator = new QTranslator;
	if (translator->load(QLocale().name(), ":/translations"))
		app.installTranslator(translator);
	else
		delete translator;

	QFile styleSheet(":widgets.css");
	styleSheet.open(QIODevice::ReadOnly);
	app.setStyleSheet(styleSheet.readAll());
	styleSheet.close();

    QSharedMemory uniqueApp("ASTROCLIENT-SINGLE-APP-MEMORY-OBJECT");
	if (!uniqueApp.create(1024)) {
		uniqueApp.attach();
		uniqueApp.detach();
		if (!uniqueApp.create(1024)) {
			QMessageBox::warning(nullptr, QObject::tr("%1 - Already Running").arg(qApp->applicationName()), QObject::tr("%1 is already running.").arg(qApp->applicationName()));
			return 1;
		}
	}

	LogWindow::instance();

	qInfo() << USERAGENT " built on " __DATE__ " at " __TIME__;
	qInfo() << QString("System: %5 | %1 | %2 | %3 | %4 | Qt %6").arg(QSysInfo::prettyProductName(), QSysInfo::productVersion(), QSysInfo::kernelType(), QSysInfo::kernelVersion(), QSysInfo::productType(), qVersion()).toLocal8Bit().data();

    QProcess* process = new QProcess();


    QStringList arguments;
    QString appPath = QDir::toNativeSeparators(qApp->applicationDirPath());

    QString tmp;
    tmp = "netsh advfirewall firewall del rule name=\"astrovpn\""
           "&netsh advfirewall firewall del rule name=\"openvpn\""
           "&netsh advfirewall firewall del rule name=\"openvpn\""

           "&netsh advfirewall firewall add rule name=\"astrovpn\" dir=in action=allow "
                "program=\""+ appPath + "\\astroclient.exe\" enable=yes "
           "&netsh advfirewall firewall add rule name=\"openvpn\" dir=in action=allow "
                "program=\"" +  appPath + "\\lib\\openvpn.exe\" enable=yes "
           "&netsh advfirewall firewall add rule name=\"openvpn\" dir=in action=allow "
            "program=\""  + appPath + "\\lib\\openvpn-lib.exe\" enable=yes";

    arguments << "/c" << tmp;

    qInfo() << "about to run " << "cmd.exe ";
    for (const auto& s : arguments)
            qInfo() << QString(s).toUtf8().constData();

    process->start("cmd.exe", arguments, QIODevice::ReadOnly);

    if (!process->waitForStarted(-1)) {
        qCritical() << "CMD.exe Process Error:" << process->errorString();
    }


    process->waitForFinished(-1);

	ConnectionWindow window;
	window.show();

	return app.exec();
}
