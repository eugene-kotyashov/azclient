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

#include "StatusIcon.h"
#include "LogWindow.h"
#include "platform-constants.h"
#include "VpnApi.h"
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QSettings>

void StatusIcon::setStatus(StatusIcon::Status status)
{
	QString icon, text;
	m_status = status;
	switch (status) {
	case Logout:
		icon = ":icons/" PLATFORM_ICONS "/disconnected.svg";
		text = tr("Disconnected");
		m_disconnectMenu->setText(tr("&Login"));
		m_transferMenu->setVisible(false);
		break;
	case Disconnected:
		icon = ":icons/" PLATFORM_ICONS "/disconnected.svg";
		text = tr("Disconnected");
		m_disconnectMenu->setText(tr("&Connect"));
		m_transferMenu->setVisible(false);
		break;
	case Connecting:
		icon = ":icons/" PLATFORM_ICONS "/connecting.svg";
		text = tr("Connecting...");
		m_disconnectMenu->setText(tr("&Disconnect"));
		m_transferMenu->setVisible(false);
		break;
	case Connected:
		icon = ":icons/" PLATFORM_ICONS "/connected.svg";
		text = tr("Connected");
		m_disconnectMenu->setText(tr("&Disconnect"));
		showMessage(qApp->applicationName(), tr("%1 is now connected.").arg(qApp->applicationName()), QSystemTrayIcon::Information, 5000);
		break;
	}
	setIcon(QIcon(icon).pixmap(128, 128));
	setToolTip(tr("%1\nStatus: %2").arg(qApp->applicationName(), text));
	m_statusMenu->setText(tr("%1 Status: %2").arg(qApp->applicationName(), text));
}

QString StatusIcon::unit(double bytes) const
{
	if (bytes < 1024.0)
		return QString("%1 B").arg(bytes, 0, 'f', 2);
	if (bytes < 1024.0 * 1024.0)
		return QString("%1 KiB").arg(bytes / 1024.0, 0, 'f', 2);
	if (bytes < 1024.0 * 1024.0 * 1024.0)
		return QString("%1 MiB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
	if (bytes < 1024.0 * 1024.0 * 1024.0 * 1024.0)
		return QString("%1 GiB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
	return QString("%1 TiB").arg(bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

void StatusIcon::setTransfer(quint64 down, quint64 up)
{
	if (m_status != Connected)
		return;

	const QString text = tr("%1 down / %2 up").arg(unit(down), unit(up));
	setToolTip(QString("%1\nStatus: Connected\n%2").arg(qApp->applicationName(), text));
	m_transferMenu->setText(text);
	m_transferMenu->setVisible(true);
}

bool StatusIcon::maybeShowParent() const
{
	if (m_status == Connecting || m_status == Connected)
		return false;
	QWidget *parentWidget = qobject_cast<QWidget *>(parent());
	if (parentWidget) {
		parentWidget->show();
		qApp->setActiveWindow(parentWidget);
	}
	return true;
}

void StatusIcon::quitApp()
{
	static QSettings settings;
	if (!settings.value("LastToken").toString().isEmpty() && !settings.value("RememberCredentials").toBool()) {
		VpnApi *api = new VpnApi(this);
		api->logout(this, settings.value("LastToken").toString(), [=](const QString &, const QString &) {
			settings.remove("LastToken");
			qApp->quit();
		});
	} else {
		qApp->quit();
	}
}

StatusIcon::StatusIcon(QObject *owner) :
	QSystemTrayIcon(owner)
{
	QMenu *menu = new QMenu;
	m_statusMenu = new QAction(menu);
	m_statusMenu->setEnabled(false);
	m_transferMenu = new QAction(menu);
	m_transferMenu->setEnabled(false);
	m_transferMenu->setVisible(false);
	QFont font;
	font.setBold(true);
	m_statusMenu->setFont(font);
	m_disconnectMenu = new QAction(menu);
	connect(this, &QSystemTrayIcon::activated, this, [=](ActivationReason reason) {
#if defined(Q_OS_LINUX)
		if (reason == Trigger)
#else
		if (reason == DoubleClick)
#endif
			maybeShowParent();
	});
	connect(m_disconnectMenu, &QAction::triggered, this, [=]() {
		if (maybeShowParent())
			return;
		setStatus(Disconnected);
		emit disconnect();
	});
	menu->addAction(m_statusMenu);
	menu->addAction(m_transferMenu);
	menu->addSeparator();
	menu->addAction(m_disconnectMenu);
#if QT_VERSION >= 0x050600
	menu->addAction(tr("Show Log"), &LogWindow::instance(), &LogWindow::show);
	menu->addAction(tr("&Exit %1").arg(qApp->applicationName()), this, &StatusIcon::quitApp);
#else
	/* TODO: once nobody cares about Ubuntu 16.04, get rid of this ifdef,
	 * since Qt 5.6 is a LTS release anyway. */
	menu->addAction(tr("Show Log"), &LogWindow::instance(), SLOT(show()));
	menu->addAction(tr("&Exit %1").arg(qApp->applicationName()), this, SLOT(quitApp()));
#endif
	setContextMenu(menu);
	setStatus(Disconnected);
	show();
}

StatusIcon::~StatusIcon()
{
	contextMenu()->deleteLater();
}
