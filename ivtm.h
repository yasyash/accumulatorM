/*
 * Copyright © 2021 Yaroslav Shkliar <mail@ilit.ru>
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
 * License along with this library; if not, write to the owner
 * www.cleenair.ru on e-mail: mail@ilit.ru
 */


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
