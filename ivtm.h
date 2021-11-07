#ifndef IVTM_H
#define IVTM_H

#include <QModbusTcpClient>
#include <QUrl>
#include <QDebug>
#include <QEventLoop>
#include <QDateTime>
#include <QThread>
#include <QCoreApplication>
#include <functional>
#include <math.h>

class ivtm : public QObject
{
    Q_OBJECT
public:
    typedef std::function<void(const QString &, const float & )> handleFunc;

    ivtm( const QUrl &server, const int &startAddress, const int &quantityToRead, const int &slaveAddress, const int &pollInterval,  QObject *parent);

    QModbusTcpClient *m_pModbusClient;
    int m_StartAddress;
    int m_QuantityToRead;
    int m_SlaveAddress;
    int m_PollInterval;


private slots:
    //void doQuery();

public:
    static const int TimeOutMs = 50000;
    bool Connect();
    void doQuery(const handleFunc &funcCallback);
};

#endif // IVTM_H
