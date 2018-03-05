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

#ifndef STATUSICON_H
#define STATUSICON_H

#include <QSystemTrayIcon>

class QAction;

class StatusIcon : public QSystemTrayIcon
{
	Q_OBJECT

public:
	enum Status { Logout, Disconnected, Connecting, Connected };
	StatusIcon(QObject *owner = 0);
	~StatusIcon();
	void setStatus(Status status);
	void setTransfer(quint64 down, quint64 up);

signals:
	void disconnect();

public Q_SLOTS:
	void quitApp();

private:
	QString unit(double bytes) const;
	bool maybeShowParent() const;
	QAction *m_disconnectMenu;
	QAction *m_statusMenu;
	QAction *m_transferMenu;
	Status m_status;
};

#endif // STATUSICON_H
