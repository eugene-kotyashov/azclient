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

#include "OpenVpnRunner.h"
#include "proxyrunner.h"
#include "LogWindow.h"
#include "platform-constants.h"
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QApplication>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QApplication>
#include <QTimer>
#include <QTemporaryFile>

OpenVpnRunner::OpenVpnRunner(QObject *parent) :
	QObject(parent),
	m_process(new QProcess(this)),
	m_managementServer(new QTcpServer(this)),
	m_managementConnection(nullptr),
	m_configFile(nullptr),
	m_hasDisconnected(false)
{
}

OpenVpnRunner::~OpenVpnRunner()
{
	QObject::disconnect(this, nullptr, nullptr, nullptr);

	if (m_managementConnection && m_managementConnection->isOpen())
		m_managementConnection->abort();
	m_process->close();
}

bool OpenVpnRunner::connect(const QString &config, const QString &username, const QString &password)
{
	emit connecting();

	m_username = username;
	m_password = password;

	m_configFile = new QTemporaryFile(this);
	m_configFile->setAutoRemove(true);
	if (!m_configFile->open()) {
		qCritical() << "Config File Write Error:" << m_configFile->errorString();
		return false;
	}
	/* Presumably QTemporaryFile already sets the umask correctly and isn't totally
	 * dumb when it comes to safe file creation, but just in case... */
	if (!m_configFile->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
		qCritical() << "Config File Permissions Error:" << m_configFile->errorString();
		return false;
	}
	if (m_configFile->write(config.toLocal8Bit()) != config.length()) {
		qCritical() << "Config File Write Error:" << "Short write.";
		return false;
	}
	m_configFile->close();

	m_process->setReadChannelMode(QProcess::MergedChannels);

	QObject::connect(m_process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, [=](int, QProcess::ExitStatus) {
		if (!m_hasDisconnected) {
			m_hasDisconnected = true;
			emit disconnected();
		}
		deleteLater();
	});

	QObject::connect(m_process, &QProcess::readyRead, this, [=]() {
		while (m_process->canReadLine())
			qInfo() << ("OpenVPN > " + m_process->readLine().trimmed()).data();
	});

	m_managementServer->setMaxPendingConnections(1);
	QObject::connect(m_managementServer, &QTcpServer::newConnection, this, [=]() {
		m_managementConnection = m_managementServer->nextPendingConnection();
		m_managementServer->close();
		managementSetup();
	});
	if (!m_managementServer->listen(QHostAddress::LocalHost)) {
		qCritical() << "Management Listener Error:" << m_managementServer->errorString();
		return false;
	}

	QStringList arguments;
#if defined(Q_OS_LINUX)
	arguments << "/usr/sbin/openvpn";
#elif defined(Q_OS_MACOS)
	arguments << QDir(qApp->applicationDirPath()).filePath("openvpn");
#endif
    QString fname("testdev1.ovpn");
    arguments << "--config" << QDir::toNativeSeparators(fname);
    qInfo() << "using config file " << QDir::toNativeSeparators(fname);
	arguments << "--verb" << "3";
	arguments << "--management" << "127.0.0.1" << QString::number(m_managementServer->serverPort());
	arguments << "--management-client";
	arguments << "--management-query-passwords";
	arguments << "--management-hold";
	arguments << "--suppress-timestamps";

#if !defined(Q_OS_WIN)
	arguments << "--log" << "/dev/stderr"; /* No stdio buffering. */
#endif

#if defined(Q_OS_LINUX)
	/* Giving script-security=2 *sucks*, since it allows code execution
	 * from the config file. But Ubuntu users will revolt without it.
	 * Besides, there are plenty of other issues with OpenVPN, for example
	 * config files containing malicious --log entries, tricky push configs,
	 * and the fact that the various mitigations for these within OpenVPN
	 * are incomplete. So... it's probably not worse than what's already
	 * possible without it, but ugh. Anyway, seeing this software could
	 * have unsigned auto-updates, like most desktop software, RCE from the
	 * the perspective of the server isn't really in this threat model. So,
	 * we grit our teeth, and enable script-security=2. */
	if (QFile::exists("/etc/openvpn/update-resolv-conf")) {
		arguments << "--script-security" << "2";
		arguments << "--up" << "/etc/openvpn/update-resolv-conf";
		arguments << "--down" << "/etc/openvpn/update-resolv-conf";
	} else if (QFile::exists("/etc/openvpn/up.sh") && QFile::exists("/etc/openvpn/down.sh")) {
		arguments << "--script-security" << "2";
		arguments << "--up" << "/etc/openvpn/up.sh";
		arguments << "--down" << "/etc/openvpn/down.sh";
	} else
		arguments << "--script-security" << "1";
#else
	arguments << "--script-security" << "1";
#endif


#if defined(Q_OS_LINUX)
	m_process->start("/usr/bin/pkexec", arguments, QIODevice::ReadOnly);
#elif defined(Q_OS_WIN)
    qInfo() << "starting openvpn from " << QDir(qApp->applicationDirPath()).filePath("openvpn.exe");
	m_process->start(QDir(qApp->applicationDirPath()).filePath("openvpn.exe"), arguments, QIODevice::ReadOnly);
#elif defined(Q_OS_MACOS)
	m_process->start(QDir(qApp->applicationDirPath()).filePath("openvpn-launcher"), arguments, QIODevice::ReadOnly);
#else
#error "Platform not supported."
#endif
	if (!m_process->waitForStarted(-1)) {
		qCritical() << "OpenVPN Process Error:" << m_process->errorString();
		m_disconnectReason = m_process->errorString();
		disconnect();
		return false;

    }


	return true;
}

