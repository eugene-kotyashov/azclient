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

#include "VpnApi.h"
#include "customization.h"
#include "platform-constants.h"
#include <QSslCipher>
#include <QSslEllipticCurve>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QApplication>
#include <QSysInfo>
#include <QDebug>
#include <QUrlQuery>

VpnApi::VpnApi(QObject *parent) :
	QObject(parent),
	m_network(new QNetworkAccessManager(this)),
	m_tls(QSslConfiguration::defaultConfiguration())
{
	m_tls.setProtocol(QSsl::TlsV1_2);
	m_tls.setCiphers({ QSslCipher("ECDHE-RSA-AES256-GCM-SHA384"), QSslCipher("ECDHE-ECDSA-AES256-GCM-SHA384")});
	QList<QSslCertificate> certs(QSslCertificate::fromPath(":ca.crt"));
	if (!certs.isEmpty())
		m_tls.setCaCertificates(certs);
}

static QNetworkReply *logReplyErrors(QObject *owner, QNetworkReply *reply)
{
	QObject::connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError error)>(&QNetworkReply::error), owner, [=](QNetworkReply::NetworkError) {
		qCritical() << "Network Error:" << reply->errorString();
		reply->deleteLater();
	});

	QObject::connect(reply, &QNetworkReply::sslErrors, owner, [=](const QList<QSslError> &errors) {
		for (auto &error : errors)
			qCritical() << "SSL Error:" << error.errorString();
	});
	return reply;
}

void VpnApi::login(QObject *owner, const QString &username, const QString &password, std::function<void(const QString &, const QString &)> success)
{
	QUrlQuery postData;
	postData.addQueryItem("username", username);
	postData.addQueryItem("password", password);
	QNetworkRequest request = networkRequest(API_BASE "v1/token/generate");
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	QNetworkReply *reply = logReplyErrors(owner, m_network->post(request, postData.toString(QUrl::FullyEncoded).toUtf8()));

	QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
		QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
		reply->deleteLater();
		success(object["status"].toString(), object["message"].toString());
	});
}

void VpnApi::getAccountInfo(QObject *owner, const QString &token, std::function<void(const QDate &)> success)
{
	QNetworkRequest request = networkRequest(API_BASE "v1/account/information");
	request.setRawHeader(QByteArray("Authorization"), QByteArray(QString("Bearer " + token).toUtf8()));
	QNetworkReply *reply = logReplyErrors(owner, m_network->get(request));

	QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
		QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
		reply->deleteLater();
		QDate expirationDate = STR2DATE(object["expires_at"].toString());
		success(expirationDate);
	});
}

void VpnApi::logout(QObject *owner, const QString &token, std::function<void(const QString &, const QString &)> success)
{
	QUrlQuery postData;
	postData.addQueryItem("token", token);
	QNetworkRequest request = networkRequest(API_BASE "v1/token/delete");
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	request.setRawHeader(QByteArray("Authorization"), QByteArray(QString("Bearer " + token).toUtf8()));
	QNetworkReply *reply = logReplyErrors(owner, m_network->post(request, postData.toString(QUrl::FullyEncoded).toUtf8()));

	QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
		QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
		reply->deleteLater();
		success(object["status"].toString(), object["message"].toString());
	});
}

void VpnApi::locations(QObject *owner, std::function<void(const QVariantList &, const QString &)> success)
{
	QNetworkRequest request = networkRequest(API_BASE "v1/locations");
	QNetworkReply *reply = logReplyErrors(owner, m_network->get(request));

	QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
		QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
		reply->deleteLater();
		success(object["locations"].toArray().toVariantList(), object["min-client-version"].toString());
	});
}

void VpnApi::ovpnConfig(QObject *owner, const QString &url, std::function<void(const QByteArray &)> success)
{
	QNetworkRequest request = networkRequest(url);
	QNetworkReply *reply = logReplyErrors(owner, m_network->get(request));

	QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
		const QByteArray bytes = reply->readAll();
		reply->deleteLater();
		success(bytes);
	});
}

void VpnApi::checkForUpdates(QObject *owner, std::function<void (const QString &, const QString &)> success)
{
	QNetworkRequest request = networkRequest(API_BASE "v1/client");
	QNetworkReply *reply = logReplyErrors(owner, m_network->get(request));

	QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
		QJsonObject values = QJsonDocument::fromJson(reply->readAll()).object()[PLATFORM_UPDATE].toObject();
		reply->deleteLater();
		success(values["version"].toString(), values["url"].toString());
	});
}

void VpnApi::reinitConnection()
{
	QNetworkAccessManager *old = m_network;
	m_network = new QNetworkAccessManager(this);
	old->deleteLater();
}

QNetworkRequest VpnApi::networkRequest(const QString &url) const
{
	QNetworkRequest request(url);
	request.setSslConfiguration(m_tls);
	request.setRawHeader("User-Agent", USERAGENT);
	return request;
}
