/*
 * Copyright © 2018-2021 Yaroslav Shkliar <mail@ilit.ru>
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


#ifndef PROCESSOR_H
#define PROCESSOR_H


#include <QTimer>
#include <QMap>
#include <QSqlDatabase>
#include <QMutex>
#include <QUuid>
#include <QVector>
#include <QMetaEnum>
#include <QThreadPool>

#include "modbus.h"
#include "modbusip.h"
#include "modbus-private.h"
#include "ups_status.h"
#include "tcpsock.h"
#include "dusttcpsock.h"
#include "meteotcpsock.h"
#include "serinus.h"
#include "grimm.h"
#include "liga.h"
#include "topasip.h"
#include "gammaet.h"
#include "ivtm.h"
#include "qcollectorc.h"
#include "enveas.h"

#define ENUM_TO_STR(ENUM) QString(#ENUM)


class processor : public QObject
{
    Q_OBJECT


public:

    processor(QObject *_parent = 0,  QStringList *cmdline = 0 );
    ~processor();

    static void stBusMonitorAddItem( modbus_t * modbus,
                                     uint8_t isOut, uint8_t slave, uint8_t func, uint16_t addr,
                                     uint16_t nb, uint16_t expectedCRC, uint16_t actualCRC );
    static void stBusMonitorRawData( modbus_t * modbus, uint8_t * data,
                                     uint8_t dataLen, uint8_t addNewline );
    void busMonitorRawData( uint8_t * data, uint8_t dataLen, bool addNewline );
    void cover_modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest, int *result);

    void releaseModbus(void);
    virtual modbus_t*  modbus() { return m_serialModbus; }
public:

    enum _status {MEASURING, DOWN, FAILURE, TEMP_NOT_READY, SENS_CHNG, SAMPLE_FILL, ELECTRONIC_ZERO_ADJUST, INSTRUMENT_WARM_UP, UNKNOWN, ABSENT,
                  STANDBYORTEST, ZERO, SPAN, ZEROREF, SPANAUTO, INVALIDDATA};
    Q_ENUM(_status)

    static QMap<QString, int>   * ms_data; //assosiative array of polling data
    static QMap<QString, int>   * ms_measure; //assosiative array of measurement quantities
    static QMap<QString, int>   * ms_range; //assosiative array of measurement equipments range

signals:
    void AsciiPortActive(bool active);
    void finished ();						// signal finished

public slots:
    void terminate ()			//termination
    {
        emit finished ();
    }

    void fillSensorData( bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample,  QString *__status); //for one status to all components
    void fillSensorData( bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample,  QMap<QString, _status> *_status); //sensor equipment type or name
    void fillSensorData( bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample); //polymorphic method for instrument without status
    void fillSensorData( bool *_is_read, QMap<QString, float> *_measure); //polymorphic method for slow measuring
    static void static_fillSensorData(  bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample);
    void fillSensorDataModbus( bool *_is_read, QMap<QString, int> *_measure, QMap<QString, int> *_sample, QMap<QString, _status> *_status);
    void fillSensorDataModbus( bool *_is_read, QMap<QString, int> *_measure, QMap<QString, int> *_sample);//polymorphic method for instrument without status


private slots:
    void sendModbusRequest( void ); //update data view
    void onSendButtonPress( void );
    void pollForDataOnBus( void );
    void onRtuPortActive(bool active);
    void onAsciiPortActive(bool active);
    void onTcpPortActive(bool active);
    void resetStatus( void );
    void setStatusError(const QString &msg);
    void renovateSlaveID( void );
    void startTransactTimer(QSqlDatabase *conn); //start by signal dbForm
    void transactionDB   (void);   //transaction timer event
    void readSocketStatus(void);

private:

    modbus_t * m_serialModbus  = nullptr;

    enveas *m_envea_so2 = nullptr;
    QString m_envea_ip_so2;
    quint16 m_envea_port_so2;
    QString m_envea_name_so2;

    enveas *m_envea_h2s = nullptr;
    QString m_envea_ip_h2s;
    quint16 m_envea_port_h2s;
    QString m_envea_name_h2s;


    enveas *m_envea_so2_h2s = nullptr;
    QString m_envea_ip_so2_h2s;
    quint16 m_envea_port_so2_h2s;
    QString m_envea_name_so2_h2s;

    enveas *m_envea_nox = nullptr;
    QString m_envea_ip_nox;
    quint16 m_envea_port_nox;
    QString m_envea_name_nox;

    enveas *m_envea_nox_nh3 = nullptr;
    QString m_envea_ip_nox_nh3;
    quint16 m_envea_port_nox_nh3;
    QString m_envea_name_nox_nh3;

    ivtm *m_ivtm = nullptr;
    QString m_ivtm_ip;
    quint16 m_ivtm_port;
    quint16 m_ivtm_address;
    quint16 m_ivtm_length;

    ModbusIP *m_modbusip = nullptr;
    QString m_modbus_ip;
    quint16 m_modbus_port;

    ModbusIP *m_modbusip232 = nullptr;
    quint16 m_modbus_port232;

    modbus_t * m_modbus = NULL;
    QTimer * m_pollTimer;
    QTimer * m_statusTimer;
    QTimer * m_renovateTimer; //timer for  renovate of all modbus slave id polling
    QTimer * m_transactTimer; //timer for trunsaction
    QMap<QString, int>   * m_data; //assosiative array of polling data
    QMap<QString, int>   * m_measure; //assosiative array of measurement quantities
    QMap<QString, int>   * m_range; //assosiative array of measurement equipments range
    QMap<QString, _status> * m_status; //assosiative array of measurements status

    QMap<QString, QUuid>   * m_meteo_uuid; //assosiative array of meteo sensors uuid in main table
    QMap<QString, QString>   * m_meteo_types; //and classmeasure (key) correspond typemeasure
    QMap<QString, QUuid>   * m_uuid; //assosiative array of sensors uuid
    //QList<int> *m_pool;
    QMap<int, int> *m_pool; //assosiative array of polling slave address - number of registers

    bool m_tcpActive;
    bool m_poll;
    uint8_t * q_poll;
    QSqlDatabase * m_conn;
    QUuid * m_uuidStation;
    //QMutex * m_mutex;
    QVector<bool> *slaveID;

    int funcModbus;
    int addrModbus;
    int numCoils;
    int *m_transactTime;
    bool verbose; //verbose mode flag
    bool ggo = false; //GGO transmition mode
    bool ggo20 = false;


    ups_status *m_ups = nullptr;   //member for UPS status
    QString m_ups_ip;
    quint16 m_ups_port;
    QString m_ups_username;

    TcpSock    *m_fire = nullptr; //member for Fire alarm status
    QString m_alarmip;
    quint16 m_alarmport;

    DustTcpSock    *m_dust = nullptr; //member for Dust Measure
    QString m_dust_ip;
    quint16 m_dust_port;

    MeteoTcpSock *m_meteo = nullptr; //member for Meteostation
    QString m_meteo_ip;
    quint16 m_meteo_port;
    QString m_meteo_model;

    Serinus *m_serinus = nullptr; //member for Serinus51
    QString m_serinus_ip;
    quint16 m_serinus_port;

    Serinus *m_serinus55 = nullptr; //member for Serinus55
    QString m_serinus_ip55;
    quint16 m_serinus_port55;

    Serinus *m_serinus50 = nullptr; //member for Serinus50
    QString m_serinus_ip50;
    quint16 m_serinus_port50;

    Serinus *m_serinus44 = nullptr; //member for Serinus44
    QString m_serinus_ip44;
    quint16 m_serinus_port44;

    Serinus *m_serinus30 = nullptr; //member for Serinus50
    QString m_serinus_ip30;
    quint16 m_serinus_port30;

    Grimm *m_grimm = nullptr; //member for Grimm
    QString m_grimm_ip;
    quint16 m_grimmport;

    Liga     *m_liga = nullptr; //member for ACA-Liga
    QString m_liga_ip;
    quint16 m_liga_port;

    TopasIP *m_topasip = nullptr;
    QString m_topas_ip;
    quint16 m_topas_port;
    QString m_topas_num;
    bool m_topas_sensor = false;


    GammaET *m_gammaet = nullptr;
    QString m_gammaet_ip;
    quint16 m_gammaet_port;
    QMap<int, int> *m_gammapool; //assosiative array of polling slave address - number of registers GammaET dev.

    QString m_ssh_ip;
    quint16 m_ssh_port;
    QString m_ssh_user;
    QString m_ssh_pwd;
    QString m_ssh_command;

    QThreadPool *m_threadPool;
    //QThreadPool *m_threadPoolSlowProcess;

private:
    void squeezeAlarmMsg();
    void initStatus();


};

#endif // PROCESSOR_H
