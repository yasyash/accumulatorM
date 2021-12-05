#include "enveas.h"

enveas::enveas(QObject *parent , QString *ip, quint16 *port) : QObject (parent)

{

    m_sock = new QUdpSocket(this);

    connect(m_sock, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(m_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(writes()));

    connect(m_sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(displayError(QAbstractSocket::SocketError)));
    //connect(this, SIGNAL(dataReady(QByteArray&)), this, SLOT(setData(QByteArray&)) );

    changeInterface(*ip, *port);
    m_sock->setSocketOption(QAbstractSocket::LowDelayOption, 0);

    measure = new  QMap<QString, int>;
    measure->insert("SO2", 0);
    measure->insert("H2S", 0);
    measure->insert("NO", 0);
    measure->insert("NO2", 0);
    measure->insert("NOx", 0);
    measure->insert("NH3", 0);
    measure->insert("PM1", 0);
    measure->insert("PM2.5", 0);
    measure->insert("PM4", 0);
    measure->insert("PM10", 0);
    measure->insert("PM", 0);

    is_read = false;
    status = UNKNOWN;
    connected = m_sock->state();

    qDebug() << "The Envea measure equipment handling has been initialized.";

}

enveas::enveas(QObject *parent, QString *ip, quint16 *port, QString *name, int __type) : QObject (parent)
{
    m_sock = new QUdpSocket(this);

    connect(m_sock, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(m_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(writes()));

    connect(m_sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(displayError(QAbstractSocket::SocketError)));
    //connect(this, SIGNAL(dataReady(QByteArray&)), this, SLOT(setData(QByteArray&)) );

    changeInterface(*ip, *port);
    m_sock->setSocketOption(QAbstractSocket::LowDelayOption, 0);

    measure = new  QMap<QString, int>;
    sample_t = new QMap<QString, int>;

    is_read = false;
    status = UNKNOWN;
    connected = m_sock->state();

    model = *name;
    type = _type(__type);

    switch (type) {
    case SO2_H2S:

        measure ->insert("SO2", 0);
        measure->insert("H2S", 0);

        sample_t->insert("SO2", 0);
        sample_t->insert("H2S", 0);

        break;

    case SO2:
        measure ->insert("SO2", 0);
        sample_t->insert("SO2", 0);

        break;
    case H2S:
        measure->insert("H2S", 0);

        sample_t->insert("H2S", 0);

        break;
    case NOx_NH3:

        measure ->insert("NO", 0);
        measure->insert("NO2", 0);
        measure ->insert("NOx", 0);
        measure->insert("NH3", 0);

        sample_t->insert("NO", 0);
        sample_t->insert("NO2", 0);
        sample_t->insert("NOx", 0);
        sample_t->insert("NH3", 0);

        break;

    case NOx:

        measure ->insert("NO", 0);
        measure->insert("NO2", 0);
        measure ->insert("NOx", 0);

        sample_t->insert("NO", 0);
        sample_t->insert("NO2", 0);
        sample_t->insert("NOx", 0);
        break;

    case CO:

        measure ->insert("CO", 0);

        sample_t->insert("CO", 0);

        break;

    case PM:

        measure ->insert("PM", 0);
        measure ->insert("PM1", 0);
        measure->insert("PM2.5", 0);
        measure ->insert("PM4", 0);
        measure->insert("PM10", 0);

        sample_t ->insert("PM", 0);
        sample_t ->insert("PM1", 0);
        sample_t->insert("PM2.5", 0);
        sample_t ->insert("PM4", 0);
        sample_t->insert("PM10", 0);

        break;
    default:
        break;
    }
    qDebug() << "The Envea " << name <<" measure equipment handling has been initialized.";

}

enveas::~enveas()
{
    //sendData("MSTOP");
    //sendData("MSTOP");

    m_sock->disconnectFromHost();
    if (m_sock->isOpen())
        m_sock->close();
    connected = m_sock->state();
}



void enveas::changeInterface(const QString &address, quint16 portNbr)
{
    m_sock->connectToHost(address, portNbr);
}





void enveas::on_cbEnabled_clicked(bool checked)
{
    if (checked)
    {
    }
    else {
        m_sock->disconnectFromHost();
    }
    //emit tcpPortActive(checked);
}


void enveas::readData()
{
    QStringList list;
    int ind;
    int running;
    QRegExp xRegExp("(-?\\d+(?:[\\.,]\\d+(?:e\\d+)?)?)");

    QByteArray data = m_sock->readAll();
    switch (last_command) {
    case READMEASURE : {
        list = QString(QString(data).remove(QRegExp("[\r\n{\r\n}]"))).split(QRegExp(" "));
        ind = xRegExp.indexIn(list.at(0));

        if ( QString(data).indexOf("M") != -1)
            status = MEASURE;

        if ( QString(data).indexOf("P") != -1)
            status = WARMUP;

        if ( QString(data).indexOf("T") != -1)
            status = STANDBYORTEST;

        if ( QString(data).indexOf("R") != -1)
            status = ZEROERF;

        if ( QString(data).indexOf("Z") != -1)
            status = ZERO;

        if ( QString(data).indexOf("S") != -1)
            status = SPAN;

        if ( QString(data).indexOf("C") != -1)
            status = SPANAUTO;

        if ( QString(data).indexOf("M") != -1)
            status = INVALIDDATA;


        if (status == MEASURE)
        {
            switch (type) {
            case SO2_H2S:
                measure->insert("SO2", int(list.at(1).toFloat()*1000) + measure->value("SO2"));
                sample_t->insert("SO2", sample_t->value("SO2")+1);

                measure->insert("H2S", int(list.at(2).toFloat()*1000) + measure->value("H2S"));
                sample_t->insert("H2S", sample_t->value("H2S")+1);

                break;

            case SO2:
                measure->insert("SO2", int(list.at(1).toFloat()*1000) + measure->value("SO2"));
                sample_t->insert("SO2", sample_t->value("SO2")+1);

                break;
            case H2S:
                measure->insert("H2S", int(list.at(1).toFloat()*1000) + measure->value("H2S"));
                sample_t->insert("H2S", sample_t->value("H2S")+1);
                break;
            case NOx_NH3:

                measure->insert("NO", int(list.at(1).toFloat()*1000) + measure->value("NO"));
                sample_t->insert("NO", sample_t->value("NO")+1);

                measure->insert("NO2", int(list.at(2).toFloat()*1000) + measure->value("NO2"));
                sample_t->insert("NO2", sample_t->value("NO2")+1);

                measure->insert("NOx", int(list.at(3).toFloat()*1000) + measure->value("NOx"));
                sample_t->insert("NOx", sample_t->value("NOx")+1);

                measure->insert("NH3", int(list.at(4).toFloat()*1000) + measure->value("NH3"));
                sample_t->insert("NH3", sample_t->value("NH3")+1);
                break;

            case NOx:

                measure->insert("NO", int(list.at(1).toFloat()*1000) + measure->value("NO"));
                sample_t->insert("NO", sample_t->value("NO")+1);

                measure->insert("NO2", int(list.at(2).toFloat()*1000) + measure->value("NO2"));
                sample_t->insert("NO2", sample_t->value("NO2")+1);

                measure->insert("NOx", int(list.at(3).toFloat()*1000) + measure->value("NOx"));
                sample_t->insert("NOx", sample_t->value("NOx")+1);

                break;

            case CO:

                measure->insert("CO", int(list.at(1).toFloat()*1000) + measure->value("CO"));
                sample_t->insert("CO", sample_t->value("CO")+1);

                break;

            case PM:

                measure->insert("PM1", int(list.at(1).toFloat()*1000) + measure->value("PM1"));
                sample_t->insert("PM1", sample_t->value("PM1")+1);

                measure->insert("PM2.5", int(list.at(2).toFloat()*1000) + measure->value("PM2.5"));
                sample_t->insert("PM2.5", sample_t->value("PM2.5")+1);

                measure->insert("PM10", int(list.at(3).toFloat()*1000) + measure->value("PM10"));
                sample_t->insert("PM10", sample_t->value("PM10")+1);

                measure->insert("PM", int(list.at(4).toFloat()*1000) + measure->value("PM"));
                sample_t->insert("PM", sample_t->value("PM")+1);

                break;

            default:
                break;
            }

        }
    }
        break;


    default: break;
    }

    qDebug() << "ENVEA measure data: " << data;//<< " lenght - " << data.length() << " \n\r";

    this->is_read = true;

    emit (dataReady(data));


    blockSize = 0;

}

void enveas::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug()<<   ("!!! ENVEA measure equipment handling error: The host was not found. Please check the "
                      "host name and port settings.\n\r");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        qDebug()<< ("!!! ENVEA measure equipment handling error: The connection was refused by the peer. "
                    "Make sure the fortune server is running, "
                    "and check that the host name and port "
                    "settings are correct.\n\r");
        break;
    default:
        qDebug()<< ("!!! ENVEA measure equipment handling error: ") << (m_sock->errorString())<<"\n\r";
    }

    if (m_sock->isOpen())
        m_sock->close();
    connected = m_sock->state();

}

