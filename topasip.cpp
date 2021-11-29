/*
 * Copyright © 2020-2021 Yaroslav Shkliar <mail@ilit.ru>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Research Laboratory of IT
 * www.ilit.ru on e-mail: mail@ilit.ru
 * Also you сould open support domain www.cleenair.ru or write to e-mail: mail@cleenair.ru
 */

#include "topasip.h"
#include <stdlib.h>
#include <QDebug>
#include <QThread>

#ifdef TOPASIP_H
TopasIP::TopasIP(QObject *parent , QString *ip, quint16 *port, QString *_serialnum) : QObject (parent)
{
    m_sock = new QTcpSocket(this);
    connect(m_sock, SIGNAL(readyRead()), this, SLOT(readData()));
    // connect(m_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(writes()));
    connect(m_sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(displayError(QAbstractSocket::SocketError)));
    //connect(this, SIGNAL(dataIsReady(const QString)), this, SLOT(writes()) );
    changeInterface(*ip, *port);
    m_sock->setSocketOption(QAbstractSocket::LowDelayOption, 0);
    //qDebug() << "Socket " << m_sock->socketOption(QAbstractSocket::SendBufferSizeSocketOption);
    // m_sock->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 1024);
    // qDebug() << "Socket next " << m_sock->socketOption(QAbstractSocket::SendBufferSizeSocketOption);
    m_sock->setSocketOption(QAbstractSocket::TypeOfServiceOption, 64);
    measure = new  QMap<QString, float>;
    sample_t = new QMap<QString, int>;
    serialnum = * _serialnum;

    is_read = false;
    status = "";
    connected = m_sock->state();

    sample_t->insert("PM", 0 );
    sample_t->insert("PM1", 0 );
    sample_t->insert("PM2.5", 0 );
    sample_t->insert("PM10", 0 );




    qDebug() << "TOPAS measure equipment handling has been initialized.\n\r";
}

