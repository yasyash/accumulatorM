#include "requester.h"

const QString Requester::KEY_QNETWORK_REPLY_ERROR = "QNetworkReplyError";
const QString Requester::KEY_CONTENT_NOT_FOUND = "ContentNotFoundError";

Requester::Requester(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
}

void Requester::initRequester(const QString &host, int port, QSslConfiguration *value)
{
    this->host = host;
    this->port = port;
    sslConfig = value;
    if (sslConfig != nullptr)
        pathTemplate = "https://";
    else
        pathTemplate = "http://";
}

void Requester::sendRequest(const QString &apiStr,
                            const handleFunc &funcSuccess,
                            const handleFunc &funcError,
                            Requester::Type type,
                            const QVariantMap &data)
{
    QNetworkRequest request = createRequest(apiStr);

    QNetworkReply *reply;
    switch (type) {
    case Type::POST: {
        QByteArray postDataByteArray = variantMapToJson(data);
        reply = manager->post(request, postDataByteArray);
        break;
    }
    case Type::GET: {
        reply = manager->get(request);
        break;
    }
    case Type::DELET: {
        if (data.isEmpty())
            reply = manager->deleteResource(request);
        else
            reply = sendCustomRequest(manager, request, "DELETE", data);
        break;
    }
    case Type::PATCH: {
        reply = sendCustomRequest(manager, request, "PATCH", data);
        break;
    }
    default:
        reply = nullptr;
        Q_ASSERT(false);
    }

    connect(reply, &QNetworkReply::finished, this,
            [this, funcSuccess, funcError, reply]() {
        QJsonObject obj = parseReply(reply);

        if (onFinishRequest(reply)) {
            if (funcSuccess != nullptr)
                funcSuccess(obj);
        } else {
            if (funcError != nullptr) {
                handleQtNetworkErrors(reply, obj);
                funcError(obj);
            }
        }
        reply->close();
        reply->deleteLater();
    } );

}

void Requester::sendRequest(const handleFuncExt &funcSuccess,
                            const handleFuncExt &funcError,
                            Requester::Type type,
                             QHttpMultiPart *data,
                            const QString uri,
                            const QDateTime &_date_time,
                           const  QDateTime &_last_time,
                            const int &_msg_id,
                             QSqlDatabase * m_conn,
                            const QString &idd)
{
    QNetworkRequest request = createRequest(QByteArray("multipart/form-data; boundary=---"));
    QEventLoop _event_loop;

    QNetworkReply *reply;
    switch (type) {
    case Type::POST: {
        reply = manager->post(request, data);
        break;
    } case Type::GET: {
        reply = manager->get(request);
        break;
    }
    default:
        reply = nullptr;
        Q_ASSERT(false);
    }
        connect(reply, &QNetworkReply::finished, this,
            [this, funcSuccess, funcError, reply, &_event_loop, uri, _date_time, _last_time, _msg_id, m_conn, idd]() {
        QJsonObject obj = parseReply(reply);
        if (onFinishRequest(reply)) {
            if (funcSuccess != nullptr)
                funcSuccess(obj, uri, _date_time, _last_time, _msg_id, m_conn, idd);
        } else {
            if (funcError != nullptr) {
                handleQtNetworkErrors(reply, obj);
                funcError(obj, uri, _date_time, _last_time, _msg_id, m_conn, idd);
            }
        }

        reply->close();
        reply->deleteLater();

        _event_loop.quit();
    });

    _event_loop.exec();
}

void Requester::sendMulishGetRequest(const QString &apiStr,
                                     const handleFunc &funcSuccess,
                                     const handleFunc &funcError,
                                     const finishFunc &funcFinish)
{
    QNetworkRequest request = createRequest(apiStr);
    //    QNetworkReply *reply;
    qInfo() << "GET REQUEST " << request.url().toString() << "\n";
    auto reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this,
            [this, funcSuccess, funcError, funcFinish, reply]() {
        QJsonObject obj = parseReply(reply);
        if (onFinishRequest(reply)) {
            if (funcSuccess != nullptr)
                funcSuccess(obj);
            QString nextPage = obj.value("next").toString();
            if (!nextPage.isEmpty()) {
                QStringList apiMethodWithPage = nextPage.split("api/");
                sendMulishGetRequest(apiMethodWithPage.value(1),
                                     funcSuccess,
                                     funcError,
                                     funcFinish
                                     );
            } else {
                if (funcFinish != nullptr)
                    funcFinish();
            }
        } else {
            handleQtNetworkErrors(reply, obj);
            if (funcError != nullptr)
                funcError(obj);
        }
        reply->close();
        reply->deleteLater();
    });
}


