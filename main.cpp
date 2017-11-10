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

	QSharedMemory uniqueApp("AZCLIENT-SINGLE-APP-MEMORY-OBJECT");
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

	ConnectionWindow window;
	window.show();

	return app.exec();
}