TopasIP::~TopasIP()
{
    m_sock->disconnectFromHost();
}
void TopasIP::changeInterface(const QString &address, quint16 portNbr)
{
    m_sock->connectToHost(address, portNbr);
}
void TopasIP::on_cbEnabled_clicked(bool checked)
{
    if (checked)
    {
    }
    else {
        m_sock->disconnectFromHost();
    }
    //emit tcpPortActive(checked);
}
void TopasIP::readData()
{
    QStringList list;
    int ind;
    int running;
    QRegExp xRegExp("(-?\\d+(?:[\\.,]\\d+(?:e\\d+)?)?)");
    QByteArray data = m_sock->readAll();
    qDebug() << " measure equipment data: " << data << " lenght - " << data.length() << " \n\r";

    int i = data.indexOf(':');
    int pool;

    QByteArray _crc, __crc, crc;

    __crc = crcTopas(data.mid(7, data.length()-10));
    crc = __crc.toUpper();//to form 0xA1 - not 0xa1
    _crc.append(data.mid(data.length()-3, 2).toUpper());

    if (is_read)
    {
        measure->insert("PM1", 0);
        measure->insert("PM2.5", 0);
        measure->insert("PM4", 0);
        measure->insert("PM10", 0);
        measure->insert("TMP_PM", 0);
        measure->insert("HUM_PM", 0);


        sample_t->insert("PM", 0 );
        sample_t->insert("PM1", 0 );
        sample_t->insert("PM2.5", 0 );
        sample_t->insert("PM10", 0 );
        sample_t->insert("TMP_PM", 0 );
        sample_t->insert("HUM_PM", 0 );

    }

    if ( i != -1){
        if (lastCommand != "read"){
            is_read = true;
        } else {

            if ((crc[0] == _crc[0]) && (crc[1] == _crc[1])) //CRC is coinside
            {
                char _status = data[6];

                if (_status == '1')
                {
                    //measure->insert("PM1", int(list.at(1).toFloat()*1000));
                    //  measure->insert("PM2.5", int(list.at(2).toFloat()*1000));
                    //  measure->insert("PM10", int(list.at(4).toFloat()*1000));

                    QByteArray _tsp = data.mid(11,4);
                    int __tsp = _tsp.toInt(nullptr,16);
                    QByteArray _pm10 = data.mid(15,4);
                    int __pm10 = _pm10.toInt(nullptr,16);
                    QByteArray _pm25 = data.mid(19,4);
                    int __pm25 = _pm25.toInt(nullptr,16);
                    QByteArray _pm1 = data.mid(23,4);
                    int __pm1 = _pm1.toInt(nullptr,16);

                    QByteArray _tmp = data.mid(27,4);
                    int __tmp = _tmp.toInt(nullptr,16);
                    QByteArray _hum = data.mid(31,4);
                    int __hum = _hum.toInt(nullptr,16);

                    measure->insert("PM", float((__tsp*0.01 - 2.56)/100));
                    measure->insert("PM10", float((__pm10*0.01 - 2.56)/100));
                    measure->insert("PM2.5", float((__pm25*0.01 - 2.56))/1000);
                    measure->insert("PM1", float((__pm1*0.01 - 2.56))/1000);
                    measure->insert("TMP_PM", float((__tmp*0.1 - 256)));
                    measure->insert("HUM_PM", float((__hum*0.1 - 256)));


                    sample_t->insert("PM", sample_t->value("PM") + 1);
                    sample_t->insert("PM1", sample_t->value("PM1") + 1);
                    sample_t->insert("PM2.5", sample_t->value("PM2.5") + 1);
                    sample_t->insert("PM10", sample_t->value("PM10") + 1);
                    sample_t->insert("TMP_PM", sample_t->value("TMP_PM") + 1);
                    sample_t->insert("HUM_PM", sample_t->value("HUM_PM") + 1);



                    is_read = false;
                }
            }
        }
    }
    if (!is_read){

        emit dataIsReady(&is_read, measure, sample_t);
    }
}
void TopasIP::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug()<<   ("TOPAS  equipment handling error: The host was not found. Please check the "
                      "host name and port settings.\n\r");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        qDebug()<< ("TOPAS equipment handling error: The connection was refused by the peer. "
                    "Make sure the fortune server is running, "
                    "and check that the host name and port "
                    "settings are correct.\n\r");
        break;
    default:
        qDebug()<< ("TOPAS equipment handling error: ") << (m_sock->errorString()) << "\n\r";
    }
    if (m_sock->isOpen())
        m_sock->close();
    connected = m_sock->state();
}
void TopasIP::sendData(QString command, QString serialnum)
{
    QByteArray  _ba;

    int sum = 0;
    _ba[0] = 33; // '!' - begin command
    _ba.append(serialnum.toLatin1()); //serial num with type of Turnkey analyzer prefix
    _ba.append(command.toLatin1());
    _ba.append(crcTopas(_ba.mid(1)).toUpper());
    _ba.append(13); //CR

    qint64 lnt = _ba.size();
    lnt = m_sock->write(_ba, lnt);
    lnt = m_sock->flush();

    qDebug()<< "\n\TOPAS command: " << _ba <<"\n\r" ;

}

void TopasIP::startSample()
{
    lastCommand = "start";
    sendData("4", this->serialnum);
}

void TopasIP::stopSample()
{
    lastCommand = "stop";
    sendData("6", this->serialnum);
}

void TopasIP::readSample()
{
    lastCommand = "read";
    sendData("00178", this->serialnum); //read from 0x0178 address in memory
}

QByteArray TopasIP::crcTopas(QByteArray command)
{
    QByteArray crc, _crc;
    int sum = 0;

    for (int i = 0; i < command.length();i++)
    {
        sum += int( command[i]);
    }
    _crc.setNum(sum, 16); // sum to bytes array
    crc.append(int(_crc[_crc.length()-2])); //dd two last digits in ASCII code
    crc.append(int(_crc[_crc.length()-1]));

    return crc;
}

int TopasIP::toAscii(int *_ch)
{
    if (*_ch == 0)
        return 48;
    if (*_ch == 1)
        return 49;
    if (*_ch == 2)
        return 50;
    if (*_ch == 3)
        return 51;
    if (*_ch == 4)
        return 52;
    if (*_ch == 5)
        return 53;
    if (*_ch == 6)
        return 54;
    if (*_ch == 7)
        return 55;
    if (*_ch == 8)
        return 56;
    if (*_ch == 9)
        return 57;
    if (*_ch == 10)
        return 65;
    if (*_ch == 11)
        return 66;
    if (*_ch == 12)
        return 67;
    if (*_ch == 13)
        return 68;
    if (*_ch == 14)
        return 69;
    if (*_ch == 15)
        return 70;
}
void TopasIP::writes()
{
    qDebug()<< "written " ;
}

#endif
