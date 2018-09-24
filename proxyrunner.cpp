#include "proxyrunner.h"
#include <QDebug>
#include <QProcess>
#include <QApplication>
#include <QDir>
#include <QTemporaryFile>
#include <QString>
#include <QTextStream>


ProxyRunner::ProxyRunner(QObject *parent):
    QObject(parent),
    m_process(new QProcess(this)),
    m_hasDisconnected(false)
{
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
    configStream << "log" << endl
                 << "internal " << internalIp << endl
                 <<  "maxconn 20000" << endl
                  << "auth iponly" << endl
                  << "nserver 178.168.253.2" << endl
                  << "nserver 178.168.253.1" << endl
                  << "nscache 262144" << endl
                  << "allow * * *" << endl
                  << "external 178.168.203.170"<< endl
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
    m_process->start(QDir(qApp->applicationDirPath()).filePath("3proxy.exe"), arguments, QIODevice::ReadOnly);
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
