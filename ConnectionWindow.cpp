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
#include "proxyrunner.h"
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
    m_disconnect = m_connectButtons->button(QDialogButtonBox::Cancel);
    m_disconnect->setText(tr("&Disconnect"));
	m_connect = m_connectButtons->button(QDialogButtonBox::Ok);
	m_connect->setText(tr("&Connect"));
    m_disconnect->setEnabled(false);

	connect(m_username, &QLineEdit::textChanged, this, &ConnectionWindow::validateFields);
	connect(m_password, &QLineEdit::textChanged, this, &ConnectionWindow::validateFields);

	connect(m_login, &QPushButton::clicked, this, [=]() {
		setStatusText(tr("Logging in..."));
		setEnabled(false);
		m_settings.setValue("LastUsername", m_username->text());

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


	connect(m_connect, &QPushButton::clicked, this, [=]() {
		setStatusText(tr("Downloading configuration..."));
		setEnabled(false);
		m_settings.setValue("LastRegion", m_region->currentText());
		m_settings.setValue("LastProtocol", m_protocol->currentText());
        setStatusText();
        m_connect->setEnabled(false);
        startOpenVpn();
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

    m_statusIcon->setStatus(StatusIcon::Disconnected);
    showConnect();

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

    m_connectForm->labelForField(m_region)->hide();
    m_region->hide();
    m_connectForm->labelForField(m_protocol)->hide();
    m_protocol->hide();
    m_connectButtons->show();
}

void ConnectionWindow::populateRegions()
{
	regionsLoading();

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

void ConnectionWindow::startOpenVpn()
{
//	if (config.length() == 0) {
//		setStatusText();
//		setEnabled(true);
//		LogWindow::instance().show();
//		return;
//	}

	OpenVpnRunner *runner = new OpenVpnRunner(this);
    ProxyRunner* proxyRunner = new ProxyRunner(this);

	connect(runner, &OpenVpnRunner::transfer, m_statusIcon, &StatusIcon::setTransfer);

	connect(runner, &OpenVpnRunner::disconnected, this, [=]() {
		show();
		setEnabled(true);        
        proxyRunner->disconnect();
        proxyRunner->deleteLater();
		m_statusIcon->setStatus(StatusIcon::Disconnected);
		setStatusText(runner->disconnectReason());
        m_connect->setEnabled(true);
	});
	connect(runner, &OpenVpnRunner::connected, this, [=]() {
        hide();
        setEnabled(true);

        setStatusText("Connected");
//start proxy here
        if (!proxyRunner->connect(runner->internalVPNIo())) {
            qCritical() << "proxyRunner->connect failed!";
            disconnect();
            return;
        }
		m_statusIcon->setStatus(StatusIcon::Connected);
	});

	connect(runner, &OpenVpnRunner::connecting, this, [=]() {
		setStatusText(tr("Connecting..."));
		m_statusIcon->setStatus(StatusIcon::Connecting);
	});

	connect(m_statusIcon, &StatusIcon::disconnect, runner, &OpenVpnRunner::disconnect);


    connect(m_disconnect, &QPushButton::clicked, runner, &OpenVpnRunner::disconnect);

	connect(m_powerNotifier, &PowerNotifier::aboutToSleep, runner, [=]() {
		m_goingToSleepWhileConnected = true;
		runner->disconnect();
	});

    //get local ip before openvpn is connected
    proxyRunner->GetLocalIp();

    if (!runner->connect("config", "token", m_lastToken)) {
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
