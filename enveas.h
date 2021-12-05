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
    void dataReady(QByteArray &str);

private:

    QUdpSocket *m_sock;

    quint32 blockSize;
    QString *m_ip;
    int     *m_port;

public:
    //enum _status {Idle, Waiting, Running};
    enum _command {STANDBY, START, SYNC, READMEASURE, ABOUTSFTWR};
    enum _command last_command;

    enum _status {MEASURE, STANDBYORTEST, WARMUP, ZERO, SPAN, ZEROERF, SPANAUTO, INVALIDDATA, UNKNOWN};
    enum _status status;

    enum _type {SO2, H2S, SO2_H2S, NOx, NOx_NH3, CO, CO_CO2, PM};
    enum _type type;

    QString model;
    bool is_read;
    QMap<QString, int> *sample_t;
    QMap<QString, int> *measure;
    QAbstractSocket::SocketState connected = QAbstractSocket::UnconnectedState;
    // QDataStream *in_stream;
};


#endif // ENVEAS_H
