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
#include "VpnApi.h"
#include "OpenVpnRunner.h"
#include "StatusIcon.h"
#include <QApplication>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QSvgWidget>
#include <QTimer>
#include <QFile>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#if QT_VERSION >= 0x050600
#include <QVersionNumber>
#elif defined(Q_OS_LINUX)
#include "dist/linux/supporting-old-ubuntu-is-a-horrible-experience/qversionnumber_p.h"
#endif

ConnectionWindow::ConnectionWindow(QWidget *parent)
	: QDialog(parent),
	  m_statusIcon(new StatusIcon(this)),
	  m_api(new VpnApi(this)),
	  m_updateGuard(false),
	  m_goingToSleepWhileConnected(false),
	  m_loggedIn(false),
	  m_powerNotifier(new PowerNotifier(this))
{
	setWindowTitle(tr("%1 - Connect").arg(qApp->applicationName()));
	setWindowIcon(QIcon(":icons/app.svg"));
#ifdef Q_OS_MACOS
	setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
#endif

	m_layout = new QVBoxLayout;
	m_layout->setSizeConstraint(QLayout::SetFixedSize);

	m_status = new QLabel;
	m_status->setAlignment(Qt::AlignCenter);
	m_status->setObjectName("status");
	setStatusText();
	m_layout->addWidget(m_status);

	m_layout->addWidget(new QSvgWidget(":logo.svg"));

	m_username = new QLineEdit;
	m_username->setText(m_settings.value("LastUsername").toString());
	m_password = new QLineEdit;
	m_password->setEchoMode(QLineEdit::Password);
	m_remember = new QCheckBox;
	m_remember->setText(tr("Save credentials"));

	m_loginForm = new QFormLayout;
	m_loginForm->addRow(tr("Username:"), m_username);
	m_loginForm->addRow(tr("Password:"), m_password);
	m_loginForm->addRow("", m_remember);

	m_loginButtons = new QDialogButtonBox(QDialogButtonBox::Ok);
	m_login = m_loginButtons->button(QDialogButtonBox::Ok);
	m_login->setText(tr("&Login"));

	m_region = new QComboBox;
	m_region->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	m_protocol = new QComboBox;
	m_protocol->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	m_connectForm = new QFormLayout;
	m_connectForm->addRow(tr("Region:"), m_region);
	m_connectForm->addRow(tr("Protocol:"), m_protocol);

	m_connectButtons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
	QPushButton *logout = m_connectButtons->button(QDialogButtonBox::Cancel);
	logout->setText(tr("&Sign out"));
	m_connect = m_connectButtons->button(QDialogButtonBox::Ok);
	m_connect->setText(tr("&Connect"));

	connect(m_username, &QLineEdit::textChanged, this, &ConnectionWindow::validateFields);
	connect(m_password, &QLineEdit::textChanged, this, &ConnectionWindow::validateFields);

	connect(m_login, &QPushButton::clicked, this, [=]() {
		setStatusText(tr("Logging in..."));
		setEnabled(false);
		m_settings.setValue("LastUsername", m_username->text());
		m_api->login(this, m_username->text(), m_password->text(), [=](const QString &status, const QString &message) {
			setEnabled(true);
			if (status == "error")
				setStatusText(tr("Invalid username or password"));
			else if (status == "success") {
				setStatusText();
				QString token = message;

				m_settings.setValue("RememberCredentials", m_remember->isChecked());
				m_settings.setValue("LastToken", token);

				m_password->clear();
				m_remember->setChecked(false);

				m_lastToken = token;
				m_loggedIn = true;

				checkAccount();
				m_statusIcon->setStatus(StatusIcon::Disconnected);

				showConnect();
			}
		});
	});

	connect(m_region, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=](int) {
		const QVariantList &protocols = m_region->currentData().toList();
		if (!protocols.count())
			return;
		m_protocol->clear();
		for (const QVariant &item : protocols) {
			const QVariantMap &protocol = item.toMap();
			const QString &name = protocol["name"].toString();
			const QString &path = protocol["url"].toString();
			if (name.isEmpty() || path.isEmpty())
				continue;
			m_protocol->addItem(name, path);
		}
		int saved = m_protocol->findText(m_settings.value("LastProtocol").toString());
		if (saved >= 0)
			m_protocol->setCurrentIndex(saved);
		validateFields();
	});
	connect(m_region, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ConnectionWindow::validateFields);

	connect(logout, &QPushButton::clicked, this, [=]() {
		setStatusText(tr("Signing out..."));
		setEnabled(false);
		m_api->logout(this, m_lastToken, [=](const QString &status, const QString &) {
			setEnabled(true);
			if (status == "error")
				setStatusText(tr("Invalid token"));
			else if (status == "success")
				setStatusText();

			showLogin();
		});
	});

	connect(m_connect, &QPushButton::clicked, this, [=]() {
		setStatusText(tr("Downloading configuration..."));
		setEnabled(false);
		m_settings.setValue("LastRegion", m_region->currentText());
		m_settings.setValue("LastProtocol", m_protocol->currentText());
		m_api->ovpnConfig(this, m_protocol->currentData().toString(), [=](const QByteArray &data) {
			setStatusText();
			startOpenVpn(data);
		});
	});

	m_lastToken = m_settings.value("LastToken").toString();
	m_loggedIn = !m_lastToken.isEmpty();
	setLayout(m_layout);
	if (m_loggedIn) {
		m_layout->addLayout(m_connectForm);
		m_layout->addWidget(m_connectButtons);

		checkAccount();
		m_statusIcon->setStatus(StatusIcon::Disconnected);
	} else {
		m_layout->addLayout(m_loginForm);
		m_layout->addWidget(m_loginButtons);
	}

	connect(m_powerNotifier, &PowerNotifier::resumed, this, [=]() {
		if (!m_goingToSleepWhileConnected)
			return;
		m_goingToSleepWhileConnected = false;
		if (m_lastUsername.isEmpty() || m_lastPassword.isEmpty())
			return;
		m_username->setText(m_lastUsername);
		m_password->setText(m_lastPassword);
		m_connect->click();
	});

	QTimer::singleShot(1000 * 10, this, &ConnectionWindow::checkForUpdates);
}