void enveas::readGases(int qw)
{
    if (qw <3)
    {
        QByteArray ba;
        ba.resize(6);
        ba[0] = 65;//AF22
        ba[1] = 70;
        ba[2] = 50;
        ba[3] = 50;
        ba[4] = 49; //instantaneous measure with floating point - 16 command
        ba[5] = 54;
        sendData(1, &ba);
    }
    else {
        QByteArray ba;
        ba.resize(qw);
        ba[0] = 50; //primary gas response
        ba[1] = 51; //secondary gas response
        ba[2] = 52; //third gas response
        ba[3] = char(0xb7); //fourth gas response

        sendData(1, &ba);
    }

    last_command = READMEASURE;

}

void enveas::readStatus()
{
    QByteArray ba;
    ba.resize(1);
    ba[0] = 110; //state of equipment

    sendData(1, &ba);
}

void enveas::readAbout()
{
    QByteArray ba;
    ba.resize(6);
    ba[0] = 65;//AF22
    ba[1] = 70;
    ba[2] = 50;
    ba[3] = 50;
    ba[4] = 49; //soft identification
    ba[5] = 50;
    sendData(1, &ba);

    last_command = ABOUTSFTWR;
}

void enveas::standby()
{
    QByteArray ba;
    ba.resize(6);
    ba[0] = 65;//AF22
    ba[1] = 70;
    ba[2] = 50;
    ba[3] = 50;
    ba[4] = 49; //standby mode 14 command
    ba[5] = 52;
    sendData(1, &ba);

    last_command = STANDBY;
}

