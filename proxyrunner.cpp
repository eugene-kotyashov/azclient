#include "proxyrunner.h"
#include <QDebug>
#include <QProcess>
#include <QApplication>
#include <QDir>


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

bool ProxyRunner::connect()
{

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
