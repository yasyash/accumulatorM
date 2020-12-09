/*
 * Copyright © 2018-2020 Yaroslav Shkliar <mail@ilit.ru>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Research Laboratory of IT
 * www.ilit.ru on e-mail: mail@ilit.ru
 */

#ifndef TOPASIP_H
#define TOPASIP_H


#include <QUdpSocket>
#include <QTcpSocket>


class TopasIP : public QObject
{
    Q_OBJECT

public:
    TopasIP(QObject *parent, QString *ip, quint16 *port, QString *serialnum);

    virtual ~TopasIP();
    int toAscii(int *_ch);
    void sendData(QString command, QString serialnum);
    void setTime ( QString serialnum, QString msg);
    void startSample();
    void stopSample();
    void readSample();

    QByteArray crcTopas(QByteArray command);

signals:
    void tcpPortActive(bool val);
    void connectionError(const QString &msg);
    void dataIsReady( bool *is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample  );


protected:
    void changeInterface(const QString& address, quint16 portNbr);

private slots:
    void on_cbEnabled_clicked(bool checked);
    void readData();
    void displayError(QAbstractSocket::SocketError socketError);
    void writes();


private:
    QTcpSocket *m_sock;
    quint32 blockSize;
    QString *m_ip;
    int *m_port;

public:
    //enum _status {Idle, Waiting, Running};
    int m_type = 485;
    enum _command {RDMN, MSTATUS, MSTART, MSTOP, RMMEAS};
    QString model;
    QString serialnum;
    QString status;
    bool is_read = false;
    QMap<QString, int> *sample_t;
    QMap<QString, float> *measure;
    enum _command last_command;
    QString lastCommand ="";

    QAbstractSocket::SocketState connected = QAbstractSocket::UnconnectedState;
    // QDataStream *in_stream;
};

#endif // TOPASIP_H
