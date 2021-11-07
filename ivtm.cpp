#include "ivtm.h"

#ifdef IVTM_H

ivtm::ivtm(const QUrl &server, const int &startAddress, const int &quantityToRead, const int &slaveAddress, const int &pollInterval, QObject *parent) : QObject(parent)
{
    m_StartAddress = startAddress;
    m_QuantityToRead = quantityToRead;
    m_SlaveAddress = slaveAddress;
    m_PollInterval = pollInterval;

    m_pModbusClient = new QModbusTcpClient(this);

    m_pModbusClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, server.port() );
    m_pModbusClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, server.host());
    m_pModbusClient->setTimeout(TimeOutMs);
    m_pModbusClient->setNumberOfRetries(5);

    qDebug()<<"IVTM has been initialized...\n\r\n\r";

};

bool ivtm::Connect()
{
    if( m_pModbusClient->state() == QModbusDevice::ConnectedState )
    {
        return true;
    }

    m_pModbusClient->connectDevice();

    QDateTime endTs = QDateTime::currentDateTime().addMSecs(TimeOutMs);
    while(QDateTime::currentDateTime() < endTs)
    {
        //  QThread::usleep(1000);
        QCoreApplication::processEvents();
        if (m_pModbusClient->state() == QModbusDevice::ConnectedState)
            break;
    }
    return m_pModbusClient->state() == QModbusDevice::ConnectedState;
}

void ivtm::doQuery(const handleFunc &funcCallback)
{
    if( !Connect() )
    {
        qDebug()<< "IVTM error connection timeout detected...";
        return;
    }
    QEventLoop _event_loop;

    if( QModbusReply *pModbusReply = m_pModbusClient->sendReadRequest(
                QModbusDataUnit( QModbusDataUnit::InputRegisters,
                                 m_StartAddress,
                                 quint16 (m_QuantityToRead)),
                m_SlaveAddress ) )
    {
        connect(pModbusReply, &QModbusReply::finished, this, [this, &funcCallback, pModbusReply, &_event_loop](){


            if(pModbusReply->isFinished() &&  pModbusReply->error() == QModbusDevice::NoError)
            {
                QVector<float> *_arr = new QVector<float>;

                const QModbusDataUnit unit = pModbusReply->result();
                float _i = 0.0f;

                char sign;
                int expo;
                uint32_t mantissa;
                uint32_t ieee;
                float result;
                int i, j, start;
                for (j=0; j<2; j++)
                {
                    sign = unit.value(1) >> 15;
                    ieee = uint32_t( unit.value(2*j + 1) << 16 | unit.value(2*j) );
                    expo = (ieee & 0x7F800000) >> 23;
                    expo -= 127;//1103051114

                    mantissa = (ieee & 0x7FFFFF);
                    float dec = 1.0f;

                    for(i=0; i<=22; i++) {
                        if(((mantissa >> (22 - i)) & 1) != 0) {
                            dec += float(pow(2, -i-1));
                        }
                    }

                    result = float( pow(2,expo));

                    if(sign)
                        result = -1*result * dec;
                    else
                        result = result * dec;

                    if (j) {
                        qDebug()<<"IVTM measure humidity: " << result <<" %.\n\r\n\r";
                        funcCallback(QString("hum_in"), result);
                    } else {
                        qDebug()<<"IVTM measure temperature: " << result <<" degrees C.\n\r\n\r";
                        funcCallback(QString("temp_in"), result);
                    }

                }
            }
            else
            {
                qDebug()<< "IVTM error detected:  " <<pModbusReply->errorString() <<"\n\r\n\r";

            }

            pModbusReply->deleteLater();

            _event_loop.quit();

        });
    } else {

        _event_loop.quit();

    }

    _event_loop.exec();
}

#endif