void ConnectionWindow::checkAccount()
{
	setStatusText(tr("Checking account..."));
	setEnabled(false);
	m_api->getAccountInfo(this, m_lastToken, [=](const QDate &expirationDate) {
		setStatusText();
		setEnabled(true);
		if (expirationDate > QDate::currentDate())
			populateRegions();
		else
			showLogin();
	});
}

void ConnectionWindow::showLogin()
{
	m_settings.setValue("RememberCredentials", false);
	m_settings.remove("LastToken");
	m_lastToken.clear();
	m_loggedIn = false;
	m_statusIcon->setStatus(StatusIcon::Logout);

	m_layout->removeItem(m_connectForm);
	m_layout->removeWidget(m_connectButtons);

	m_connectForm->labelForField(m_region)->hide();
	m_region->hide();
	m_connectForm->labelForField(m_protocol)->hide();
	m_protocol->hide();
	m_connectButtons->hide();

	m_layout->addLayout(m_loginForm);
	m_layout->addWidget(m_loginButtons);

	m_loginForm->labelForField(m_username)->show();
	m_username->show();
	m_loginForm->labelForField(m_password)->show();
	m_password->show();
	m_remember->show();
	m_loginButtons->show();
}

void ConnectionWindow::showConnect()
{
	m_layout->removeItem(m_loginForm);
	m_layout->removeWidget(m_loginButtons);

	m_loginForm->labelForField(m_username)->hide();
	m_username->hide();
	m_loginForm->labelForField(m_password)->hide();
	m_password->hide();
	m_remember->hide();
	m_loginButtons->hide();

	m_layout->addLayout(m_connectForm);
	m_layout->addWidget(m_connectButtons);

	m_connectForm->labelForField(m_region)->show();
	m_region->show();
	m_connectForm->labelForField(m_protocol)->show();
	m_protocol->show();
	m_connectButtons->show();
}

void ConnectionWindow::populateRegions()
{
	regionsLoading();
	m_api->locations(this, [=](const QVariantList &locations, const QString &minVersion) {
		if (!minVersion.isEmpty() && QVersionNumber::fromString(minVersion) > QVersionNumber::fromString(qApp->applicationVersion())) {
			setStatusText(tr("Client is out of date. Please update."));
			setEnabled(false);
			m_region->clear();
			m_protocol->clear();
			m_settings.remove("IgnoredVersion");
			checkForUpdates();
			qCritical() << "Client is out of date. Immediate update required.";
			return;
		}
		bool foundOne = false;
		m_region->clear();
		for (const QVariant &item : locations) {
			const QVariantMap &location = item.toMap();
			const QString &name = location["iso"].toString() + " - " + location["city"].toString();
			if (name.isEmpty())
				continue;
			foundOne = true;
			m_region->addItem(name, location["endpoints"].toMap()["openvpn"]);
		}
		if (foundOne) {
			int saved = m_region->findText(m_settings.value("LastRegion").toString());
			if (saved >= 0)
				m_region->setCurrentIndex(saved);
			m_region->setEnabled(true);
			m_protocol->setEnabled(true);
			setStyleSheet(styleSheet());
			validateFields();
		}
		else {
			qCritical() << "Response Error:" << "No regions in locations endpoint.";
			regionsLoading();
			QTimer::singleShot(5000, this, &ConnectionWindow::populateRegions);
		}
	});
}

