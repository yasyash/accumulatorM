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


#ifndef QCOLLECTORC_H
#define QCOLLECTORC_H

#include <QHash>
#include <QHttpMultiPart>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlField>
#include <QDateTime>
#include <QRunnable>

#include "requester.h"

const QHash<QString, QString> aspiap_dir = {
    {"Пыль общая", "P001"},
    {"PM1", "PM1"},
    {"PM2.5", "P301"},
    {"PM10", "P201"},
    {"NO2", "P005"},
    {"NO", "P006"},
    {"NH3", "P019"},
    {"бензол", "P028"},
    {"HF", "P030"},
    {"HCl", "P015"},
    {"м,п-ксилол", "м,п-ксилол"},
    {"о-ксилол", "о-ксилол"},
    {"O3", "P007"},
    {"H2S", "P008"},
    {"SO2", "P002"},
    {"стирол", "P068"},
    {"толуол", "P071"},
    {"CO", "P004"},
    {"фенол", "P010"},
    {"CH2O", "P022"},
    {"хлорбензол", "P077"},
    {"этилбензол", "P083"},
    {"Атм. давление", "Атм. давление"}
};

class qcollectorc : public QObject, public QRunnable
{

public:
    QSqlDatabase * m_conn;
    QString m_idd;
    QDateTime m_from_t;
    QDateTime m_to_t;
    QDateTime m_last_t;
    QString m_uri;
    int m_code;
    QString m_token;
    QString m_locality;
    double m_msg_id;
    int m_msg_id_out;
    bool frame20 = false;
    bool verbose = false;

public:
    explicit qcollectorc(QSqlDatabase * _conn, QString &idd, QDateTime &_from_t, QDateTime &_to_t, QDateTime &_last_t, QString &uri, int &code, QString &token, QString &locality, double &msg_id, int &msg_id_out, QObject *parent, bool _verbose = false, bool _frame20 = false);
    ~qcollectorc();
   // void funcSuccess(const QJsonObject &_resp,const QString &uri,const QDateTime &_date_time,const QDateTime &_last_time,const int &_msg_id, QSqlDatabase * m_conn,const QString &idd);
    //void funcError(const QJsonObject &_resp,const QString &uri,const QDateTime &_date_time,const QDateTime &_last_time,const int &_msg_id, QSqlDatabase * m_conn,const QString &idd);
private:
    void run();
};

#endif // QCOLLECTORC_H
