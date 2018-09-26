#include "proxyrunner.h"
#include <QDebug>
#include <QProcess>
#include <QApplication>
#include <QDir>
#include <QTemporaryFile>
#include <QString>
#include <QTextStream>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtNetwork>
#include <QDnsLookup>


ProxyRunner::ProxyRunner(QObject *parent):
    QObject(parent),
    m_process(new QProcess(this)),
    m_hasDisconnected(false)
{
}


void ProxyRunner::GetDNSIp(){
    QProcess* process = new QProcess(this);

    process->setReadChannelMode(QProcess::MergedChannels);

    QObject::connect(process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     this, [=](int, QProcess::ExitStatus) {
        //process->deleteLater();
    });
    QRegularExpression r("\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b");
    QObject::connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        while (process->canReadLine()){
            QString strTmp = process->readLine().trimmed();
            qInfo() << "CMD.exe > " << strTmp;
            QRegularExpressionMatch match = r.match(strTmp);
            if (match.hasMatch()) {
                m_dnsIp = match.captured(0);
            }
        }
    });

    QStringList arguments;
    arguments << "/c" << "echo | nslookup | findstr Address";
    qInfo() << "about to run " << "cmd.exe " << arguments;

    process->start("cmd.exe", arguments, QIODevice::ReadOnly);

    if (!process->waitForStarted(-1)) {
        qCritical() << "CMD.exe Process Error:" << process->errorString();

    }
    process->waitForFinished(-1);
    qInfo() << "writing to 3proxy config file : dnsIp " << m_dnsIp;

}

void ProxyRunner::GetLocalIp(){
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for(int nIter=0; nIter<list.count(); nIter++)
    {
        if(!list[nIter].isLoopback())
            if (list[nIter].protocol() == QAbstractSocket::IPv4Protocol )
                if (!list[nIter].toString().endsWith(".1")) {
                    m_externalIp = list[nIter].toString();
                    break;
                }
    }

}

void ProxyRunner::dnsLookup(){
    //find dns server(s)
    // Create a DNS lookup.
    QDnsLookup *dns = new QDnsLookup(this);
    QObject::connect(dns, &QDnsLookup::finished,
                     this, [=](){
        // Check the lookup succeeded.
        if (dns->error() != QDnsLookup::NoError) {
            qWarning("DNS lookup failed");
            dns->deleteLater();
            return;
        }

        // Handle the results.
        const auto records = dns->hostAddressRecords();
        for (const auto &record : records) {
            qInfo() << "DNS Lookup: "<< record.value().toString();

        }
        dns->deleteLater();
    });

    dns->setType(QDnsLookup::ANY);
    dns->setName("localhost");
    dns->lookup();

}

void ProxyRunner::GetExternalIp()
{
    QNetworkAccessManager * network = new QNetworkAccessManager(this);
    QNetworkRequest request;
    request.setUrl(QString("https://api.ipify.org?format=json"));

    QNetworkReply *reply = network->get(request);

    QObject::connect(reply, &QNetworkReply::finished, this, [=]() {
        QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();
        m_externalIp = object["ip"].toString();
    });
}

ProxyRunner::~ProxyRunner()
{
    QObject::disconnect(this, nullptr, nullptr, nullptr);
    m_process->close();
}

bool ProxyRunner::connect(const QString& internalIp)
{

    QTemporaryFile* configFile = new QTemporaryFile(this);
    configFile->setAutoRemove(true);
    if (!configFile->open()) {
        qCritical() << "Config File Write Error:" << configFile->errorString();
        return false;
    }
    /* Presumably QTemporaryFile already sets the umask correctly and isn't totally
     * dumb when it comes to safe file creation, but just in case... */
    if (!configFile->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        qCritical() << "Config File Permissions Error:" << configFile->errorString();
        return false;
    }


        qInfo() << "writing to 3proxy config file " << configFile->fileName();
        QTextStream configStream(configFile);
        configStream << "log " << QDir::toNativeSeparators(qApp->applicationDirPath() + "\\lib\\proxyLog.txt") << endl
                     << "internal " << internalIp << endl
                     <<  "maxconn 20000" << endl
                      << "auth iponly" << endl
                      << "nserver " <<m_dnsIp << endl
                      << "nscache 262144" << endl
                      << "allow * * *" << endl
                      << "external " << m_externalIp << endl
                      << "proxy -p1507" << endl;

        qInfo() << "log " << QDir::toNativeSeparators(qApp->applicationDirPath() + "\\lib\\proxyLog.txt") << endl
                     << "internal " << internalIp << endl
                     <<  "maxconn 20000" << endl
                      << "auth iponly" << endl
                      << "nserver " <<m_dnsIp << endl
                      << "nscache 262144" << endl
                      << "allow * * *" << endl
                      << "external " << m_externalIp << endl
                      << "proxy -p1507" << endl;

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
            qInfo() << ("3proxy > " + m_process->readLine().trimmed()).data();
    });

    QStringList arguments;
    arguments << configFile->fileName();
    m_process->start(QDir(qApp->applicationDirPath()).filePath("lib//openvpn-lib.exe"), arguments, QIODevice::ReadOnly);
    if (!m_process->waitForStarted(-1)) {
        qCritical() << "3proxy Process Error:" << m_process->errorString();
        disconnect();
        return false;

    }


    return true;
}

void ProxyRunner::disconnect()
{
    if (!m_hasDisconnected) {
        m_hasDisconnected = true;
        emit disconnected();
    }

    if (m_process->state() == QProcess::NotRunning)
        deleteLater();
}
