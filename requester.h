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


#ifndef REQUESTER_H
#define REQUESTER_H


#include <QObject>
#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <functional>
#include <QEventLoop>
#include <QSqlDatabase>
#include <QHttpPart>

class Requester : public QObject
{
    Q_OBJECT
public:
    typedef std::function<void(const QJsonObject &)> handleFunc;
    typedef std::function<void(const QJsonObject &, const QString &, const QDateTime &, const QDateTime &, const int &,  QSqlDatabase *, const QString &)> handleFuncExt;
    typedef std::function<void()> finishFunc;
    typedef  void (_handleFuncExt)(const QJsonObject &, const QString &, const QDateTime &, const QDateTime &, const int &,  QSqlDatabase *, const QString &);

    static const QString KEY_QNETWORK_REPLY_ERROR;
    static const QString KEY_CONTENT_NOT_FOUND;

    enum class Type {
        POST,
        GET,
        PATCH,
        DELET
    };

    bool syncronous_free = true;

    explicit Requester(QObject *parent = 0);

    void initRequester(const QString& host, int port, QSslConfiguration *value);

    void sendRequest(const QString &apiStr,
                     const handleFunc &funcSuccess,
                     const handleFunc &funcError,
                     Type type = Type::GET,
                     const QVariantMap &data = QVariantMap());

    void sendRequest(const handleFuncExt &funcSuccess,
                     const handleFuncExt &funcError,
                     Requester::Type type,
                     QHttpMultiPart *data,
                     const QString uri,
                     const QDateTime &_date_time,
                    const  QDateTime &_last_time,
                    const int &_msg_id,
                      QSqlDatabase * m_conn,
                    const QString &idd);
    void sendRequest(const handleFuncExt &funcSuccess,
                     const handleFuncExt &funcError,
                     Requester::Type type,
                     QByteArray &data,
                     const QString uri,
                     const QDateTime &_date_time,
                    const  QDateTime &_last_time,
                    const int &_msg_id,
                      QSqlDatabase * m_conn,
                    const QString &idd);
    void sendMulishGetRequest(const QString &apiStr,
                              const handleFunc &funcSuccess,
                              const handleFunc &funcError,
                              const finishFunc &funcFinish);

    QString getToken() const;
    void setToken(const QString &value);
    QJsonObject parseReply(QNetworkReply *reply);

private:
    static const QString httpTemplate;
    static const QString httpsTemplate;

    QString host;
    int port;
    QString token;
    QSslConfiguration *sslConfig;

    QString pathTemplate;

    QByteArray variantMapToJson(QVariantMap data);

    QNetworkRequest createRequest(const QString &apiStr);
    QNetworkRequest createRequest(const QByteArray &_type);

    QNetworkReply *sendCustomRequest(QNetworkAccessManager *manager,
                                     QNetworkRequest &request,
                                     const QString &type,
                                     const QVariantMap &data);


    bool onFinishRequest(QNetworkReply *reply);

    void handleQtNetworkErrors(QNetworkReply *reply, QJsonObject &obj);
    QNetworkAccessManager *manager;

signals:
    void networkError();


public slots:
};


#endif // REQUESTER_H
