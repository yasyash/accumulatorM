/*
 * Copyright © 2021-2022 Yaroslav Shkliar <mail@ilit.ru>
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
    {"Пыль общая", "TSP"},
    {"PM1", "PM1"},
    {"PM2.5", "P301"},
    {"PM10", "P201"},
    {"NO2", "P005"},
    {"NO", "P006"},
    {"NH3", "P019"},
    {"бензол", "P028"},
    {"HF", "P030"},
    {"HCl", "P015"},
    {"м,п-ксилол", "P447"},
    {"м-ксилол", "P247"},
    {"п-ксилол", "P347"},
    {"о-ксилол", "P147"},
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

const QHash<QString, int> chem_precision = {
    {"Пыль общая", 3},
    {"PM1", 3},
    {"PM2.5", 3},
    {"PM10", 3},
    {"NO2", 3},
    {"NO", 3},
    {"NH3", 2},
    {"бензол", 3},
    {"HF", 3},
    {"HCl", 2},
    {"м,п-ксилол", 3},
    {"м-ксилол", 3},
    {"п-ксилол", 3},
    {"о-ксилол", 3},
    {"O3", 3},
    {"H2S", 3},
    {"SO2", 3},
    {"стирол", 3},
    {"толуол", 3},
    {"CO", 1},
    {"фенол", 3},
    {"CH2O", 3},
    {"хлорбензол", 3},
    {"этилбензол", 3},
    {"Атм. давление", 1},
    {"Влажность внутр.", 1},
    {"Влажность внеш.", 1},
    {"Скорость ветра", 1},
    {"Направление ветра", 1},
    {"Темп. внутренняя", 1},
    {"Темп. внешняя", 1},
    {"Интенс. осадков", 1}
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
