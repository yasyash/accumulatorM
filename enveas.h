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
 * License along with this library; if not, write to the Research Laboratory of IT
 * www.ilit.ru on e-mail: mail@ilit.ru
 * Also you сould open support domain www.cleenair.ru or write to e-mail: mail@cleenair.ru
 */

#ifndef ENVEAS_H
#define ENVEAS_H

#include <QUdpSocket>
#include<QDateTime>

class enveas: public QObject
{
    Q_OBJECT

public:
    enveas(QObject *parent, QString *ip, quint16 *port);
    enveas(QObject *parent, QString *ip, quint16 *port, QString *name, int __type);

    virtual ~enveas();


    void readGases(int qw);
    void readStatus();
    void sendData(int command, QByteArray *data);
    void readAbout();
    void standby();
    void sync();
    void start();
    void zero_ref();

    QByteArray crc(QByteArray command);
    int toAscii(int _ch);





protected:
    void changeInterface(const QString& address, quint16 portNbr);

private slots:
    void on_cbEnabled_clicked(bool checked);
    void readData();
    void displayError(QAbstractSocket::SocketError socketError);
    void writes();

signals:
    void portActive(bool val);
    void connectionError(const QString &msg);
    void dataIsReady( bool *is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample, QString *__status  );

private:

    QUdpSocket *m_sock;

    quint32 blockSize;
    QString *m_ip;
    int     *m_port;

public:
    //enum _status {Idle, Waiting, Running};
    enum _command {STANDBY, START, SYNC, READMEASURE, ABOUTSFTWR, ZEROREFERENCE};
    Q_ENUM(_command)
    enum _command last_command;

    enum _status {MEASURING, STANDBYORTEST, INSTRUMENT_WARM_UP, ZERO, SPAN, ZEROREF, SPANAUTO, INVALIDDATA, UNKNOWN};
    Q_ENUM(_status)
    enum _status status;

    enum _type {SO2, H2S, SO2_H2S, NOx, NOx_NH3, CO, CO_CO2, PM};
    Q_ENUM(_type)
    enum _type type;

    QString model;
    bool is_read;
    QMap<QString, int> *sample_t;
    QMap<QString, float> *measure;
    QAbstractSocket::SocketState connected = QAbstractSocket::UnconnectedState;
    bool verbose = false;

    // QDataStream *in_stream;
};


#endif // ENVEAS_H