void OpenVpnRunner::disconnect()
{
	if (m_managementServer->isListening())
		m_managementServer->close();
	if (m_managementConnection && m_managementConnection->isOpen())
		m_managementConnection->abort();

	if (!m_hasDisconnected) {
		m_hasDisconnected = true;
		emit disconnected();
	}

	if (m_process->state() == QProcess::NotRunning)
		deleteLater();
}

QString OpenVpnRunner::escape(const QString &in)
{
	QString out(in);
	out.replace("\\", "\\\\").replace("\"", "\\\"");
	return "\"" + out + "\"";
}

void OpenVpnRunner::managementSetup()
{
	QObject::connect(m_managementConnection, &QTcpSocket::readyRead, this, &OpenVpnRunner::managmentReadLine);
	m_managementConnection->write("state on\n");
	m_managementConnection->write("state\n");
	m_managementConnection->write("bytecount 1\n");
	m_managementConnection->write("hold release\n");
}

void OpenVpnRunner::managmentReadLine()
{
	while (m_managementConnection->canReadLine()) {
		if (m_configFile) {
			m_configFile->deleteLater();
			m_configFile = nullptr;
		}
		const QString line = m_managementConnection->readLine().trimmed();
		if (line == ">PASSWORD:Need 'Auth' username/password") {
			m_managementConnection->write(QString("username Auth %1\n").arg(escape(m_username)).toLocal8Bit());
			m_managementConnection->write(QString("password Auth %1\n").arg(escape(m_password)).toLocal8Bit());
		} else if (line.startsWith(">PASSWORD:Verification Failed:")) {
			m_disconnectReason = tr("Server connection error. Try another server.");
			disconnect();
		} else if (line.startsWith(">STATE:")) {
			if (line.contains(",CONNECTED,SUCCESS,"))
				emit connected();
			else if (line.contains(",RECONNECTING,") || line.contains(",CONNECTING,"))
				emit connecting();
		} else if (line.startsWith(">BYTECOUNT:")) {
			const QStringList colon = line.split(':');
			if (colon.length() != 2)
				continue;
			const QStringList comma = colon[1].split(',');
			if (comma.length() != 2)
				continue;
			emit transfer(comma[0].toLongLong(), comma[1].toLongLong());
		}
	}
}