void ConnectionWindow::regionsLoading()
{
	m_region->clear();
	m_protocol->clear();
	m_connect->setEnabled(false);
	m_protocol->setEnabled(false);
	m_protocol->addItem(tr("Loading..."));
	m_region->setEnabled(false);
	m_region->addItem(tr("Loading..."));
	setStyleSheet(styleSheet());
}

void ConnectionWindow::validateFields()
{
	if (m_loggedIn)
		m_connect->setEnabled(!m_protocol->currentData().toString().isEmpty());
	else
		m_login->setEnabled(!m_username->text().isEmpty() && !m_password->text().isEmpty());
}

void ConnectionWindow::startOpenVpn(const QByteArray &config)
{
	if (config.length() == 0) {
		setStatusText();
		setEnabled(true);
		LogWindow::instance().show();
		return;
	}

	OpenVpnRunner *runner = new OpenVpnRunner(this);
	connect(runner, &OpenVpnRunner::transfer, m_statusIcon, &StatusIcon::setTransfer);
	connect(runner, &OpenVpnRunner::disconnected, this, [=]() {
		m_api->reinitConnection();
		show();
		setEnabled(true);
		m_statusIcon->setStatus(StatusIcon::Disconnected);
		setStatusText(runner->disconnectReason());
	});
	connect(runner, &OpenVpnRunner::connected, this, [=]() {
		m_api->reinitConnection();
		hide();
		m_password->clear();
		setStatusText();
		m_statusIcon->setStatus(StatusIcon::Connected);
	});
	connect(runner, &OpenVpnRunner::connecting, this, [=]() {
		m_api->reinitConnection();
		setStatusText(tr("Connecting..."));
		m_statusIcon->setStatus(StatusIcon::Connecting);
	});
	connect(m_statusIcon, &StatusIcon::disconnect, runner, &OpenVpnRunner::disconnect);
	connect(m_powerNotifier, &PowerNotifier::aboutToSleep, runner, [=]() {
		m_goingToSleepWhileConnected = true;
		runner->disconnect();
	});

	if (!runner->connect(config, "token", m_lastToken)) {
		show();
		setEnabled(true);
		setStatusText();
		LogWindow::instance().show();
	}
}

void ConnectionWindow::checkForUpdates()
{
	if (m_updateGuard)
		return;
	m_api->checkForUpdates(this, [=](const QString &newVersion, const QString &url) {
		if (m_updateGuard)
			return;
		if (newVersion.isEmpty() || url.isEmpty()) {
			QTimer::singleShot(1000 * 60 * 30, this, &ConnectionWindow::checkForUpdates);
			return;
		}
		if (QVersionNumber::fromString(newVersion) <= QVersionNumber::fromString(qApp->applicationVersion()) || newVersion == m_settings.value("IgnoredVersion").toString()) {
			QTimer::singleShot(1000 * 60 * 10, this, &ConnectionWindow::checkForUpdates);
			return;
		}
		QMessageBox question(QMessageBox::Question, tr("Update Available"), tr("An update to %1 is available.\n\nWould you like to download it now?").arg(qApp->applicationName()), QMessageBox::Yes | QMessageBox::Ignore, this);
		question.setButtonText(QMessageBox::Yes, tr("&Download Now"));
		question.setButtonText(QMessageBox::Ignore, tr("Ignore this Update"));
		question.setDefaultButton(QMessageBox::Yes);
		m_updateGuard = true;
		int response = question.exec();
		m_updateGuard = false;
		if (response == QMessageBox::Ignore)
			m_settings.setValue("IgnoredVersion", newVersion);
		else if (response == QMessageBox::Yes) {
			QDesktopServices::openUrl(url);
			QTimer::singleShot(1000 * 60 * 60, this, &ConnectionWindow::checkForUpdates);
			return;
		}
		QTimer::singleShot(1000 * 60 * 10, this, &ConnectionWindow::checkForUpdates);
	});
}

void ConnectionWindow::closeEvent(QCloseEvent *event)
{
	event->ignore();
	hide();
}

void ConnectionWindow::setStatusText(const QString &text)
{
	if (text.isEmpty()) {
		m_status->clear();
		m_status->setVisible(false);
	} else {
		m_status->setText(text);
		m_status->setVisible(true);
	}
}
