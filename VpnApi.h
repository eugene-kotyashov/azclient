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

#ifndef VPNAPI_H
#define VPNAPI_H

#include <functional>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QSslConfiguration>

class VpnApi : QObject
{
	Q_OBJECT

public:
	VpnApi(QObject *parent = 0);
	void login(QObject *owner, const QString &username, const QString &password, std::function<void(const QString &, const QString &)> success);
	void getAccountInfo(QObject *owner, const QString &token, std::function<void(const QDate &)> success);
	void logout(QObject *owner, const QString &token, std::function<void(const QString &, const QString &)> success);
	void locations(QObject *owner, std::function<void(const QVariantList &, const QString &)> success);
	void ovpnConfig(QObject *owner, const QString &url, std::function<void(const QByteArray &)> success);
	void checkForUpdates(QObject *owner, std::function<void(const QString &newVersion, const QString &url)> success);
	void reinitConnection();
private:
	QNetworkRequest networkRequest(const QString &url) const;
	QNetworkAccessManager *m_network;
	QSslConfiguration m_tls;
};

#endif // VPNAPI_H