QString Requester::getToken() const
{
    return token;
}

void Requester::setToken(const QString &value)
{
    token = value;
}

QByteArray Requester::variantMapToJson(QVariantMap data)
{
    QJsonDocument postDataDoc = QJsonDocument::fromVariant(data);
    QByteArray postDataByteArray = postDataDoc.toJson();

    return postDataByteArray;
}

QNetworkRequest Requester::createRequest(const QString &apiStr)
{
    QNetworkRequest request;
    QString url = pathTemplate.arg(host).arg(port).arg(apiStr);
    request.setUrl(QUrl(url));
    request.setRawHeader("Content-Type","application/json");
    if(!token.isEmpty())
        request.setRawHeader("Authorization",QString("token %1").arg(token).toUtf8());
    if (sslConfig != nullptr)
        request.setSslConfiguration(*sslConfig);

    return request;
}

QNetworkRequest Requester::createRequest(const QByteArray &_type)
{
    QNetworkRequest request;
    request.setUrl(QUrl(host));
    request.setRawHeader("Content-Type", _type);

    QSslConfiguration sslConfiguration = QSslConfiguration(QSslConfiguration::defaultConfiguration());
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);

    request.setSslConfiguration(sslConfiguration);
    //request.setRawHeader(QByteArray("Authorization"), QByteArray("Basic "));

    return request;
}

QNetworkReply* Requester::sendCustomRequest(QNetworkAccessManager* manager,
                                            QNetworkRequest &request,
                                            const QString &type,
                                            const QVariantMap &data)
{
    request.setRawHeader("HTTP", type.toUtf8());
    QByteArray postDataByteArray = variantMapToJson(data);
    QBuffer *buff = new QBuffer;
    buff->setData(postDataByteArray);
    buff->open(QIODevice::ReadOnly);
    QNetworkReply* reply =  manager->sendCustomRequest(request, type.toUtf8(), buff);
    buff->setParent(reply);
    return reply;
}

QJsonObject Requester::parseReply(QNetworkReply *reply)
{
    QJsonObject jsonObj;
    QJsonDocument jsonDoc;
    QJsonParseError parseError;
    auto replyText = reply->readAll();
    jsonDoc = QJsonDocument::fromJson(replyText, &parseError);
    if(parseError.error != QJsonParseError::NoError){
        qDebug() << replyText;
        qWarning() << "Json parse error: " << parseError.errorString();
    }else{
        if(jsonDoc.isObject())
            jsonObj  = jsonDoc.object();
        else if (jsonDoc.isArray())
            jsonObj["non_field_errors"] = jsonDoc.array();
    }
    return jsonObj;
}

bool Requester::onFinishRequest(QNetworkReply *reply)
{
    auto replyError = reply->error();
    if (replyError == QNetworkReply::NoError ) {
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if ((code >=200) && (code < 300)) {
            return true;
        }
    }
    return false;
}

void Requester::handleQtNetworkErrors(QNetworkReply *reply, QJsonObject &obj)
{
    auto replyError = reply->error();
    if (!(replyError == QNetworkReply::NoError ||
          replyError == QNetworkReply::ContentNotFoundError ||
          replyError == QNetworkReply::ContentAccessDenied ||
          replyError == QNetworkReply::ProtocolInvalidOperationError
          ) ) {
        qDebug() << reply->error();
        obj[KEY_QNETWORK_REPLY_ERROR] = reply->errorString();
    } else if (replyError == QNetworkReply::ContentNotFoundError)
        obj[KEY_CONTENT_NOT_FOUND] = reply->errorString();
}
