#ifndef OPENVPNRUNNER_H
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

#define OPENVPNRUNNER_H

#include <QObject>

class QProcess;
class QTcpServer;
class QTcpSocket;
class QTemporaryFile;
class ProxyRunner;

class OpenVpnRunner : public QObject
{
	Q_OBJECT

public:
    OpenVpnRunner(QObject *parent = nullptr);
	~OpenVpnRunner();
	const QString &disconnectReason() const { return m_disconnectReason; }

public slots:
	bool connect(const QString &config, const QString &username, const QString &password);
	void disconnect();

signals:
	void connected();
	void connecting();
	void disconnected();
	void processExit();
	void transfer(quint64 down, quint64 up);

private:
	QString escape(const QString &in);
	void managementSetup();
	QString m_disconnectReason;
	QString m_username;
	QString m_password;
	QProcess *m_process;
	QTcpServer *m_managementServer;
	QTcpSocket *m_managementConnection;
	QTemporaryFile *m_configFile;
	bool m_hasDisconnected;


private slots:
	void managmentReadLine();
};

#endif // OPENVPNRUNNER_H
