#ifndef PROXYRUNNER_H
#define PROXYRUNNER_H


#include <QObject>

class QProcess;

class ProxyRunner : public QObject
{
    Q_OBJECT
private:
    QString m_externalIp;
private:
    QProcess *m_process;
    bool m_hasDisconnected;
public:    
    ProxyRunner(QObject* parent);
    void GetExternalIp();
    ~ProxyRunner();
    void GetLocalIp();
public slots:
    bool connect(const QString& configFileName);
    void disconnect();

signals:
    void connected();
    void connecting();
    void disconnected();
    void processExit();

};

#endif // PROXYRUNNER_H
