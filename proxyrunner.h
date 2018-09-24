#ifndef PROXYRUNNER_H
#define PROXYRUNNER_H


#include <QObject>

class QProcess;

class ProxyRunner : public QObject
{
    Q_OBJECT
private:
    QProcess *m_process;
    bool m_hasDisconnected;
public:
    ProxyRunner(QObject* parent);
    ~ProxyRunner();
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