void enveas::start()
{
    QByteArray ba;
    ba.resize(6);
    ba[0] = 65;//AF22
    ba[1] = 70;
    ba[2] = 50;
    ba[3] = 50;
    ba[4] = 48; //measure 09 command
    ba[5] = 57;
    sendData(1, &ba);

    last_command = START;
}
void enveas::sync()
{
    QDateTime _now = QDateTime::currentDateTime();

    QByteArray ba;
    ba.resize(6);
    ba[0] = 65;//AF22
    ba[1] = 70;
    ba[2] = 50;
    ba[3] = 50;
    ba[4] = 48; //sync 01 command
    ba[5] = 49;
    ba[6] = char(toAscii( _now.toString("yyyy").mid(2,1).toInt()));
    ba[7] = char(toAscii( _now.toString("yyyy").mid(3,1).toInt()));
    ba[8] = char(toAscii( _now.toString("MM").mid(0,1).toInt()));
    ba[9] = char(toAscii( _now.toString("MM").mid(1,1).toInt()));
    ba[10] = char(toAscii( _now.toString("dd").mid(0,1).toInt()));
    ba[11] = char(toAscii( _now.toString("dd").mid(1,1).toInt()));
    ba[12] = char(toAscii( _now.toString("HH").mid(0,1).toInt()));
    ba[13] = char(toAscii( _now.toString("HH").mid(1,1).toInt()));
    ba[14] = char(toAscii( _now.toString("mm").mid(0,1).toInt()));
    ba[15] = char(toAscii( _now.toString("mm").mid(1,1).toInt()));
    ba[16] = char(toAscii( _now.toString("ss").mid(0,1).toInt()));
    ba[17] = char(toAscii( _now.toString("ss").mid(1,1).toInt()));

    sendData(1, &ba);

    last_command = SYNC;
}

void enveas::sendData(int command, QByteArray *data)
{
    int checksum =  0; //id = 0
    QString _msg="";

    if (data->length() == 2){
        checksum = checksum ^  data->length() ^ data->at(0) ^ data->at(1);
    } else
    {
        //checksum = checksum ^ command ^ data->length();
        for (int i =0; i < data->length(); i++)
        {
            checksum = checksum ^ (data->at(i) & 0xFF);
        }
        // checksum = checksum  ^ data->at(0) ^ data->at(1) ^ 0xb7;

    }
    if (data->length() == 2){
        _msg = QString(0x02) +QString(data->at(0)) + QString(data->at(1)) + QString(checksum) + QLatin1Char(0x04);
    } else
    {
        _msg = QString(0x02) ;
        for (int i =0; i < data->length(); i++)
        {
            _msg = _msg + QString(data->at(i) & 0xFF);

        }
        //QByteArray _crc = crc();
        _msg = _msg + QString(toAscii((checksum >> 4) & 0xF )) +  QString(toAscii((checksum) & 0xF) )+ QLatin1Char(0x03);

        //_msg = QString(0x02) + QLatin1Char(0) + QLatin1Char(command) + QLatin1Char(0x03) + QString(data->length()) + QString(data->at(0)) + QString(data->at(1))+ QLatin1Char(0xb7)+ QString(checksum) + QLatin1Char(0x04);
    }
    //strcat(str,  QLatin1Char(51));
    //strcat(str,  "\r");
    qint64 lnt = _msg.size();//qint64(strlen(str));

    lnt = m_sock->write(_msg.toLatin1(), lnt);
    lnt = m_sock->flush();

    qDebug()<< "\n\rEnvea command: " << _msg <<"\n\r" ;
}

QByteArray enveas::crc(QByteArray command)
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

int enveas::toAscii(int _ch)
{
    if (_ch == 0)
        return 48;
    if (_ch == 1)
        return 49;
    if (_ch == 2)
        return 50;
    if (_ch == 3)
        return 51;
    if (_ch == 4)
        return 52;
    if (_ch == 5)
        return 53;
    if (_ch == 6)
        return 54;
    if (_ch == 7)
        return 55;
    if (_ch == 8)
        return 56;
    if (_ch == 9)
        return 57;
    if (_ch == 10)
        return 65;
    if (_ch == 11)
        return 66;
    if (_ch == 12)
        return 67;
    if (_ch == 13)
        return 68;
    if (_ch == 14)
        return 69;
    if (_ch == 15)
        return 70;
}


void enveas::writes()
{
    //qDebug()<< "written " ;
}
