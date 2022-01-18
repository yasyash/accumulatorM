/*
 * Copyright © 2018-2022 Yaroslav Shkliar <mail@ilit.ru>
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


// -slaveid 1 3 11 12 15 -port /dev/ttyr00 -baud 9600 -data 8 -stop 1 -parity none -db weather -user weather -pwd 31415 -dustip 192.168.1.3 -dustport 3602 -alarmip 192.168.1.110 -alarmport 5555 -upsip 192.168.1.120 -upsport 3493 -upsuser liebert -meteoip 192.168.1.200 -meteoport 22222 -polltime 10 -verbose

#include <libssh/libssh.h>

#include "processor.h"
#include "app.h"
#include <QDebug>
#include <QSqlQuery>
#include <QRegExp>
#include <QDateTime>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlField>
#include <QtMath>
#include <functional>

#include <errno.h>

extern _App	*globalApp;

QMap<QString, int> *processor::ms_range = new QMap<QString, int>;
QMap<QString, int> *processor::ms_data = new QMap<QString, int>;
QMap<QString, int> *processor::ms_measure = new QMap<QString, int>;

void push_data (const QString &_key, const float &_val)
{
    
}

//processor::ms_range->insert("Пыль общая", 1000);
//ms_range->insert("PM1", 1000);
//ms_range->insert("PM2.5", 1000);
//ms_range->insert("PM4", 1000);
//ms_range->insert("PM10", 1000);

void free_channel(ssh_channel channel) {
    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
}

void free_session(ssh_session session) {
    ssh_disconnect(session);
    ssh_free(session);
}

void error(ssh_session session) {
    qDebug( "Error: %s\n", ssh_get_error(session));
    free_session(session);
    exit(-1);
}

processor::processor(QObject *_parent,    QStringList *cmdline) : QObject (_parent),
    m_modbus( NULL ),
    m_tcpActive(false),
    m_poll(false), // not working initial state
    funcModbus(0x03),
    addrModbus(0),
    numCoils(7), //maximum 7 registers of modbus equipments of OPTEC
    verbose(false)
  
{
    
    QStringList cmdline_args =  *cmdline;
    m_status =  new QMap<QString, _status>;
    
    m_threadPool = new QThreadPool();
    m_threadPool->setMaxThreadCount(1);
    
    //m_threadPoolSlowProcess = new QThreadPool();

    int _verbose = cmdline_args.indexOf("-verbose");
    if (_verbose > 0)
    {
        verbose = true;
        qDebug () << "\n\r*************************************************\n\r            Fetcher version " <<  APP_VERSION << "\n\r*************************************************\n\r";
        
    }

    int _ggo = cmdline_args.indexOf("-ggo");
    if (_ggo > 0)
    {
        ggo = true;
        qDebug () << "Transmit to GGO is working!\n\r";

    }


    int _transactTime = cmdline_args.indexOf("-transtime");
    if (_transactTime > 0)
    {
        m_transactTime =  new int;
        *m_transactTime = cmdline_args.value(cmdline_args.indexOf("-transtime") +1).toInt();
        qDebug () << "Transaction every" <<  *m_transactTime << "seconds...\n\r";
        
    }
    
    int _ver = cmdline_args.indexOf("-version");
    if (_ver > 0)
    {
        qDebug () << "\n\r*************************************************\n\r            Fetcher version " <<  APP_VERSION << "\n\r*************************************************\n\r";
    }
    //DB connection init
    QString db ="";
    if (cmdline_args.indexOf("-db") > -1)
        db = cmdline_args.value(cmdline_args.indexOf("-db") +1);
    if (db == "")
    {
        // releaseModbus();

        qDebug ( "Fatal error: wrong data of the database parameter\n\r");
        exit(-1);

    }

    QString user = cmdline_args.value(cmdline_args.indexOf("-user") +1);
    if (user == "")
    {
        // releaseModbus();

        qDebug ( "Fatal error: wrong data of the user parameter\n\r");
        exit(-1);

    }

    QString pw = cmdline_args.value(cmdline_args.indexOf("-pwd") +1);
    if (pw == "")
    {
        // releaseModbus();

        qDebug ( "Fatal error: wrong data of the password parameter\n\r");
        exit(-1);

    }

    m_conn = new QSqlDatabase();
    *m_conn = QSqlDatabase::addDatabase("QPSQL");
    m_conn->setHostName("localhost");
    m_conn->setDatabaseName(db);
    m_conn->setUserName(user);
    m_conn->setPassword(pw);


    bool status = m_conn->open();

    // UPS init
    if (cmdline_args.indexOf("-upsip") > -1){
        m_ups_ip = cmdline_args.value(cmdline_args.indexOf("-upsip") +1);
        m_ups_port = cmdline_args.value(cmdline_args.indexOf("-upsport") +1).toUShort();
        m_ups_username = cmdline_args.value(cmdline_args.indexOf("-upsuser") +1);
    }
    else
    { if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-upsip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_ups_ip = init_str_list[init_str_list.indexOf("-upsip") +1];
            m_ups_port = init_str_list[init_str_list.indexOf("-upsport") +1].toUShort();
            m_ups_username = init_str_list[init_str_list.indexOf("-upsuser") +1];
        }
    }
    
    if (m_ups_ip == "")
    {
        qDebug ( "IP address of UPS is not set.\n\r");
    }
    else
    {
        
        if (m_ups_port <= 0)
        {
            qDebug ("UPS port error:  expected parameter\n\r");
        }
        else
        {
            
            m_ups = new ups_status(&m_ups_ip, &m_ups_port, &m_ups_username);
            //qDebug() << "UPS model: "<< QString::fromStdString(m_ups->m_model->getValue().data()[0]) <<"\n Voltage: "
            //<< QString::fromStdString(m_ups->m_voltage->getValue().data()[0]);
        }
    }
    
    //ENVEA group init
    //SO2
    if (cmdline_args.indexOf("-enveaipso2")>-1){
        m_envea_ip_so2 = cmdline_args.value(cmdline_args.indexOf("-enveaipso2") +1);
        m_envea_port_so2 = cmdline_args.value(cmdline_args.indexOf("-enveaportso2") +1).toUShort();
        m_envea_name_so2 = cmdline_args.value(cmdline_args.indexOf("-enveanameso2") +1);
    } else {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-enveaipso2%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_envea_ip_so2 = init_str_list[init_str_list.indexOf("-enveaipso2") +1];
            m_envea_port_so2 = init_str_list[init_str_list.indexOf("-enveaportso2") +1].toUShort();
            m_envea_name_so2 = init_str_list[init_str_list.indexOf("-enveanameso2") +1];
        }
    }

    if (m_envea_ip_so2 == "")
    {
        qDebug ( "IP address of ENVEA equipment for SO2 is not set.\n\r");
    }
    else
    {
        if (m_envea_port_so2 <= 0)
        {
            qDebug ("ENVEA equipment for SO2 port error:  expected parameter\n\r");
        }
        else
        {
            if (m_envea_name_so2 == "")
            {
                qDebug ("ENVEA equipment for SO2 the MODE 4 name error:  expected parameter\n\r");
            }
            else
            {
                m_envea_so2 = new enveas(this, &m_envea_ip_so2, &m_envea_port_so2, &m_envea_name_so2, enveas::SO2);
                m_envea_so2->sync();
                m_envea_so2->standby();
                m_envea_so2->start();
                connect(m_envea_so2, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_so2->verbose = true;
            }
        }
    }

    //H2S
    if (cmdline_args.indexOf("-enveaiph2s")>-1){
        m_envea_ip_h2s = cmdline_args.value(cmdline_args.indexOf("-enveaiph2s") +1);
        m_envea_port_h2s = cmdline_args.value(cmdline_args.indexOf("-enveaporth2s") +1).toUShort();
        m_envea_name_h2s = cmdline_args.value(cmdline_args.indexOf("-enveanameh2s") +1);

    } else {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-enveaiph2s%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_envea_ip_h2s = init_str_list[init_str_list.indexOf("-enveaiph2s") +1];
            m_envea_port_h2s = init_str_list[init_str_list.indexOf("-enveaporth2s") +1].toUShort();
            m_envea_name_h2s = init_str_list[init_str_list.indexOf("-enveanameh2s") +1];
        }
    }

    if (m_envea_ip_h2s == "")
    {
        qDebug ( "IP address of ENVEA equipment for H2S is not set.\n\r");
    }
    else
    {
        if (m_envea_port_h2s <= 0)
        {
            qDebug ("ENVEA equipment for H2S port error:  expected parameter\n\r");
        }
        else
        {
            if (m_envea_name_h2s == "")
            {
                qDebug ("ENVEA equipment for H2S the MODE 4 name error:  expected parameter\n\r");
            }
            else
            {
                m_envea_h2s = new enveas(this, &m_envea_ip_h2s, &m_envea_port_h2s, &m_envea_name_h2s, enveas::H2S);
                m_envea_h2s->sync();
                m_envea_h2s->standby();
                m_envea_h2s->start();
                connect(m_envea_h2s, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_h2s->verbose = true;
            }
        }
    }

    //SO2 + H2S
    if (cmdline_args.indexOf("-enveaipso2h2s")>-1){
        m_envea_ip_so2_h2s = cmdline_args.value(cmdline_args.indexOf("-enveaipso2h2s") +1);
        m_envea_port_so2_h2s = cmdline_args.value(cmdline_args.indexOf("-enveaportso2h2s") +1).toUShort();
        m_envea_name_so2_h2s = cmdline_args.value(cmdline_args.indexOf("-enveanameso2h2s") +1);

    } else {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-enveaipso2h2s%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_envea_ip_so2_h2s = init_str_list[init_str_list.indexOf("-enveaipso2h2s") +1];
            m_envea_port_so2_h2s = init_str_list[init_str_list.indexOf("-enveaportso2h2s") +1].toUShort();
            m_envea_name_so2_h2s = init_str_list[init_str_list.indexOf("-enveanameso2h2s") +1];
        }
    }

    if (m_envea_ip_so2_h2s == "")
    {
        qDebug ( "IP address of ENVEA equipment for SO2 + H2S is not set.\n\r");
    }
    else
    {
        if (m_envea_port_so2_h2s <= 0)
        {
            qDebug ("ENVEA equipment for SO2 + H2S port error:  expected parameter\n\r");
        }
        else
        {
            if (m_envea_name_so2_h2s == "")
            {
                qDebug ("ENVEA equipment for SO2 + H2S the MODE 4 name error:  expected parameter\n\r");
            }
            else
            {
                m_envea_so2_h2s = new enveas(this, &m_envea_ip_so2_h2s, &m_envea_port_so2_h2s, &m_envea_name_so2_h2s, enveas::SO2_H2S);
                m_envea_so2_h2s->sync();
                m_envea_so2_h2s->standby();
                m_envea_so2_h2s->start();
                connect(m_envea_so2_h2s, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_so2_h2s->verbose = true;
            }
        }
    }

    //NOx

    if (cmdline_args.indexOf("-enveaipnox")>-1){
        m_envea_ip_nox = cmdline_args.value(cmdline_args.indexOf("-enveaipnox") +1);
        m_envea_port_nox = cmdline_args.value(cmdline_args.indexOf("-enveaportnox") +1).toUShort();
        m_envea_name_nox = cmdline_args.value(cmdline_args.indexOf("-enveanamenox") +1);

    } else {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-enveaipnox%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_envea_ip_nox = init_str_list[init_str_list.indexOf("-enveaipnox") +1];
            m_envea_port_nox = init_str_list[init_str_list.indexOf("-enveaportnox") +1].toUShort();
            m_envea_name_nox = init_str_list[init_str_list.indexOf("-enveanamenox") +1];
        }
    }

    if (m_envea_ip_nox == "")
    {
        qDebug ( "IP address of ENVEA equipment for NOx is not set.\n\r");
    }
    else
    {
        if (m_envea_port_nox <= 0)
        {
            qDebug ("ENVEA equipment for NOx port error:  expected parameter\n\r");
        }
        else
        {
            if (m_envea_name_nox == "")
            {
                qDebug ("ENVEA equipment for NOx the MODE 4 name error:  expected parameter\n\r");
            }
            else
            {
                m_envea_nox = new enveas(this, &m_envea_ip_nox, &m_envea_port_nox, &m_envea_name_nox, enveas::NOx);
                m_envea_nox->sync();
                m_envea_nox->standby();
                m_envea_nox->start();
                connect(m_envea_nox, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_nox->verbose = true;
            }
        }
    }

    //NOx + NH3

    if (cmdline_args.indexOf("-enveaipnoxnh3")>-1){
        m_envea_ip_nox_nh3 = cmdline_args.value(cmdline_args.indexOf("-enveaipnoxnh3") +1);
        m_envea_port_nox_nh3 = cmdline_args.value(cmdline_args.indexOf("-enveaportnoxnh3") +1).toUShort();
        m_envea_name_nox_nh3 = cmdline_args.value(cmdline_args.indexOf("-enveanamenoxnh3") +1);
    }
    else
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-enveaipnoxnh3%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_envea_ip_nox_nh3 = init_str_list[init_str_list.indexOf("-enveaipnoxnh3") +1];
            m_envea_port_nox_nh3 = init_str_list[init_str_list.indexOf("-enveaportnoxnh3") +1].toUShort();
            m_envea_name_nox_nh3 = init_str_list[init_str_list.indexOf("-enveanamenoxnh3") +1];
        }
    }

    if (m_envea_ip_nox_nh3 == "")
    {
        qDebug ( "IP address of ENVEA equipment for NOx + NH3 is not set.\n\r");
    }
    else
    {
        if (m_envea_port_nox_nh3 <= 0)
        {
            qDebug ("ENVEA equipment for NOx + NH3 port error:  expected parameter\n\r");
        }
        else
        {
            if (m_envea_name_nox_nh3 == "")
            {
                qDebug ("ENVEA equipment for NOx + NH3 the MODE 4 name error:  expected parameter\n\r");
            }
            else
            {
                m_envea_nox_nh3 = new enveas(this, &m_envea_ip_nox_nh3, &m_envea_port_nox_nh3, &m_envea_name_nox_nh3, enveas::NOx_NH3);
                m_envea_nox_nh3->sync();
                m_envea_nox_nh3->standby();
                m_envea_nox_nh3->start();
                connect(m_envea_nox_nh3, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_nox_nh3->verbose = true;
            }
        }
    }

    //IVTM init
    if (cmdline_args.indexOf("-ivtmip")>-1){
        m_ivtm_ip = cmdline_args.value(cmdline_args.indexOf("-ivtmip") +1);
        m_ivtm_port = cmdline_args.value(cmdline_args.indexOf("-ivtmport") +1).toUShort();

    } else {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-ivtmip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_ivtm_ip = init_str_list[init_str_list.indexOf("-ivtmip") +1];
            m_ivtm_port = init_str_list[init_str_list.indexOf("-ivtmport") +1].toUShort();
        }
    }
    
    if (m_ivtm_ip == "")
    {
        qDebug ( "IP address of IVTM is not set.\n\r");
    }
    else
    {
        if (m_ivtm_port <= 0)
        {
            qDebug ("IVTM TCP Modbus port error:  expected parameter\n\r");
        }
        else
        {
            QUrl _url;
            _url.setHost(m_ivtm_ip);
            _url.setPort(m_ivtm_port);
            
            m_ivtm = new ivtm(_url, 0, 4, 1, 1000, this );
            
        }
        
    }
    
    
    // GammaET init
    if (cmdline_args.indexOf("-gammaetip")>-1){
        m_gammaet_ip = cmdline_args.value(cmdline_args.indexOf("-gammaetip") +1);
        m_gammaet_port = cmdline_args.value(cmdline_args.indexOf("-gammaetport") +1).toUShort();

    }
    else
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-gammaetip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_gammaet_ip = init_str_list[init_str_list.indexOf("-gammaetip") +1];
            m_gammaet_port = init_str_list[init_str_list.indexOf("-gammaetport") +1].toUShort();
        }
    }
    if (m_gammaet_ip == "")
    {
        qDebug ( "IP address of GammaET is not set.\n\r");
    }
    else
    {
        
        if (m_gammaet_port <= 0)
        {
            qDebug ("GammaET port error:  expected parameter\n\r");
        }
        else
        {
            m_gammaet = new GammaET(this, &m_gammaet_ip, &m_gammaet_port);
            
            connect(m_gammaet, SIGNAL(dataIsReady(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorDataModbus(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
        }
        
        
    }
    
    
    // modbusip init
    if (cmdline_args.indexOf("-moxaip")>-1){
        m_modbus_ip = cmdline_args.value(cmdline_args.indexOf("-moxaip") +1);
        m_modbus_port = cmdline_args.value(cmdline_args.indexOf("-moxaport485") +1).toUShort();
        m_modbus_port232 = cmdline_args.value(cmdline_args.indexOf("-moxaport232") +1).toUShort();

    }
    else
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-moxaip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_modbus_ip = init_str_list[init_str_list.indexOf("-moxaip") +1];
            m_modbus_port = init_str_list[init_str_list.indexOf("-moxaport485") +1].toUShort();
            m_modbus_port232 = init_str_list[init_str_list.indexOf("-moxaport232") +1].toUShort();
        }
    }
    if (m_modbus_ip == "")
    {
        qDebug ( "IP address of modbus is not set.\n\r");
    }
    else
    {
        
        if (m_modbus_port <= 0)
        {
            qDebug ("modbus485 port error:  expected parameter\n\r");
        }
        else
        {
            m_modbusip = new ModbusIP(this, &m_modbus_ip, &m_modbus_port);
            
            connect(m_modbusip, SIGNAL(dataIsReady(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorDataModbus(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            
        }
        
        if (m_modbus_port232 <= 0)
        {
            qDebug ("modbus232 port error:  expected parameter\n\r");
        }
        else
        {
            m_modbusip232 = new ModbusIP(this, &m_modbus_ip, &m_modbus_port232, 232);
            
            connect(m_modbusip232, SIGNAL(dataIsReady(bool*, QMap<QString, int>*, QMap<QString, int>*)), this, SLOT(fillSensorDataModbus(bool*, QMap<QString, int>*, QMap<QString, int>*))); //fill several data to one sensor's base
            
        }
    }
    
    //topas init
    QString _serialnum;
    if (cmdline_args.indexOf("-topasip") > -1){
        m_topas_ip = cmdline_args.value(cmdline_args.indexOf("-topasip") +1);
        m_topas_port = cmdline_args.value(cmdline_args.indexOf("-topasport") +1).toUShort();
        QString _serialnum = cmdline_args.value(cmdline_args.indexOf("-topasnum") +1);

    }
    else
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-topasip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_topas_ip = init_str_list[init_str_list.indexOf("-topasip") +1];
            m_topas_port = init_str_list[init_str_list.indexOf("-topasport") +1].toUShort();
            _serialnum = init_str_list[init_str_list.indexOf("-topasnum") +1];
        }
    }

    if (m_topas_ip == "")
    {
        qDebug ( "IP address of the TOPAS is not set.\n\r");
    }
    else
    {
        
        if (m_topas_port <= 0)
        {
            qDebug ("TOPAS port error:  expected parameter\n\r");
        }
        else
        {
            if (_serialnum == "")
            {
                qDebug ("TOPAS serial number ia absend error:  expected parameter\n\r");
            }
            else
            {
                m_topasip = new TopasIP(this, &m_topas_ip, &m_topas_port, &_serialnum);
                if (m_topasip){
                    connect(m_topasip, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*))); //fill several data to one sensor's base
                    
                }
            }
        }
        
    }
    

    if (!status)
    {
        //releaseModbus();
        
        qDebug() << ( QString("Connection error: " + m_conn->lastError().text()).toLatin1().constData()) <<   " \n\r";
        exit(-1);
        
    }
    
    
    
    connect( this, SIGNAL(AsciiPortActive(bool)), this, SLOT(onAsciiPortActive(bool)));
    
    QTimer * t = new QTimer( this );
    connect( t, SIGNAL(timeout()), this, SLOT(pollForDataOnBus()));
    t->start( 500 );
    
    m_pollTimer = new QTimer( this );
    // connect( m_pollTimer, SIGNAL(timeout()), this, SLOT(sendModbusRequest()));
    connect( m_pollTimer, SIGNAL(timeout()), this, SLOT(readSocketStatus()));
    
    //  m_statusTimer = new QTimer( this );
    //connect( m_statusTimer, SIGNAL(timeout()), this, SLOT(resetStatus()));
    //m_statusTimer->setSingleShot(true);
    
    m_renovateTimer = new QTimer(this);
    connect( m_renovateTimer, SIGNAL(timeout()), this, SLOT(renovateSlaveID()));
    
    m_transactTimer = new QTimer(this);
    connect( m_transactTimer, SIGNAL(timeout()), this, SLOT(transactionDB()));
    
    m_meteo_uuid = new  QMap<QString, QUuid>;
    m_meteo_types =  new QMap<QString, QString>;
    m_uuid = new  QMap<QString, QUuid>;
    m_data = new  QMap<QString, int>;
    m_measure =  new QMap<QString, int>;
    
    //alarm init
    m_alarmip = cmdline_args.value(cmdline_args.indexOf("-alarmip") +1);
    m_alarmport = cmdline_args.value(cmdline_args.indexOf("-alarmport") +1).toUShort();

    if (m_alarmip == "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-alarmip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_alarmip = init_str_list[init_str_list.indexOf("-alarmip") +1];
            m_alarmport = init_str_list[init_str_list.indexOf("-alarmport") +1].toUShort();
        }
    }

    if (m_alarmip == "")
    {
        qDebug ( "IP address of fire alarm is not set.\n\r");

    }
    else
    {
        if (m_alarmport <= 0)
        {
            qDebug ( "Port of fire alarm is not set.");
        }
        else
        {
            m_fire = new TcpSock(this, &m_alarmip, &m_alarmport);
        }
        
    }
    
    // Dust equipment init
    
    m_dust_ip = cmdline_args.value(cmdline_args.indexOf("-dustip") +1);
    m_dust_port = cmdline_args.value(cmdline_args.indexOf("-dustport") +1).toUShort();

    if (m_dust_ip == "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-dustip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_dust_ip = init_str_list[init_str_list.indexOf("-dustip") +1];
            m_dust_port = init_str_list[init_str_list.indexOf("-dustport") +1].toUShort();
        }
    }

    if (m_dust_ip == "")
    {
        qDebug ( "IP address of dust measure equipment is not set.\n\r");
    }
    else
    {
        if (m_dust_port <= 0)
        {
            qDebug ( "Port of dust measure equipment is not set.\n\r");
        }
        else
        {
            m_dust = new DustTcpSock(this, &m_dust_ip, &m_dust_port);
            //m_dust->sendData( "RDMN");
            /* while (!m_dust->is_read);*/
            m_dust->is_read = false;
            m_dust->sendData( "MSTART");
            /*while (!m_dust->is_read);*/
            //m_dust->is_read = false;
            
        }
        
    }
    
    // EDM init for tty...
    
    if (cmdline_args.indexOf("-grimmip") >-1){
        m_grimm_ip = cmdline_args.value(cmdline_args.indexOf("-grimmip") +1);
        m_grimmport = cmdline_args.value(cmdline_args.indexOf("-grimmport") +1).toUShort();

    }
    else
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-grimmip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_grimm_ip = init_str_list[init_str_list.indexOf("-grimmip") +1];
            m_grimmport = init_str_list[init_str_list.indexOf("-grimmport") +1].toUShort();
        }
    }

    if ( m_grimm_ip == "")
    {
        qDebug ( "IP address of the Grimm equipment is not set.\n\r");
    }
    else
    {
        if (m_grimmport <= 0)
        {
            qDebug ("Error:  wrong data of the Grimm port parameter\n\r");
            
        } else {
            
            m_grimm = new Grimm(this, &m_grimm_ip, &m_grimmport);
            
            
            //connect(m_grimm->tracker->set_data_recv_bundle, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*))); //fill several data to one sensor's base
            m_grimm->setCallbackFunc(static_fillSensorData);
            
        }
        
    }
    
    
    // Meteostation init
    
    m_meteo_ip = cmdline_args.value(cmdline_args.indexOf("-meteoip") +1);
    m_meteo_port = cmdline_args.value(cmdline_args.indexOf("-meteoport") +1).toUShort();
    QString meteoparams = cmdline_args.value(cmdline_args.indexOf("-meteoparams") +1);

    if (cmdline_args.indexOf("-meteomodel") > -1){
        m_meteo_model = cmdline_args.value(cmdline_args.indexOf("-meteomodel") +1);
    }

    if (m_meteo_ip == "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-meteoip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_meteo_ip = init_str_list[init_str_list.indexOf("-meteoip") +1];
            m_meteo_port = init_str_list[init_str_list.indexOf("-meteoport") +1].toUShort();
            meteoparams = init_str_list[init_str_list.indexOf("-meteoparams") +1];
            m_meteo_model = init_str_list[init_str_list.indexOf("-meteomodel") +1];

        }
    }

    if (m_meteo_ip == "")
    {
        qDebug ( "IP address of meteostation is not set.\n\r");
    }
    else
    {
        if (m_meteo_port <= 0)
        {
            qDebug ( "Port of meteostation is not set.");
        }
        else
        {

            QSqlQuery *query= new QSqlQuery ("select * from sensors_data where typemeasure = 'Темп. внутренняя' order by date_time desc", *m_conn);
            qDebug() << "Temp. inner status is " <<   query->isActive()<< " and err " << query->lastError().text() << "\n\r";
            query->first();
            QSqlRecord rec = query->record();
            
            float temp_in = (rec.field("measure").value().toFloat());
            query->finish();
            
            QSqlQuery *queryout= new QSqlQuery ("select * from sensors_data where typemeasure = 'Темп. внешняя' order by date_time desc", *m_conn);
            qDebug() << "Temp. inner status is " <<   query->isActive()<< " and err " << query->lastError().text() << "\n\r";
            queryout->first();
            rec = queryout->record();
            
            float temp_out = (rec.field("measure").value().toFloat());
            query->finish();
            
            

            
            if (m_meteo_model == ""){
                if (meteoparams == "db")
                {
                    m_meteo = new MeteoTcpSock(this, &m_meteo_ip, &m_meteo_port, temp_in, temp_out);
                } else{
                    m_meteo = new MeteoTcpSock(this, &m_meteo_ip, &m_meteo_port);
                }
            } else {
                if (meteoparams == "db")
                {
                    m_meteo = new MeteoTcpSock(this, &m_meteo_ip, &m_meteo_port, temp_in, temp_out, &m_meteo_model);
                } else {
                    m_meteo = new MeteoTcpSock(this, &m_meteo_ip, &m_meteo_port, &m_meteo_model);
                    
                }
            }
            if (verbose)
                m_meteo->verbose = true;
        }
        
    }
    
    // Serinus init
    //  -serinusip 192.168.1.101
    
    m_serinus_ip = cmdline_args.value(cmdline_args.indexOf("-serinusip") +1);
    m_serinus_port = cmdline_args.value(cmdline_args.indexOf("-serinusport") +1).toUShort();

    if (m_serinus_ip == "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-serinusip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_serinus_ip = init_str_list[init_str_list.indexOf("-serinusip") +1];
            m_serinus_port = init_str_list[init_str_list.indexOf("-serinusport") +1].toUShort();
        }
    }

    if (m_serinus_ip == "")
    {
        qDebug ( "IP address of the Serinus 51 is not set.\n\r");
    }
    else
    {
        if (m_serinus_port <= 0)
        {
            qDebug ( "Port of the Serinus 51 is not set.\n\r");
        }
        else
        {
            m_serinus = new Serinus(this, &m_serinus_ip, &m_serinus_port);
            connect(m_serinus, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            //QObject::connect(m_serinus, SIGNAL(dataIsReady(const QString)), this, SLOT(test())); //fill several data to one sensor's base
            if (verbose)
                m_serinus->verbose = true;
        }
    }
    
    m_serinus_ip55 = cmdline_args.value(cmdline_args.indexOf("-serinusip55") +1);
    m_serinus_port55 = cmdline_args.value(cmdline_args.indexOf("-serinusport55") +1).toUShort();

    if (m_serinus_ip55 == "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-serinusip55%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_serinus_ip55 = init_str_list[init_str_list.indexOf("-serinusip55") +1];
            m_serinus_port55 = init_str_list[init_str_list.indexOf("-serinusport55") +1].toUShort();
        }
    }

    if (m_serinus_ip55 == "")
    {
        qDebug ( "IP address of the Serinus55 is not set.\n\r");
    }
    else
    {
        if (m_serinus_port55 <= 0)
        {
            qDebug ( "Port of the Serinus55 is not set.\n\r");
        }
        else
        {
            m_serinus55 = new Serinus(this, &m_serinus_ip55, &m_serinus_port55, int(55));
            connect(m_serinus55, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            //QObject::connect(m_serinus, SIGNAL(dataIsReady(const QString)), this, SLOT(test())); //fill several data to one sensor's base
            if (verbose)
                m_serinus55->verbose = true;
        }
    }
    
    m_serinus_ip50 = cmdline_args.value(cmdline_args.indexOf("-serinusip50") +1);
    m_serinus_port50 = cmdline_args.value(cmdline_args.indexOf("-serinusport50") +1).toUShort();

    if (m_serinus_ip50== "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-serinusip50%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_serinus_ip50 = init_str_list[init_str_list.indexOf("-serinusip50") +1];
            m_serinus_port50 = init_str_list[init_str_list.indexOf("-serinusport50") +1].toUShort();
        }
    }

    if (m_serinus_ip50 == "")
    {
        qDebug ( "IP address of the Serinus50 is not set.\n\r");
    }
    else
    {
        if (m_serinus_port50 <= 0)
        {
            qDebug ( "Port of the Serinus50 is not set.\n\r");
        }
        else
        {
            m_serinus50 = new Serinus(this, &m_serinus_ip50, &m_serinus_port50, int(50));
            connect(m_serinus50, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            //QObject::connect(m_serinus, SIGNAL(dataIsReady(const QString)), this, SLOT(test())); //fill several data to one sensor's base
            if (verbose)
                m_serinus50->verbose = true;
        }
    }
    
    m_serinus_ip30 = cmdline_args.value(cmdline_args.indexOf("-serinusip30") +1);
    m_serinus_port30 = cmdline_args.value(cmdline_args.indexOf("-serinusport30") +1).toUShort();

    if (m_serinus_ip30== "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-serinusip30%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_serinus_ip30 = init_str_list[init_str_list.indexOf("-serinusip30") +1];
            m_serinus_port30 = init_str_list[init_str_list.indexOf("-serinusport30") +1].toUShort();
        }
    }

    if (m_serinus_ip30 == "")
    {
        qDebug ( "IP address of the Serinus30 is not set.\n\r");
    }
    else
    {
        if (m_serinus_port30 <= 0)
        {
            qDebug ( "Port of the Serinus30 is not set.\n\r");
        }
        else
        {
            m_serinus30 = new Serinus(this, &m_serinus_ip30, &m_serinus_port30, int(30));
            connect(m_serinus30, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            //QObject::connect(m_serinus, SIGNAL(dataIsReady(const QString)), this, SLOT(test())); //fill several data to one sensor's base
            if (verbose)
                m_serinus30->verbose = true;
        }
    }
    
    m_serinus_ip44 = cmdline_args.value(cmdline_args.indexOf("-serinusip44") +1);
    m_serinus_port44 = cmdline_args.value(cmdline_args.indexOf("-serinusport44") +1).toUShort();

    if (m_serinus_ip44== "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-serinusip44%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_serinus_ip44 = init_str_list[init_str_list.indexOf("-serinusip44") +1];
            m_serinus_port44 = init_str_list[init_str_list.indexOf("-serinusport44") +1].toUShort();
        }
    }


    if (m_serinus_ip44 == "")
    {
        qDebug ( "IP address of the Serinus44 is not set.\n\r");
    }
    else
    {
        if (m_serinus_port44 <= 0)
        {
            qDebug ( "Port of the Serinus44 is not set.\n\r");
        }
        else
        {
            m_serinus44 = new Serinus(this, &m_serinus_ip44, &m_serinus_port44, int(44));
            connect(m_serinus44, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            //QObject::connect(m_serinus, SIGNAL(dataIsReady(const QString)), this, SLOT(test())); //fill several data to one sensor's base
            if (verbose)
                m_serinus44->verbose = true;
        }
    }
    // ACA-Liga init
    //  -ligaip 192.168.1.111 -ligaport 7120
    
    m_liga_ip = cmdline_args.value(cmdline_args.indexOf("-ligaip") +1);
    m_liga_port = cmdline_args.value(cmdline_args.indexOf("-ligaport") +1).toUShort();

    if (m_liga_ip== "")
    {
        if (status){
            QSqlQuery *query= new QSqlQuery ("select * from service where init_str like '%-ligaip%' and is_active = 'true' and is_present = 'true'", *m_conn);
            query->first();
            QSqlRecord rec = query->record();

            QString init_str = QString(rec.field("init_str").value().toString()).append(" ").append(rec.field("parameter").value().toString());
            QStringList init_str_list = init_str.split(QLatin1Char(' '));

            query->finish();
            query->~QSqlQuery();

            m_liga_ip = init_str_list[init_str_list.indexOf("-ligaip") +1];
            m_liga_port = init_str_list[init_str_list.indexOf("-ligaport") +1].toUShort();
        }
    }

    if (m_liga_ip == "")
    {
        qDebug ( "IP address of the ACA-Liga is not set.\n\r");
    }
    else
    {
        if (m_liga_port <= 0)
        {
            qDebug ( "Port of the ACA-Liga is not set.\n\r");
        }
        else
        {
            m_liga = new Liga( &m_liga_ip, &m_liga_port);
            //   connect(m_liga, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*))); //fill several data to one sensor's base
            
        }
    }
    
    //end of equipments init.
    
    //timers initialization
    m_renovateTimer->start(300000); // every 5 minutes we set all slave ID to active mode for polling despite of really state
    
    QString polltime = cmdline_args.value(cmdline_args.indexOf("-polltime") +1);
    if (polltime == "")
    {
        m_pollTimer->start(5000); //start polling timer with hardcoded period 5 sec.
        
        qDebug ( "Polling time is set up to 5 sec.\n\r");
        
    }
    else
    {
        m_pollTimer->start(polltime.toInt() * 1000);
        
        qDebug () << "Polling time is set up to " << polltime <<" sec.\n\r";
    }
    
    
    startTransactTimer(m_conn);    //start transaction timer - must be after polling timer!!!
    
    resetStatus();
    
    //range coefficients init
    m_range = new QMap<QString, int>;

    if (m_envea_h2s||m_envea_nox||m_envea_so2||m_envea_nox_nh3||m_envea_so2_h2s)
    {
        m_range->insert("CO", 10);
        m_range->insert("NO2", 1000000);
        m_range->insert("NO", 1000000);
        m_range->insert("SO2", 1000000);
        m_range->insert("H2S", 1000000);
        m_range->insert("CH2O", 1000000);
        m_range->insert("O3", 1000000);
        m_range->insert("NH3", 1000000);
        m_range->insert("NOx", 1000000);
    } else {
        m_range->insert("CO", 10);
        m_range->insert("NO2", 1000);
        m_range->insert("NO", 1000);
        m_range->insert("SO2", 1000);
        m_range->insert("H2S", 1000);
        m_range->insert("CH2O", 1000);
        m_range->insert("O3", 1000);
        m_range->insert("NH3", 1000);
        m_range->insert("NOx", 1000);
    }
    
    m_range->insert("бензол", 1000);
    m_range->insert("толуол", 1000);
    m_range->insert("этилбензол", 1000);
    m_range->insert("м,п-ксилол", 1000);
    m_range->insert("о-ксилол", 1000);
    m_range->insert("хлорбензол", 1000);
    m_range->insert("стирол", 1000);
    m_range->insert("фенол", 1000);
    
    m_range->insert("Ресурс сенс. NO", 1);
    m_range->insert("Ресурс сенс. H2S", 1);
    m_range->insert("Напряжение мин.", 1);
    m_range->insert("Напряжение макс.", 1);
    
    if (m_topasip){
        m_range->insert("PM", 1000000);
        m_range->insert("PM1", 1000000);
        m_range->insert("PM2.5", 1000000);
        m_range->insert("PM4", 1000000);
        m_range->insert("PM10", 1000000);
        m_range->insert("TMP_PM", 1000);
        m_range->insert("HUM_PM", 1000);
        
    } else {
        
        m_range->insert("PM", 1000);
        m_range->insert("PM1", 1000);
        m_range->insert("PM2.5", 1000);
        m_range->insert("PM4", 1000);
        m_range->insert("PM10", 1000);
    }
    
    //UPS data init
    if (m_ups){
        m_ups->read_voltage();
        m_data->insert("Напряжение мин.", m_ups->voltage);
        //m_measure->insert("Напряжение мин.", 1);
        m_data->insert("Напряжение макс.", m_ups->voltage);
        //m_measure->insert("Напряжение макс.", 1);
    }
    
    
    
    //Grimm listening confiramtion & range
    if (m_grimm){
        ms_range->insert("PM", 1000000);
        ms_range->insert("PM1", 1000000);
        ms_range->insert("PM2.5", 1000000);
        ms_range->insert("PM4", 1000000);
        ms_range->insert("PM10", 1000000);
        m_grimm->send_go();
    }
    
    //GammaET elements
    m_range->insert("CH", 1000);
    m_range->insert("HCH", 1000);
    m_range->insert("CH4", 1000);
    
    //Insert Modbus slave id group
    QStringListIterator iterator(cmdline_args);
    
    if (m_modbusip || m_modbus)
    {
        m_pool = new QMap<int, int>;
        
        while (iterator.hasNext())
        {
            if (iterator.next() == "-slaveid") // read slave id for Modbus
            {
                QString tmp = iterator.next().toLocal8Bit().constData();
                while (tmp.indexOf("-") == -1) {
                    
                    //m_pool->push_back(tmp.toInt());
                    m_pool->insert(tmp.toInt(), numCoils);
                    
                    tmp = iterator.next().toLocal8Bit().constData();
                }
                // return;
            }
        }
        if (m_modbusip || m_modbus){
            if (m_pool->size() == 0)
            {
                for ( int i = 0; i < 16; i++)
                {
                    m_pool->insert(i, numCoils);
                    
                }
            }
        }
        slaveID = new QVector<bool>(30, true); //max slaveID number is hardcoded to 30 devices
        
        q_poll = new uint8_t[29];
        memset(q_poll, 24, 30);
        
    }
    
    
    if (m_gammaet){
        m_gammapool = new QMap<int, int>;
        while (iterator.hasNext())
        {
            if (iterator.next() == "-slavegammaetid") // read slave id for GammaET devices Modbus
                // insert hardcoded  <numCoils> addresses for GammaET if is empty cmd. string parameter
                
            {
                QString tmp = iterator.next().toLocal8Bit().constData();
                while (tmp.indexOf("-") == -1) {
                    
                    m_gammapool->insert(tmp.toInt(), numCoils);
                    
                    tmp = iterator.next().toLocal8Bit().constData();
                }
            }
        }
    }
    if (m_gammaet){
        
        if (m_gammapool->size() == 0) // insert 5 slave id and hardcoded  <numCoils> addresses for GammaET if is empty cmd. string parameter
        {
            for ( int i = 0; i < 5; i++)
            {
                m_gammapool->insert(i, numCoils);
                
            }
        }
    }
    
    //status init
    
    m_status->insert("CO", ABSENT);
    m_status->insert("NO2", ABSENT);
    m_status->insert("NO", ABSENT);
    
    m_status->insert("O3", ABSENT);
    m_status->insert("NH3", ABSENT);
    m_status->insert("CH2O", ABSENT);
    
    m_status->insert("NOx", ABSENT);
    
    
    m_status->insert("SO2", ABSENT);
    m_status->insert("H2S", ABSENT);
    
    
    m_status->insert("бензол", ABSENT);
    m_status->insert("толуол", ABSENT);
    m_status->insert("этилбензол", ABSENT);
    m_status->insert("м,п-ксилол", ABSENT);
    m_status->insert("о-ксилол", ABSENT);
    m_status->insert("хлорбензол", ABSENT);
    m_status->insert("стирол", ABSENT);
    m_status->insert("фенол", ABSENT);
    
    m_status->insert("Напряжение мин.", ABSENT);
    m_status->insert("Напряжение макс.", ABSENT);
    m_status->insert("Темп. сенсор ИБП", ABSENT);
    
    
    m_status->insert("PM", ABSENT);
    m_status->insert("PM1", ABSENT);
    m_status->insert("PM2.5", ABSENT);
    m_status->insert("PM4", ABSENT);
    m_status->insert("PM10", ABSENT);
    
    //GammaET elements
    
    m_status->insert("CH", ABSENT);
    m_status->insert("HCH", ABSENT);
    m_status->insert("CH4", ABSENT);
    
    
    initStatus();
    
    //try ssh
    if (cmdline_args.indexOf("-sship") > -1)
        m_ssh_ip = cmdline_args.value(cmdline_args.indexOf("-sship") +1);
    if (m_ssh_ip == "")
    {
        qDebug ( "SSH IP address is not set.\n\r");
    }
    else
    {
        
        m_ssh_port = cmdline_args.value(cmdline_args.indexOf("-sshport") +1).toUShort();
        if (m_ssh_port <= 0)
        {
            qDebug ("SSH port error:  expected parameter\n\r");
        }
        else
        {
            m_ssh_user = cmdline_args.value(cmdline_args.indexOf("-sshuser") +1).toUShort();
            if (m_ssh_user == "")
            {
                qDebug ("SSH user error:  expected parameter\n\r");
            } else {
                m_ssh_pwd = cmdline_args.value(cmdline_args.indexOf("-sshpwdr") +1).toUShort();
                if (m_ssh_pwd ==""){
                    qDebug ("Password user error:  expected parameter\n\r");
                } else {
                    m_ssh_command = cmdline_args.value(cmdline_args.indexOf("-sshcommand") +1).toUShort();
                    if (m_ssh_user == "")
                    {
                        qDebug ("SSH command error:  expected parameter\n\r");
                    } else {
                        //begin ssh session
                        ssh_session session;
                        ssh_channel channel;
                        int rc;
                        char buffer[1024];
                        unsigned int nbytes;
                        
                        qDebug("Session...\n");
                        session = ssh_new();
                        if (session == NULL) exit(-1);
                        
                        ssh_options_set(session, SSH_OPTIONS_HOST, m_ssh_ip.toLatin1());
                        ssh_options_set(session, SSH_OPTIONS_PORT, &m_ssh_port);
                        ssh_options_set(session, SSH_OPTIONS_USER, m_ssh_user.toLatin1());
                        
                        qDebug("Connecting...\n");
                        rc = ssh_connect(session);
                        if (rc != SSH_OK) error(session);
                        
                        qDebug("Password Autentication...\n");
                        rc = ssh_userauth_password(session, NULL, m_ssh_pwd.toLatin1());
                        if (rc != SSH_AUTH_SUCCESS) error(session);
                        
                        qDebug("Channel...\n");
                        channel = ssh_channel_new(session);
                        if (channel == NULL)  error(session);
                        
                        qDebug("Opening...\n");
                        rc = ssh_channel_open_session(channel);
                        if (rc != SSH_OK) error(session);
                        
                        qDebug("Executing remote command...\n");
                        rc = ssh_channel_request_exec(channel, m_ssh_command.toLatin1());
                        if (rc != SSH_OK) error(session);
                        
                        qDebug("Received:\n");
                        nbytes = uint( ssh_channel_read(channel, buffer, sizeof(buffer), 0));
                        // while (nbytes > 0) {
                        //      fwrite(buffer, 1, nbytes, stdout);
                        //      nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
                        //}
                        qDebug("Received:\n", nbytes);
                        free_channel(channel);
                        free_session(session);
                    }
                }
            }
            
            
            
        }
        
    }
    
}


processor::~processor()
{ if (m_dust)
        m_dust->sendData( "MSTOP");
    if (m_serialModbus){
        modbus_close( m_serialModbus );
        modbus_free( m_serialModbus );
        m_serialModbus = NULL;
    }
    delete m_threadPool;
    //delete  m_threadPoolSlowProcess;
}

void processor::releaseModbus()
{
    modbus_close( m_serialModbus );
    modbus_free( m_serialModbus );
    m_serialModbus = NULL;
    emit(AsciiPortActive(false));
    
}

void processor::onSendButtonPress( void )
{
    //if already polling then stop
    if( m_pollTimer->isActive() )
    {
        m_pollTimer->stop();
        m_transactTimer->stop();
        //ui->sendBtn->setText( tr("Send") );
        
    }
    else
    {
        //if polling requested then enable timer
        if( m_poll )
        {
            //m_pollTimer->start( 1000 * ui->pollingInterval->value() );
            m_transactTimer->start();
            //  qCDebug(QT_QM_ASCII_OPTEC_MAIN) <<   ui->pollingInterval->value() << " interval";
            
            // ui->sendBtn->setText( tr("Stop") );
        }
        
        sendModbusRequest();
    }
}







static QString descriptiveDataTypeName( int funcCode )
{
    switch( funcCode )
    {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_MULTIPLE_COILS:
        return "Coil (binary)";
    case MODBUS_FC_READ_DISCRETE_INPUTS:
        return "Discrete Input (binary)";
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_WRITE_SINGLE_REGISTER:
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        return "Holding Register (16 bit)";
    case MODBUS_FC_READ_INPUT_REGISTERS:
        return "Input Register (16 bit)";
    default:
        break;
    }
    return "Unknown";
}


static inline QString embracedString( const QString & s )
{
    return s.section( '(', 1 ).section( ')', 0, 0 );
}


static inline int stringToHex( QString s )
{
    return s.replace( "0x", "" ).toInt( NULL, 16 );
}



void processor::cover_modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest, int *result)
{
    *result = modbus_read_registers(ctx,  addr, nb, dest);
}

void processor::sendModbusRequest( void )
{
    
    QString tmp_type_measure;
    int j = 0;
    QMap<int, int>::iterator slave;
    //if( m_tcpActive )
    //  ui->tcpSettingsWidget->tcpConnect();
    
    if( m_modbus == NULL )
    {
        qDebug() << "Modbus is not configured!\n" ;
        return;
    }
    //for( int j = 0; j < m_pool->size() ; j++ )
    for (slave = m_pool->begin(); slave != m_pool->end(); ++slave)
    {
        tmp_type_measure.clear(); //It's reset when new Slave ID is viewed
        
        if (slaveID->value(slave.key() - 1))
        {
            //QString slave = QString(j);
            const int func = funcModbus;
            const int addr = addrModbus;
            int num = numCoils;
            uint8_t dest[1024];
            uint16_t * dest16 = (uint16_t *) dest;
            
            memset( dest, 0, 1024 );
            
            int ret = -1;
            bool is16Bit = false;
            bool writeAccess = false;
            const QString dataType = descriptiveDataTypeName( func );
            
            //modbus_set_slave( m_modbus, m_pool->value(j) );
            modbus_set_slave( m_modbus, slave.key() );
            
            switch( func )
            {
            case MODBUS_FC_READ_COILS:
                ret = modbus_read_bits( m_modbus, addr, slave.value(), dest );
                break;
            case MODBUS_FC_READ_DISCRETE_INPUTS:
                ret = modbus_read_input_bits( m_modbus, addr, slave.value(), dest );
                break;
            case MODBUS_FC_READ_HOLDING_REGISTERS:
                //while(!m_mutex->tryLock());{
                
                // qDebug() << "ret... " << ret << " \n";
                //ret = modbus_read_registers( m_modbus, addr, num, dest16 );
                ret = modbus_read_registers( m_modbus, addr, slave.value(), dest16 );
                is16Bit = true;
                // m_mutex->unlock();
                break;
            case MODBUS_FC_READ_INPUT_REGISTERS:
                ret = modbus_read_input_registers( m_modbus, addr, slave.value(), dest16 );
                is16Bit = true;
                break;
            case MODBUS_FC_WRITE_SINGLE_COIL:
                /*  ret = modbus_write_bit( m_modbus, addr,
                                        ui->regTable->item( 0, DataColumn )->
                                        text().toInt(0, 0) ? 1 : 0 );
                writeAccess = true;
                num = 1;*/
                break;
            case MODBUS_FC_WRITE_SINGLE_REGISTER:
                /*ret = modbus_write_register( m_modbus, addr,
                                             ui->regTable->item( 0, DataColumn )->
                                             text().toInt(0, 0) );
                writeAccess = true;
                num = 1;*/
                break;
                
            case MODBUS_FC_WRITE_MULTIPLE_COILS:
            {
                /* uint8_t * data = new uint8_t[num];
                for( int i = 0; i < num; ++i )
                {
                    data[i] = ui->regTable->item( i, DataColumn )->
                            text().toInt(0, 0);
                }
                ret = modbus_write_bits( m_modbus, addr, num, data );
                delete[] data;
                writeAccess = true;*/
                break;
            }
            case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            {
                /*uint16_t * data = new uint16_t[num];
                for( int i = 0; i < num; ++i )
                {
                    data[i] = ui->regTable->item( i, DataColumn )->
                            text().toInt(0, 0);
                }
                ret = modbus_write_registers( m_modbus, addr, num, data );
                delete[] data;
                writeAccess = true;*/
                break;
            }
                
            default:
                break;
            }
            
            if( ret == slave.value()  )
            {
                if( writeAccess )
                {
                    qDebug() <<  tr( "Values successfully sent\n\r" ) ;
                    
                }
                else
                {
                    // RESULT PARSING
                    
                    bool b_hex = is16Bit && true;
                    QString qs_num;
                    
                    for( int i = 0; i < slave.value(); ++i )
                    {
                        int data = is16Bit ? dest16[i] : dest[i];
                        
                        
                        q_poll[slave.key() - 1] = 24; //reset of fault polling <i> counter
                        
                        switch( i ) //register value explanation
                        {
                        case 0:{
                            QString result;
                            QTextStream(&result) << data;
                            qDebug() << QDateTime::currentDateTime().toString( "yyyy-MM-dd hh:mm:ss ")  << "\nSlave address = " << result;
                            
                        }
                            break;
                        case 1:
                        {
                            uint8_t _mode = data & 0xFF;
                            QString md, name, result;
                            
                            
                            
                            uint8_t _type = (data >> 8) & 0xF;
                            
                            if (_type == 2){ name = "CO"; //detect type of a sensor HARDCODED for OPTEC's equipments
                                if (_mode == 2) md = "fault";
                                else
                                {md = (_mode ?  "off" :  "measuring");};
                            }
                            if (_type == 4){ name = "NO2"; //detect type of a sensor HARDCODED
                                if (_mode == 7) md = "change sensor";
                                else
                                {md = (_mode ?  "off" :  "measuring");};
                            }
                            
                            if (_type == 6){ name = "SO2"; //detect type of a sensor HARDCODED
                                if (_mode == 7) md = "change sensor";
                                else
                                {md = (_mode ?  "off" :  "measuring");};
                            }
                            
                            if ((_type == 2) && (*dest16 == 30)){ name = "CH2O"; //hardcoded for the Fort measure equipment address = 30 (or OPTEC's equipments)
                                if (_mode == 2) md = "fault";
                                else
                                {md = (_mode ?  "off" :  "measuring");};
                            }
                            
                            if (_type == 1){ name = "O3"; //detect type of a sensor HARDCODED for OPTEC's equipments
                                if (_mode == 2) md = "fault";
                                else
                                {md = (_mode ?  "off" :  "measuring");};
                            }
                            
                            tmp_type_measure = name;
                            
                            uint8_t _number = (data >> 12) & 0xF;
                            QTextStream(&result) << name << " : " << md << " : " << _number;
                            qDebug() << result;
                            
                            break;
                        }
                        case 2:{
                            QString result;
                            int tmp, cnt;
                            
                            QTextStream(&result) << float (data)/m_range->value(tmp_type_measure) << " mg/m3";
                            
                            
                            tmp = m_data->value(tmp_type_measure, -1); //detect first measure
                            if ( tmp == -1){
                                m_data->insert(tmp_type_measure, QString::number(data, 16).toInt()); // insert into QMap ordering pair of measure first time
                                m_measure->insert(tmp_type_measure, 1);
                                qDebug() << "measure... \n type= " << tmp_type_measure << " value = " << (float)QString::number(data, 16).toInt()/m_range->value(tmp_type_measure);
                                
                            } else {
                                m_data->insert(tmp_type_measure, tmp + QString::number(data, 16).toInt() );
                                cnt = m_measure->value(tmp_type_measure, 0);
                                m_measure->insert(tmp_type_measure, cnt+1);
                                qDebug()<<  "measure... \n type= " << tmp_type_measure << " value = " << (float)QString::number(data, 16).toInt()/m_range->value(tmp_type_measure)<<"\n\r";
                                
                            }
                            break;
                        }
                        case 3:
                        {
                            uint8_t _mode = data & 0xFF;
                            QString md, name, result;
                            
                            if (_mode == 2) md = "fault";
                            else
                            {md = (_mode ?  "off" :  "measuring");};
                            
                            uint8_t _type = (data >> 8) & 0xF;
                            if (_type != 0){
                                if (_type == 7){ name = "NO"; //detect type of sensor HARDCODED
                                    if (_mode == 7) md = "change sensor";
                                    else
                                    {md = (_mode ?  "off" :  "measuring\n\r");};
                                }
                                
                                if (_type == 3){ name = "H2S"; //detect type of sensor HARDCODED
                                    if (_mode == 7) md = "change sensor\n\r";
                                    else
                                    {md = (_mode ?  "off" :  "measuring\n\r");};
                                }
                            } else {
                                
                                _type = (data ) & 0xF; //in case of only one byte for some equipments
                                if (_type == 7){ name = "NO"; //detect type of sensor HARDCODED
                                    
                                }
                            }
                            
                            tmp_type_measure = name;
                            
                            uint8_t _number = (data >> 12) & 0xF;
                            QTextStream(&result) << name << " : " << md << " : " << _number <<"\n\r";
                            qDebug() << result ;
                            
                            
                            break;
                        }
                        case 4:{
                            QString result;
                            int tmp, cnt;
                            
                            QTextStream(&result) << float (data)/m_range->value(tmp_type_measure) << " mg/m3\n\r";
                            
                            
                            tmp = m_data->value(tmp_type_measure, -1); //detect first measure
                            if ( tmp == -1){
                                m_data->insert(tmp_type_measure, QString::number(data, 16).toInt()); // insert into QMap ordering pair of measure first time
                                m_measure->insert(tmp_type_measure, 1);
                                qDebug() << "measure... \n type= " << tmp_type_measure << " value = " << (float)QString::number(data, 16).toInt()/m_range->value(tmp_type_measure) <<"\n\r";
                                
                            } else {
                                m_data->insert(tmp_type_measure, tmp + QString::number(data, 16).toInt() );
                                cnt = m_measure->value(tmp_type_measure, 0);
                                m_measure->insert(tmp_type_measure, cnt+1);
                                qDebug()<<  "measure... \n\r type= " << tmp_type_measure << " value = " << (float)QString::number(data, 16).toInt()/m_range->value(tmp_type_measure)<<"\n\r";
                                
                            }
                            break;
                        }
                            
                        case 5:{
                            QString result, str, md, name;
                            int tmp, cnt;
                            uint8_t _mode = data & 0xFF;
                            
                            
                            if (i < slave.value()-1){ // resourse sensor detection - last register if it's true
                                
                                if (_mode == 2) md = "fault";
                                else
                                {md = (_mode ?  "off" :  "measuring \n\r");};
                                
                                uint8_t _type = (data >> 8) & 0xF;
                                if (_type != 0){
                                    if (_type == 1){ name = "NH3"; //detect type of sensor HARDCODED
                                        if (_mode == 7) md = "change sensor \n\r";
                                        else
                                        {md = (_mode ?  "off" :  "measuring \n\r");};
                                    }
                                } else{
                                    _type = (data ) & 0xF; //in case of only one byte for some equipments
                                    if (_type == 1){ name = "NH3"; //detect type of sensor HARDCODED
                                        
                                    }
                                }
                                tmp_type_measure = name;
                                
                                uint8_t _number = (data >> 12) & 0xF;
                                QTextStream(&result) << name << " : " << md << " : " << _number <<"\n\r";
                                qDebug() << result;
                                
                                
                                
                            } else {
                                
                                QTextStream(&result) << float (data) << " %\n\r";
                                
                                
                                str = "Ресурс сенс. " % tmp_type_measure;
                                tmp = m_data->value(str, -1); //detect first measure
                                
                                if ( tmp == -1){
                                    
                                    m_data->insert(str, QString::number(data, 16).toInt()); // insert into QMap ordering pair of measure first time
                                    m_measure->insert(str, 1);
                                    qDebug() << "measure... \n type= " << str << " value = " << (float)QString::number(data, 16).toInt() << " %\n\r";
                                    
                                } else {
                                    m_data->insert(str, tmp + QString::number(data, 16).toInt());
                                    cnt = m_measure->value(str, 0);
                                    m_measure->insert(str, cnt+1);
                                    qDebug()<<  "measure... \n\r type= " << tmp_type_measure << " value = " << (float)QString::number(data, 16).toInt() << " %\n\r";
                                    
                                }
                            }
                            break;
                        }
                        case 6:{
                            QString result;
                            int tmp, cnt;
                            
                            QTextStream(&result) << float (data)/m_range->value(tmp_type_measure) << " mg/m3\n\r";
                            
                            
                            tmp = m_data->value(tmp_type_measure, -1); //detect first measure
                            if ( tmp == -1){
                                m_data->insert(tmp_type_measure, QString::number(data, 16).toInt()); // insert into QMap ordering pair of measure first time
                                m_measure->insert(tmp_type_measure, 1);
                                qDebug() << "measure... \n\r type= " << tmp_type_measure << " value = " << (float)QString::number(data, 16).toInt()/m_range->value(tmp_type_measure)<< "\n\r";
                                
                            } else {
                                m_data->insert(tmp_type_measure, tmp + QString::number(data, 16).toInt() );
                                cnt = m_measure->value(tmp_type_measure, 0);
                                m_measure->insert(tmp_type_measure, cnt+1);
                                qDebug()<<  "measure... \n\r type= " << tmp_type_measure << " value = " << (float)QString::number(data, 16).toInt()/m_range->value(tmp_type_measure) << "\n\r";
                                
                            }
                            break;
                        }
                            
                        default:
                            break;
                        }
                    }
                }
            }
            else
            {
                QString err;
                
                if( ret < 0 )
                {
                    if(
        #ifdef WIN32
                            errno == WSAETIMEDOUT ||
        #endif
                            errno == EIO
                            )
                    {
                        err += tr( "\n\rI/O error" );
                        err += ": ";
                        err += tr( "did not receive any data from slave.\n\r" );
                        q_poll[slave.key() - 1] --;
                        if (q_poll[slave.key() - 1]==0)
                        {
                            
                            slaveID->replace(slave.key() - 1, false);
                        }
                    }
                    else
                    {
                        err += tr( "Protocol error" );
                        err += ": ";
                        err += tr( "Slave threw exception '" );
                        err += modbus_strerror( errno );
                        err += tr( "' or function not implemented.\n\r" );
                        
                        if (errno == EMBXILADD)  slave.value()--; //in case the number registers is too much
                        q_poll[slave.key() - 1] --;
                        if (q_poll[slave.key() - 1]==0)
                        {
                            
                            slaveID->replace(slave.key() - 1, false);
                        }
                    }
                }
                else
                {
                    err += tr( "Protocol error" );
                    err += ": ";
                    err += tr( "Number of registers returned does not "
                               "match number of registers requested!\n\r" );
                }
                
                if(( err.size() > 0 ) )
                    if (verbose){
                        qDebug()<< QDateTime::currentDateTime().toString( "yyyy-MM-dd hh:mm:ss ") <<"\n\rSlave ID = " << slave.key() << "\n\r"<< err << "\n\r";
                    }
                    else
                    {
                        qDebug() << QDateTime::currentDateTime().toString( "yyyy-MM-dd hh:mm:ss ") <<"\n\rSlave ID = " << slave.key() <<" is not detected.\n\r";
                    }
            }
        }
        j++;
    }
    
    
    //m_mutex->unlock();
}



void processor::resetStatus( void )
{
    qDebug() << tr( "Ready" ) ;
    
}

void processor::pollForDataOnBus( void )
{
    if( m_modbus )
    {
        modbus_poll( m_modbus );
    }
}




void processor::onRtuPortActive(bool active)
{
    if (active) {
        //m_modbus = ui->rtuSettingsWidget->modbus();
        if (m_modbus) {
            //modbus_register_monitor_add_item_fnc(m_modbus, processor::stBusMonitorAddItem);
            // modbus_register_monitor_raw_data_fnc(m_modbus, processor::stBusMonitorRawData);
        }
        m_tcpActive = false;
    }
    else {
        m_modbus = NULL;
    }
}

void processor::busMonitorRawData( uint8_t * data, uint8_t dataLen, bool addNewline )
{
    if( dataLen > 0 )
    {
        QString dump;
        for( int i = 0; i < dataLen; ++i )
        {
            dump += QString().sprintf( "%.2x ", data[i] );
        }
        if( addNewline )
        {
            qDebug().noquote() << "Raw data:  " << dump << "\r\n";
            
            //dump += "\n";
        }
        else {
            qDebug() << "Raw data:  " << dump <<"\n\r";
        }
    }
}


// static
void processor::stBusMonitorRawData( modbus_t * modbus, uint8_t * data, uint8_t dataLen, uint8_t addNewline )
{
    Q_UNUSED(modbus);
    globalApp->_obj->busMonitorRawData( data, dataLen, addNewline != 0 );
}

void processor::onAsciiPortActive(bool active)
{
    if (active) {
        m_modbus = modbus();
        
        if (m_modbus) {
            if(verbose)
            {
                //modbus_register_monitor_add_item_fnc(m_modbus, processor::stBusMonitorAddItem);
                modbus_register_monitor_raw_data_fnc(m_modbus, processor::stBusMonitorRawData);
            }
        }
        m_tcpActive = false;
    }
    else {
        m_modbus = NULL;
    }
}

void processor::onTcpPortActive(bool active)
{
    m_tcpActive = active;
    
    if (active) {
        //m_modbus = ui->tcpSettingsWidget->modbus();
        if (m_modbus) {
            //modbus_register_monitor_add_item_fnc(m_modbus, processor::stBusMonitorAddItem);
            // modbus_register_monitor_raw_data_fnc(m_modbus, processor::stBusMonitorRawData);
        }
    }
    else {
        m_modbus = NULL;
    }
}

void processor::setStatusError(const QString &msg)
{
    qDebug() << msg ;
    
    
}

void processor::renovateSlaveID( void )
{if (m_modbusip || m_modbus)
    {
        memset(q_poll, 24, 30);
        for( int j = 0; j < slaveID->size(); ++j )
        {
            if (!slaveID->value(j))
            {
                slaveID->replace(j, true);
                m_pool->insert (j+1, numCoils); //renovate of registers quantity
            }
        }
    }
    if (m_modbusip)
    {
        if (!m_modbusip->connected){
            
            m_modbusip->~ModbusIP();
            m_modbusip = new ModbusIP(this, &m_modbus_ip, &m_modbus_port);
            connect(m_modbusip, SIGNAL(dataIsReady(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorDataModbus(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            
        }
    }
    
    if (m_gammaet)
    {
        if (!m_gammaet->connected){
            
            m_gammaet->~GammaET();
            m_gammaet = new GammaET(this, &m_gammaet_ip, &m_gammaet_port);
            connect(m_gammaet, SIGNAL(dataIsReady(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorDataModbus(bool*, QMap<QString, int>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
            
        }
    }
    
    
    if (m_dust) {
        //if (!m_dust->connected){
        if ( (m_dust_ip != "") && (m_dust_port >0)){
            m_dust->~DustTcpSock();
            m_dust = new DustTcpSock(this, &m_dust_ip, &m_dust_port);
            m_dust->sendData( "MSTART"); //restart Dust measure equipment
            // }
        }
    }
    
    if (m_ivtm)
    {
        if (!m_ivtm->Connect())
        {
            m_ivtm->~ivtm();
            
            QUrl _url;
            
            _url.setHost(m_ivtm_ip);
            _url.setPort(m_ivtm_port);
            
            m_ivtm = new ivtm(_url, 0, 4, 1, 1000, this );
        }
    }
    
    if (m_meteo) {
        
        if (!m_meteo->connected){
            if ( (m_meteo_ip != "") && (m_meteo_port >0)){
                
                m_meteo->~MeteoTcpSock();
                QSqlQuery *query= new QSqlQuery ("select * from sensors_data where typemeasure = 'Темп. внутренняя' order by date_time desc", *m_conn);
                qDebug() << "Temp. inner status is " <<   query->isActive()<< " and err " << query->lastError().text() << "\n\r";
                query->first();
                QSqlRecord rec = query->record();
                
                float temp_in = (rec.field("measure").value().toFloat());
                query->finish();
                
                QSqlQuery *queryout= new QSqlQuery ("select * from sensors_data where typemeasure = 'Темп. внешняя' order by date_time desc", *m_conn);
                qDebug() << "Temp. inner status is " <<   query->isActive()<< " and err " << query->lastError().text() << "\n\r";
                queryout->first();
                rec = queryout->record();
                
                float temp_out = (rec.field("measure").value().toFloat());
                query->finish();
                
                if (m_meteo_model == ""){
                    
                    m_meteo = new MeteoTcpSock(this, &m_meteo_ip, &m_meteo_port, temp_in, temp_out);
                    if (verbose)
                        m_meteo->verbose = true;
                } else {
                    m_meteo = new MeteoTcpSock(this, &m_meteo_ip, &m_meteo_port, temp_in, temp_out, &m_meteo_model);
                    if (verbose)
                        m_meteo->verbose = true;
                }            }
        }
    }
    
    if (m_ups){
        if ( (m_ups_ip != "") && (m_ups_port >0)){
            
            m_ups->err_count = 0;
        }
    }
    
    if (m_serinus){
        if (!m_serinus->connected)
        {
            if ( (m_serinus_ip != "") && (m_serinus_port >0)){
                
                m_serinus->~Serinus();
                m_serinus = new Serinus(this, &m_serinus_ip, &m_serinus_port);
                connect(m_serinus, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
                
            }
        }
    }
    
    
    if (m_serinus50){
        if (!m_serinus50->connected)
        {
            if ( (m_serinus_ip50 != "") && (m_serinus_port50 >0)){
                
                m_serinus50->~Serinus();
                m_serinus50 = new Serinus(this, &m_serinus_ip, &m_serinus_port, int(50));
                connect(m_serinus50, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
                
            }
        }
    }
    
    
    if (m_serinus55){
        if (!m_serinus55->connected)
        {
            if ( (m_serinus_ip55 != "") && (m_serinus_port55 >0)){
                
                m_serinus55->~Serinus();
                m_serinus55 = new Serinus(this, &m_serinus_ip55, &m_serinus_port55, int(55));
                connect(m_serinus55, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
                
            }
        }
    }
    
    
    if (m_serinus30){
        if (!m_serinus30->connected)
        {
            if ( (m_serinus_ip30 != "") && (m_serinus_port30 >0)){
                
                m_serinus30->~Serinus();
                m_serinus30 = new Serinus(this, &m_serinus_ip30, &m_serinus_port30, int(30));
                connect(m_serinus30, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
                
            }
        }
    }
    
    if (m_serinus44){
        if (!m_serinus44->connected)
        {
            if ( (m_serinus_ip44 != "") && (m_serinus_port44 >0)){
                
                m_serinus44->~Serinus();
                m_serinus44 = new Serinus(this, &m_serinus_ip44, &m_serinus_port44, int(44));
                connect(m_serinus44, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QMap<QString, _status>*))); //fill several data to one sensor's base
                
            }
        }
    }
    

    if (m_grimm)
        m_grimm->reOpen(&m_grimm_ip, &m_grimmport);

    if (m_topasip)
    {
        if (!m_topasip->connected)
        {
            m_topasip->~TopasIP();
            
            
            QString _serialnum = m_topasip->serialnum;
            m_topasip = new TopasIP(this, &m_topas_ip, &m_topas_port, &_serialnum);
            if (m_topasip){
                connect(m_topasip, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*))); //fill several data to one sensor's base
            }
        }
    }

    if (m_envea_so2){
        if (!m_envea_so2->connected)
        {
            if ( (m_envea_ip_so2 != "") && (m_envea_port_so2 >0)){

                m_envea_so2->~enveas();
                m_envea_so2 = new enveas(this, &m_envea_ip_so2, &m_envea_port_so2, &m_envea_name_so2, enveas::SO2);
                m_envea_so2->start();
                connect(m_envea_so2, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_so2->verbose = true;
            }
        }
    }

    if (m_envea_h2s){
        if (!m_envea_h2s->connected)
        {
            if ( (m_envea_ip_h2s != "") && (m_envea_port_h2s >0)){

                m_envea_h2s->~enveas();
                m_envea_h2s = new enveas(this, &m_envea_ip_h2s, &m_envea_port_h2s, &m_envea_name_h2s, enveas::H2S);
                m_envea_h2s->start();
                connect(m_envea_h2s, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_h2s->verbose = true;
            }
        }
    }


    if (m_envea_so2_h2s){
        if (!m_envea_so2_h2s->connected)
        {
            if ( (m_envea_ip_so2_h2s != "") && (m_envea_port_so2_h2s >0)){

                m_envea_so2_h2s->~enveas();
                m_envea_so2_h2s = new enveas(this, &m_envea_ip_so2_h2s, &m_envea_port_so2_h2s, &m_envea_name_so2_h2s, enveas::SO2_H2S);
                m_envea_so2_h2s->start();
                connect(m_envea_so2_h2s, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_so2_h2s->verbose = true;
            }
        }
    }

    if (m_envea_nox){
        if (!m_envea_nox->connected)
        {
            if ( (m_envea_ip_nox != "") && (m_envea_port_nox >0)){

                m_envea_nox->~enveas();
                m_envea_nox = new enveas(this, &m_envea_ip_nox, &m_envea_port_nox, &m_envea_name_nox, enveas::NOx);
                m_envea_nox->start();
                connect(m_envea_nox, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_nox->verbose = true;
            }
        }
    }

    if (m_envea_nox_nh3){
        if (!m_envea_nox_nh3->connected)
        {
            if ( (m_envea_ip_nox_nh3 != "") && (m_envea_port_nox_nh3 >0)){

                m_envea_nox_nh3->~enveas();
                m_envea_nox_nh3 = new enveas(this, &m_envea_ip_nox_nh3, &m_envea_port_nox_nh3, &m_envea_name_nox_nh3, enveas::NOx_NH3);
                m_envea_nox_nh3->start();
                connect(m_envea_nox_nh3, SIGNAL(dataIsReady(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*)), this, SLOT(fillSensorData(bool*, QMap<QString, float>*, QMap<QString, int>*, QString*))); //fill several data to one sensor's base
                if (verbose)
                    m_envea_nox_nh3->verbose = true;
            }
        }
    }
}
void processor::squeezeAlarmMsg()
{
    QMap<QDateTime, QString>::iterator event_iterator;
    QMap<QDateTime, QString>::iterator event_code_iterator;
    if (m_fire){
        if( (m_fire->surgardI->m_event->count() < 10))
        {
            if (m_fire->surgardI->m_event->count() > 1)
            {
                //squeezing of the repeating sequence
                event_code_iterator = m_fire->surgardI->m_event_code->begin();
                
                while ( event_code_iterator != m_fire->surgardI->m_event_code->end())
                { QString val = event_code_iterator.value();
                    
                    event_code_iterator++;
                    if (event_code_iterator!=m_fire->surgardI->m_event_code->end())
                    {
                        if (val == event_code_iterator.value())
                        {
                            m_fire->surgardI->m_event_code->remove(event_code_iterator.key());
                            m_fire->surgardI->m_event->remove(event_code_iterator.key());
                            event_code_iterator--;
                        }
                    }
                }
                
                //pair "E" - "R" squeezing
                event_code_iterator = m_fire->surgardI->m_event_code->begin();
                while ( event_code_iterator != m_fire->surgardI->m_event_code->end())
                {
                    if (event_code_iterator.value().left(1) == "E")
                    {
                        QString str = event_code_iterator.value().mid(1, 3);
                        bool flag = false;
                        
                        for (event_iterator = m_fire->surgardI->m_event_code->begin()+1; event_iterator != m_fire->surgardI->m_event_code->end(); ++event_iterator)
                        {
                            if (event_iterator.value() == QString("R").append(str))
                            {
                                m_fire->surgardI->m_event_code->remove(event_iterator.key()) ;
                                m_fire->surgardI->m_event->remove(event_iterator.key());
                                flag = true;
                                break;
                            }
                        }
                        if (flag){
                            m_fire->surgardI->m_event_code->remove(event_code_iterator.key());
                            m_fire->surgardI->m_event->remove(event_code_iterator.key());
                            event_code_iterator = m_fire->surgardI->m_event_code->begin();
                            
                        }
                        else
                        {
                            event_code_iterator++;
                            
                        }
                    } else{
                        event_code_iterator++;
                    }
                }
            }
            
        }
        else
        {
            
            m_fire->surgardI->m_event->clear();
            m_fire->surgardI->m_event_code->clear();
        }
    }
}
void processor::transactionDB(void)
{
    
    
    QMap<QString, QUuid>::iterator sensor;
    QMap<QDateTime, QString>::iterator event_iterator;
    QMap<QString, float>::iterator meteo_iterator;
    QString _key;
    
    QSqlQuery query = QSqlQuery(*m_conn);
    QSqlQuery query_log = QSqlQuery(*m_conn);
    
    
    int val;
    float average;
    QString tmp_time = QDateTime::currentDateTime().toString( "yyyy-MM-dd hh:mm:ss"); //all SQL INSERTs should be at the same time
    qDebug() << "Transaction time is " << tmp_time;
    //prepare for trouble logging
    query_log.prepare("INSERT INTO logs (date_time, type, descr, idd ) "
                      "VALUES ( :date_time, :type, :descr, :idd)");
    
    //Alarm data reading and filtering
    if (m_fire){
        squeezeAlarmMsg();
        
        for (event_iterator = m_fire->surgardI->m_event->begin(); event_iterator != m_fire->surgardI->m_event->end(); ++event_iterator)
        {
            query.prepare("INSERT INTO fire (idd, typemeasure, surgard, date_time_in) "
                          "VALUES (:idd, :typemeasure, :surgard, :date_time_in)");
            
            query.bindValue(":idd", QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")));
            query.bindValue(":date_time_in", event_iterator.key().toString( "yyyy-MM-dd hh:mm:ss"));
            query.bindValue(":typemeasure", event_iterator.value());
            query.bindValue(":surgard", m_fire->surgardI->m_event_code->value( event_iterator.key()) );
            
            if (!m_conn->isOpen())
                m_conn->open();
            
            if(!m_conn->isOpen())
            {
                qDebug() << "Unable to reopen database connection!\n\r";
            }
            else
            {
                if (verbose)
                {
                    qDebug() << "Transaction status to the Fire Alarm table is " << ((query.exec() == true) ? "successful!" :  "not complete!\n\r");
                    qDebug() << "The last error is " << (( query.lastError().text().trimmed() == "") ? "absent" : query.lastError().text())<<"\n\r";
                }
                else
                {
                    if (query.exec())
                    {
                        qDebug() << "Insertion to the Fire Alarm table is successful!\n\r";
                    }
                    else
                    {
                        qDebug() << "Insertion to the Fire Alarm table is not successful!\n\r";
                        
                    }
                }
                
            }
        }
        query.finish();
        if (m_fire){
            m_fire->surgardI->m_event->clear();
            m_fire->surgardI->m_event_code->clear();
        }
    }
    
    //Meteo data processing
    if (m_meteo) {
        if (m_meteo->sample_t > 0){
            query.prepare("INSERT INTO meteo (station, date_time, bar, temp_in, hum_in, temp_out, hum_out, speed_wind, dir_wind, dew_pt, heat_indx, chill_wind, thsw_indx, rain, rain_rate, uv_indx, rad_solar, et) "
                          "VALUES (:station, :date_time, :bar, :temp_in, :hum_in, :temp_out, :hum_out, :speed_wind, :dir_wind, :dew_pt, :heat_indx, :chill_wind, :thsw_indx, :rain, :rain_rate, :uv_indx, :rad_solar, :et)");
            
            query.bindValue(":station", QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")));
            query.bindValue(":date_time", tmp_time);
            
            QSqlQuery _query_ins;
            
            _query_ins.prepare( "INSERT INTO sensors_data (idd, serialnum, date_time, typemeasure, measure, is_alert) "
                                "VALUES (:idd, :serialnum, :date_time, :typemeasure, :measure, false)");
            _query_ins.bindValue(":idd", QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")));
            _query_ins.bindValue(":date_time", tmp_time);
            
            for (meteo_iterator = m_meteo->measure->begin(); meteo_iterator != m_meteo->measure->end(); ++meteo_iterator)
            {
                float _sin_avrg, _cos_avrg, dir_angle;
                
                if ((meteo_iterator.key() == "dir_wind")||(meteo_iterator.key() == "dir_wind_hi"))
                {
                    
                    _sin_avrg = m_meteo->measure_dir_wind->value("dir_wind_sin") / m_meteo->sample_t;
                    _cos_avrg = m_meteo->measure_dir_wind->value("dir_wind_cos") / m_meteo->sample_t;
                    
                    dir_angle = float(qAsin(qreal(_sin_avrg)));
                    
                    if ((_sin_avrg > 0)&&(_cos_avrg >0 ))
                        dir_angle = dir_angle  * 180 / 3.1415926535f;
                    
                    if ((_sin_avrg  > 0 )&&(_cos_avrg < 0))
                        dir_angle = 180 - dir_angle  * 180 / 3.1415926535f;
                    
                    if ((_sin_avrg < 0)&&(_cos_avrg < 0))
                        dir_angle = 180 + dir_angle  * 180 / 3.1415926535f;
                    
                    if ((_sin_avrg < 0)&&(_cos_avrg > 0))
                        dir_angle = 360 + dir_angle  * 180 / 3.1415926535f;
                    
                    
                    query.bindValue(QString(":").append(meteo_iterator.key()), QString::number(double(dir_angle), 'f', 0));
                    qDebug() << "Мeteo - "<< meteo_iterator.key() << " samples = " << m_meteo->sample_t<< " and value = "<< meteo_iterator.value()/m_meteo->sample_t;
                    
                }
                else
                {
                    qDebug() << "Мeteo - "<< meteo_iterator.key() << " samples = " << m_meteo->sample_t<< " and value = "<< meteo_iterator.value()/m_meteo->sample_t;
                    query.bindValue(QString(":").append(meteo_iterator.key()), meteo_iterator.value()/m_meteo->sample_t);
                }
                
                if (!m_meteo_uuid->value((meteo_iterator.key())).isNull())
                {
                    _query_ins.bindValue(":serialnum",QString( m_meteo_uuid->value(meteo_iterator.key()).toString().remove(QRegExp("[\\{\\}]"))));
                    _query_ins.bindValue(":measure", meteo_iterator.value()/m_meteo->sample_t);
                    
                    _query_ins.bindValue(":typemeasure",m_meteo_types->value(meteo_iterator.key()));
                    
                    if (verbose)
                    {
                        qDebug() << "Transaction status to the main data table with meteo parameter " << m_meteo_types->value(meteo_iterator.key()) <<" is "<< ((_query_ins.exec() == true) ? "successful!" :  "not complete!\n\r");
                        qDebug() << "The last error is " << (( _query_ins.lastError().text().trimmed() == "") ? "absent" : _query_ins.lastError().text()) << "\n\r";
                    }
                    else
                    {
                        if (_query_ins.exec())
                        {
                            qDebug() << "Insertion to the  main data table with meteo parameter is successful!\n\r";
                        }
                        else
                        {
                            qDebug() << "Insertion to the  main data table with meteo parameter is not successful!\n\r";
                            
                        }
                    }
                }
            }
            if (!m_conn->isOpen())
                m_conn->open();
            
            if(!m_conn->isOpen())
            {
                qDebug() << "Unable to reopen database connection!";
            }
            else
            {
                if (verbose)
                {
                    qDebug() << "Transaction status to the meteostation's table is " << ((query.exec() == true) ? "successful!" :  "not complete!\n\r");
                    qDebug() << "The last error is " << (( query.lastError().text().trimmed() == "") ? "absent" : query.lastError().text()) << "\n\r";
                }
                else
                {
                    if (query.exec())
                    {
                        qDebug() << "Insertion to the meteostation's table is successful!\n\r";
                    }
                    else
                    {
                        qDebug() << "Insertion to the meteostation's table is not successful!\n\r";
                        
                    }
                }
                
            }
            
            query.finish();
            if (m_meteo){
                m_meteo->measure->clear();
                m_meteo->measure_dir_wind->clear();
                m_meteo->sample_t = 0;
            }
        } else {
            query_log.bindValue(":date_time", tmp_time );
            query_log.bindValue( ":type", 404 );
            query_log.bindValue(":descr", "Метеокомплекс на посту  " + QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")) + "  не отвечает..."  );
            
            query_log.exec();
            // qDebug() << "Transaction status to the meteostation's table is " << ((query_log.exec() == true) ? "successful!" :  "not complete!");
            // qDebug() << "The last error is " << (( query_log.lastError().text().trimmed() == "") ? "absent" : query_log.lastError().text());
        }
    }
    
    
    //ACA-Liga slow fetch data but too fast comparing with the chromatography processing
    if (m_liga){
        m_liga->getLastResult();
        if (!m_liga->is_read)
        {
            fillSensorData(&m_liga->is_read, m_liga->measure);//copy data from object
            
        } else { if (m_liga->error)
            {
                query_log.bindValue(":date_time", tmp_time );
                query_log.bindValue( ":type", 404 );
                query_log.bindValue(":descr", "Хроматограф на посту  " + QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")) + " работает с ошибкой." + m_liga->status  );
                
                query_log.exec();
                
            }
            
        }
        
    }
    
    //Sensor data processing
    bool is_static = false; // for PM static array indication
    
    for (sensor = m_uuid->begin(); sensor != m_uuid->end(); ++sensor)
    {
        _key = sensor.key();
        
        if ((sensor.key() == "Пыль общая") || (sensor.key() == "PM1") || (sensor.key() == "PM2.5") || (sensor.key() == "PM4") || (sensor.key() == "PM10")||
                (sensor.key() == "Влажн. внешняя пылемера")||(sensor.key() == "Темп. внешняя пылемера"))
        {
            if (m_grimm){//static array for Grimm
                if(sensor.key() == "Пыль общая"){
                    val = ms_data->value("PM", -1); //Hardcoded for Cyrillic name of Dust total
                    _key = "PM";
                } else {
                    val = ms_data->value(sensor.key(), -1);
                    _key = sensor.key();
                }
                is_static = true;
            } else {
                if(sensor.key() == "Пыль общая"){
                    val = m_data->value("PM", -1); //Hardcoded for Cyrillic name of Dust total
                    _key = "PM";
                } else {
                    if (sensor.key() == "Влажн. внешняя пылемера")
                    {
                        val = m_data->value("HUM_PM", -1);
                        _key = "HUM_PM";
                    } else {
                        if (sensor.key() == "Темп. внешняя пылемера")
                        {
                            val = m_data->value("TMP_PM", -1);
                            _key = "TMP_PM";
                        } else {
                            val = m_data->value(sensor.key(), -1);
                            _key = sensor.key();
                        }
                    }
                    
                }
            }
        } else {
            
            val = m_data->value(sensor.key(), -1);
            _key = sensor.key();
        }
        
        
        
        if ((val != -1)&& ((m_measure->value(_key) > 0 ) || is_static) && ((m_status->value(_key)== MEASURING)||(m_status->value(_key)== UNKNOWN))){
            //QSqlQuery query = QSqlQuery(*m_conn);
            int _samples = m_measure->value(sensor.key(), 1);
            
            query.prepare("INSERT INTO sensors_data (idd, serialnum, date_time, typemeasure, measure, is_alert) "
                          "VALUES (:idd, :serialnum, :date_time, :typemeasure, :measure, false)");
            
            if ((sensor.key().indexOf("PM")!= -1) || (sensor.key().indexOf("Пыль общая")!= -1)||
                    (sensor.key().indexOf("HUM_PM")!= -1) || (sensor.key().indexOf("Влажн. внешняя пылемера")!= -1)||
                    (sensor.key().indexOf("TMP_PM")!= -1) || (sensor.key().indexOf("Темп. внешняя пылемера")!= -1))
            {
                //average = (float (val)) / m_measure->value(sensor.key(), 1) / m_range->value(sensor.key()); //for dust measure range is less
                //if (sensor.key() == "Пыль общая")
                // {
                //    average = (float (val)) / m_measure->value("PM", 1) /m_range->value(sensor.key()); //Hardcoded for Cyrillic name of Dust total
                
                //} else {
                
                //     average = (float (val)) / m_measure->value(sensor.key(), 1) / 1000; //for dust measure range is less
                //}
                
                if (is_static)
                {
                    average = (float (val)) / ms_measure->value(_key, 1) / ms_range->value(_key, 1); //for dust measure range from static array
                    _samples = ms_measure->value(_key, 1);
                    
                    ms_data->remove(_key);
                    ms_measure->remove(_key);
                } else {
                    average = (float (val)) / m_measure->value(_key, 1) / m_range->value(_key, 1); //for dust measure range from static array
                    _samples = m_measure->value(_key, 1);
                }
                
            }
            else
            {
                average = (float (val)) / m_measure->value(sensor.key(), 1) /m_range->value(sensor.key());
                
            }
            
            
            
            query.bindValue(":idd", QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")));
            query.bindValue(":serialnum",  QString(m_uuid->value(sensor.key()).toString()).remove(QRegExp("[\\{\\}]")));
            query.bindValue(":date_time", tmp_time);
            query.bindValue(":typemeasure",sensor.key());
            query.bindValue(":measure", average );
            qDebug() << "\n\rTransaction prepare: \n\r idd === "<< QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")) << "\n\r serial === " <<  QString(m_uuid->value(sensor.key()).toString()).remove(QRegExp("[\\{\\}]")) <<
                        "\n\r date_time ===" << QDateTime::currentDateTime().toString( "yyyy-MM-ddThh:mm:ss") << "\n\r typemeasure " <<  sensor.key() <<
                        "\n\r measure ===" <<average << " and samples === " << _samples <<"\n\r";
            if (!m_conn->isOpen())
                m_conn->open();
            
            if(!m_conn->isOpen())
            {
                qDebug() << "Unable to reopen database connection!\n\r";
            }
            else
            {
                if (verbose)
                {
                    qDebug() << "Transaction status is " << ((query.exec() == true) ? "successful!\n\r" :  "not complete! \n\r");
                    qDebug() << "The last error is " << (( query.lastError().text().trimmed() == "") ? "absent " : query.lastError().text()) << "\n\r";
                }
                else
                {
                    if (query.exec())
                    {
                        qDebug() << "Insertion is successful!\n\r";
                    }
                    else
                    {
                        qDebug() << "Insertion is not successful!\n\r";
                        
                    }
                }
                query.finish();
                //  query.~QSqlQuery();
                
                m_data->remove(sensor.key());
                m_measure->remove(sensor.key());
                
                
            }
            
        }
        else { //logging error
            QMetaEnum metaEnum = QMetaEnum::fromType<processor::_status>();
            
            if ((val == -1)&&(m_status->value(_key)!= ABSENT)) {//if measure is absend
                
                query_log.bindValue(":date_time", tmp_time );
                query_log.bindValue( ":type", 404 );
                query_log.bindValue(":descr", "Сенсор " + _key + "     " + QString(m_uuid->value(sensor.key()).toString()).remove(QRegExp("[\\{\\}]"))+"  на посту " + QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")) + " не отвечает. Статус сенсора - "
                                    + metaEnum.valueToKey( m_status->value(_key)));
                query_log.bindValue( ":idd", QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")) );
                query_log.exec();
            }
            
            if (val != -1)
            {
                //if instruments measure but not ready
                
                query_log.bindValue(":date_time", tmp_time );
                query_log.bindValue( ":type", 404 );
                query_log.bindValue(":descr", "Сенсор " + _key + "     " + QString(m_uuid->value(sensor.key()).toString()).remove(QRegExp("[\\{\\}]"))+"  на посту " + QString(m_uuidStation->toString()).remove(QRegExp("[\\{\\}]")) + " не отвечает. Статус серсора - "
                                    + metaEnum.valueToKey( m_status->value(_key)));
                
                query_log.exec();
                
            }
            
        }
        
        
    }
    query_log.finish();
    query.finish();
    m_data->clear();
    m_measure->clear();

    //send to GGO via external REST API

    if (ggo && (m_threadPool->activeThreadCount() == 0))
    {
        QSqlDatabase _conn;

        if (QSqlDatabase::contains( QString("weather_ggo")))
        {
            {

                {
                    QSqlDatabase _temp_db = QSqlDatabase::database( QString("weather_ggo"));
                    _temp_db.close();
                    qDebug() << "DB open = " <<_temp_db.isOpen() <<"\n\r";

                }
                QSqlDatabase::removeDatabase(QString("weather_ggo"));
                _conn = QSqlDatabase::cloneDatabase(*m_conn, QString("weather_ggo"));

            }
        } else {
            _conn = QSqlDatabase::cloneDatabase(*m_conn, QString("weather_ggo"));
        }

        QDateTime local(QDateTime::currentDateTime());
        QDateTime UTC(local.toUTC());
        qint64 tz = QDateTime::currentDateTime().offsetFromUtc()/3600; //time zone
        QString o = QString("+").append((QDateTime::currentDateTime().offsetFromUtc()/3600 < 10) ? (QString("0").append(QString::number( QDateTime::currentDateTime().offsetFromUtc()/3600))) : (QString::number(QDateTime::currentDateTime().offsetFromUtc()/3600 )));
        
        qDebug() << "Rest API call is started...\n\r";
        
        
        QString select_st = "SELECT * FROM injection WHERE is_present = true and id = 2"; //id is hardcoded to 2 position
        
        QSqlQuery *query_inj = new QSqlQuery(*m_conn);
        QSqlRecord rs_station;
        
        query_inj->prepare(select_st);
        query_inj->exec();
        
        //begin work
        if(!query_inj->lastError().isValid())
        {
            while (query_inj->next()) {
                rs_station = query_inj->record();
                
                QString uri = QString(rs_station.value("uri").toString());
                QString token = QString(rs_station.value("token").toString());
                //QString locality = QString(rs_station.value("locality").toString());
                QString indx = QString(rs_station.value("indx").toString());
                QString idd = QString(rs_station.value("idd").toString());
                int code = rs_station.value("code").toInt();
                double msg_id = rs_station.value("msg_id").toDouble();
                int msg_id_out = rs_station.value("msg_id_out").toInt();
                
                
                QString begin_t = QString(rs_station.value("date_time").toString());
                QDateTime _begin_t = QDateTime::fromString(begin_t.left(19),"yyyy-MM-ddTHH:mm:ss");
                
                QString last_t = QString(rs_station.value("last_time").toString());
                QDateTime _last_t = QDateTime::fromString(last_t.left(19),"yyyy-MM-ddTHH:mm:ss");
                
                QString curr_t = QDateTime(QDateTime::currentDateTime()).toString("yyyy-MM-ddThh:mm:ss");
                QDateTime _curr_t = QDateTime::fromString(curr_t.left(19),"yyyy-MM-ddTHH:mm:ss");
                
                QDateTime _from_t = _begin_t.addSecs(1);
                
                if (_curr_t.addDays(-1) > _begin_t)
                    _from_t = _curr_t.addDays(-1);
                
                QDateTime __curr_t = _curr_t.addSecs(1);
                qcollectorc *_qcollector = new qcollectorc(&_conn, idd, _from_t, __curr_t, _last_t, uri, code, token,  indx, msg_id, msg_id_out, this);
                
                m_threadPool->start(_qcollector);
            }
        }
    }

    //end section of external REST API call
    
    
}
void processor::startTransactTimer( QSqlDatabase *conn) //start by signal dbForm
{
    
    
    //m_conn = conn;
    QSqlQuery *query= new QSqlQuery ("select * from equipments where is_present = 'true' and measure_class = 'data'"
                                     "and idd = (select idd from stations where is_present = 'true') order by date_time_in", *conn);
    qDebug() << "Try to add data UUID sensors... status is " <<   query->isActive()<< " and err " << query->lastError().text() << "\n\r";
    query->first();
    QSqlRecord rec = query->record();
    
    if (!m_transactTime){
        m_transactTimer->start(rec.field("average_period").value().toInt() *1000);
    } else {
        m_transactTimer->start(*m_transactTime * 1000);
    }
    
    if( !m_pollTimer->isActive() )
    {
        m_transactTimer->stop();
    }
    
    m_uuidStation  = new QUuid(rec.field("idd").value().toUuid());
    
    
    for (int i = 0; i < query->size(); i++ )
    {
        qDebug() << query->value("typemeasure").toString() << "  -----  "<< query->value("serialnum").toUuid() <<"\n\r";
        
        m_uuid->insert( query->value("typemeasure").toString(), query->value("serialnum").toUuid());
        
        query->next();
    }
    query->finish();
    
    //meteo sensors UUID loading
    
    *query=  QSqlQuery ("select * from equipments where is_present = 'true' and measure_class <> 'data' and measure_class <> 'alert'"
                        "and idd = (select idd from stations where is_present = 'true') order by date_time_in", *conn);
    rec.clear();
    
    qDebug() << "Try to add UUID meteosensors... status is " <<   query->isActive() << " and err " << query->lastError().text() << "\n\r";
    
    query->first();
    rec = query->record();
    
    for (int i = 0; i < query->size(); i++ )
    {
        qDebug() << query->value("typemeasure").toString() << "  -----  "<< query->value("serialnum").toUuid() <<"\n\r";
        
        m_meteo_uuid->insert( query->value("measure_class").toString(), query->value("serialnum").toUuid());
        m_meteo_types->insert(query->value("measure_class").toString(), query->value("typemeasure").toString());
        
        query->next();
    }
    
    query->finish();
    rec.clear();
}

void  processor::initStatus()
{
    
    if ((m_modbusip))
    {
        m_status->insert("CO", UNKNOWN);
        m_status->insert("NO2", UNKNOWN);
        m_status->insert("NO", UNKNOWN);
        
        m_status->insert("O3", UNKNOWN);
        m_status->insert("NH3", UNKNOWN);
        m_status->insert("CH2O", UNKNOWN);
        
    }
    
    if ((m_serinus44))
    {
        m_status->insert("NO2", UNKNOWN);
        m_status->insert("NO", UNKNOWN);
        m_status->insert("NH3", UNKNOWN);
        m_status->insert("NOx", UNKNOWN);
        
    }
    
    if (m_serinus30){
        m_status->insert("CO", UNKNOWN);
        
    }
    
    if (m_serinus50)
        m_status->insert("SO2", UNKNOWN);

    if (m_serinus55)
        m_status->insert("H2S", UNKNOWN);

    if (m_serinus){
        m_status->insert("SO2", UNKNOWN);
        m_status->insert("H2S", UNKNOWN);
    }
    
    
    
    if (m_liga){
        m_status->insert("бензол", UNKNOWN);
        m_status->insert("толуол", UNKNOWN);
        m_status->insert("этилбензол", UNKNOWN);
        m_status->insert("м,п-ксилол", UNKNOWN);
        m_status->insert("о-ксилол", UNKNOWN);
        m_status->insert("хлорбензол", UNKNOWN);
        m_status->insert("стирол", UNKNOWN);
        m_status->insert("фенол", UNKNOWN);
    }

    if (m_ups){
        m_status->insert("Напряжение мин.", UNKNOWN);
        m_status->insert("Напряжение макс.", UNKNOWN);
    }
    
    if (m_topasip || m_dust || m_grimm){
        m_status->insert("PM", UNKNOWN);
        m_status->insert("PM1", UNKNOWN);
        m_status->insert("PM2.5", UNKNOWN);
        m_status->insert("PM4", UNKNOWN);
        m_status->insert("PM10", UNKNOWN);
        
        
    }
    
    
    //GammaET elements
    if (m_gammaet)
    {
        m_status->insert("CH", UNKNOWN);
        m_status->insert("HCH", UNKNOWN);
        m_status->insert("CH4", UNKNOWN);
    }

    //ENVEA
    if (m_envea_nox)
    {
        m_status->insert("NO2", UNKNOWN);
        m_status->insert("NO", UNKNOWN);
        m_status->insert("NH3", UNKNOWN);
        m_status->insert("NOx", UNKNOWN);
    }

    if (m_envea_so2)
        m_status->insert("SO2", UNKNOWN);

    if (m_envea_h2s)
        m_status->insert("H2S", UNKNOWN);

    if (m_envea_nox_nh3)
    {
        m_status->insert("SO2", UNKNOWN);
    }

    if (m_envea_so2_h2s)
    {
        m_status->insert("SO2", UNKNOWN);
        m_status->insert("H2S", UNKNOWN);
    }

}

//Read the status of devices that are connected via TCP
void processor::readSocketStatus()
{
    
    
    QString tmp_type_measure;
    QStringList dust = {"PM1", "PM2.5", "PM4", "PM10", "PM"  };
    int tmp;
    
    
    int j = 0;
    QMap<int, int>::iterator slave;
    //if( m_tcpActive )
    //  ui->tcpSettingsWidget->tcpConnect();
    
    
    // MODBUS ip
    if (m_modbusip){
        for (slave = m_pool->begin(); slave != m_pool->end(); ++slave)
        {
            //tmp_type_measure.clear();
            int _key = slave.key();
            int _reg = slave.value();
            m_modbusip->sendData( _key, _reg);
            //m_modbusip->m_address = slave.key();
            //m_modbusip->m_registers = slave.value();
            //m_threadPoolSlowProcess->start(m_modbusip);
        }
    }
    
    // GammaET MODBUS ip
    if (m_gammaet){
        for (slave = m_gammapool->begin(); slave != m_gammapool->end(); ++slave)
        {
            //tmp_type_measure.clear();
            m_gammaet->sendData(slave.key(), slave.value());
            
        }
    }
    
    
    //Dust data reading
    if (m_dust){
        if (m_dust->connected){
            m_dust->sendData( "RMMEAS");
            //while (!m_dust->is_read);
            m_dust->is_read = false;
            //qDebug() << "count " << dust.length() <<"\n\r";
            
            for (int i = 0; i < dust.length(); i++)
            {
                tmp_type_measure = dust[i];
                tmp = m_data->value(tmp_type_measure, -1); //detect first measure
                
                if ( tmp == -1)
                {
                    if (m_dust->measure->value(tmp_type_measure) >0)
                    {
                        m_data->insert(tmp_type_measure, m_dust->measure->value(tmp_type_measure)); // insert into QMap ordering pair of measure first time
                        m_measure->insert(tmp_type_measure, 1);
                        if(verbose)
                            qDebug() << "measure... " << tmp_type_measure << " value = " << (float)m_dust->measure->value(tmp_type_measure)/m_range->value(tmp_type_measure) <<"\n\r";
                    }
                } else {
                    if (m_dust->measure->value(tmp_type_measure) >0)
                    {
                        m_data->insert(tmp_type_measure, tmp + m_dust->measure->value(tmp_type_measure));
                        m_measure->insert(tmp_type_measure, m_measure->value(tmp_type_measure, 0) +1); //increment counter
                        if(verbose)
                            qDebug() << "measure... " << tmp_type_measure << " value = " << (float)m_dust->measure->value(tmp_type_measure)/m_range->value(tmp_type_measure) <<"\n\r";
                    }
                }
            }
            
            // }
            
            m_dust->measure->clear();
        }
    }
    //Meteostation data reading
    if (m_meteo) {
        if (m_meteo->connected){
            if (m_meteo->model){
                m_meteo->sendData("E0");
            } else {
                m_meteo->sendData("LPS 2 1");//sendData("LOOP 1");
            }
            
        }
        
        // IVTM data reading with gamma function as callback method meteo data filling
        
        if (m_ivtm){
            
            m_ivtm->doQuery( [this](const QString & _key, const float & _val){
                this->m_meteo->push_data(_key, _val);
                if (!this->m_meteo->sample_t )
                    this->m_meteo->sample_t = 1;
            });
        }
    }
    //m_liga->getLastResult();
    
    //UPS acqusition data reading
    if (m_ups){
        if (m_ups->err_count <10){ //minimum error threshold
            m_ups->read_voltage();
            if (!m_measure->value("Напряжение мин."))
            {   m_data->insert("Напряжение мин.", m_ups->voltage);
                m_measure->insert("Напряжение мин.", 1); //we don't interested in average voltage - we need lowest or highest values
                
            }
            
            if (!m_measure->value("Напряжение макс."))
            {   m_data->insert("Напряжение макс.", m_ups->voltage);
                m_measure->insert("Напряжение макс.", 1); //we don't interested in average voltage - we need lowest or highest values
                
            }
            if (m_ups) {
                if (m_ups->voltage < m_data->value("Напряжение мин.")){
                    m_data->insert("Напряжение мин.", m_ups->voltage);
                }
                
                if (m_ups->voltage > m_data->value("Напряжение макс.")){
                    m_data->insert("Напряжение макс.", m_ups->voltage);
                }
            }
        }
    }
    //Alarm data reading
    if (m_fire) {
        
        qDebug() << "Alarm messages is  "<< m_fire->surgardI->m_event->count() <<"\n\r";
    }
    // Read Serinus status
    if (m_serinus) {
        if (m_serinus->connected)
        {   m_serinus->readStatus();
            m_serinus->readGases(2);
            if(verbose)
                
                qDebug()<< "\n\rSerinus 51 read command sent\n\r";
            
            
        }
    }
    
    if (m_serinus50) {
        if (m_serinus50->connected)
        {   m_serinus50->readStatus();
            m_serinus50->readGases(1);
            if(verbose)
                
                qDebug()<< "\n\rSerinus 50 read command sent\n\r" ;
            
            
        }
    }
    
    if (m_serinus55) {
        if (m_serinus55->connected)
        {
            m_serinus55->readStatus();
            m_serinus55->readGases(1);
            if(verbose)
                
                qDebug()<< "\n\rSerinus 55 read command sent\n\r" ;
            
            
        }
    }
    
    if (m_serinus30) {
        if (m_serinus30->connected)
        {
            m_serinus30->readStatus();
            m_serinus30->readGases(1);
            if(verbose)
                
                qDebug()<< "\n\rSerinus 30 read command sent\n\r" ;
            
            
        }
    }
    
    if (m_serinus44) {
        if (m_serinus44->connected)
        {
            m_serinus44->readStatus();
            m_serinus44->readGases(4);
            
            if(verbose)
                
                qDebug()<< "\n\rSerinus 44  read command sent\n\r" ;
            
            
        }
    }
    
    
    if (m_topasip)
    {
        if (m_topasip->lastCommand == "")
        {
            m_topasip->startSample();
        }
        m_topasip->readSample();
        //m_topasip->readSample();
    }
    
    if (m_envea_so2)
    {
        m_envea_so2->readGases(1);
    }

    if (m_envea_h2s)
    {
        m_envea_h2s->readGases(1);
    }

    if (m_envea_so2_h2s)
    {
        m_envea_so2_h2s->readGases(1);
    }

    if (m_envea_nox)
    {
        m_envea_nox->readGases(1);
    }

    if (m_envea_nox_nh3)
    {
        //m_envea_nox_nh3->readGases(1);
        m_envea_nox_nh3->readGasesEx();
    }
}

void processor::fillSensorData( bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample,  QString *__status)
{
    QMap<QString, float>::iterator sensor;

    QString status_local = *__status;

    for (sensor = _measure->begin(); sensor != _measure->end(); ++sensor)
    {
        if (_sample->value(sensor.key())>0)
        {
            m_data->insert(sensor.key(), int(_measure->value(sensor.key()) *m_range->value(sensor.key())) + m_data->value(sensor.key()));
            m_measure->insert(sensor.key(), m_measure->value(sensor.key()) + _sample->value(sensor.key()));

            QMetaObject metaObject = processor::staticMetaObject;
            QMetaEnum metaEnum = metaObject.enumerator(metaObject.indexOfEnumerator("_status"));

            _status enumValue = static_cast<_status>(metaEnum.keyToValue(status_local.toLatin1()));

            m_status->insert(sensor.key(), enumValue);

            _measure->insert(sensor.key(), 0 );
            _sample->insert(sensor.key(), 0);
        }

    }
    *_is_read = true;

}

void processor::fillSensorData( bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample,  QMap<QString, _status> *_status)
{
    QMap<QString, float>::iterator sensor;
    
    
    
    for (sensor = _measure->begin(); sensor != _measure->end(); ++sensor)
    {
        if (_sample->value(sensor.key())>0)
        {
            m_data->insert(sensor.key(), int(_measure->value(sensor.key()) *m_range->value(sensor.key())) + m_data->value(sensor.key()));
            m_measure->insert(sensor.key(), m_measure->value(sensor.key()) + _sample->value(sensor.key()));
            m_status->insert(sensor.key(), _status->value(sensor.key()));
            
            //  _measure->insert(sensor.key(), 0 );
            // _sample->insert(sensor.key(), 0);
        }
        
    }
    *_is_read = true;
    
}

void processor::fillSensorData( bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample)
{
    QMap<QString, float>::iterator sensor;
    
    
    
    for (sensor = _measure->begin(); sensor != _measure->end(); ++sensor)
    {
        if (_sample->value(sensor.key())>0)
        {
            m_data->insert(sensor.key(), int(_measure->value(sensor.key()) *m_range->value(sensor.key())) + m_data->value(sensor.key()));
            m_measure->insert(sensor.key(), m_measure->value(sensor.key()) + _sample->value(sensor.key()));
            
        }
        
    }
    *_is_read = true;
    
}

void processor::fillSensorData( bool *_is_read, QMap<QString, float> *_measure)
{
    QMap<QString, float>::iterator sensor;
    
    
    
    for (sensor = _measure->begin(); sensor != _measure->end(); ++sensor)
    {
        
        m_data->insert(sensor.key(), int(_measure->value(sensor.key()) *m_range->value(sensor.key())) );
        m_measure->insert(sensor.key(), 1);
        
    }
    *_is_read = true;
    
}

void processor::static_fillSensorData(bool *_is_read, QMap<QString, float> *_measure, QMap<QString, int> *_sample)
{
    QMap<QString, float>::iterator sensor;
    
    
    
    for (sensor = _measure->begin(); sensor != _measure->end(); ++sensor)
    {
        
        ms_data->insert(sensor.key(),  ms_data->value(sensor.key()) + int(_measure->value(sensor.key()) *ms_range->value(sensor.key())) );
        ms_measure->insert(sensor.key(), ms_measure->value(sensor.key()) + 1);
        _measure->insert(sensor.key(), 0 );
        _sample->insert(sensor.key(), 0);
    }
    *_is_read = true;
    
}

void processor::fillSensorDataModbus( bool *_is_read, QMap<QString, int> *_measure, QMap<QString, int> *_sample, QMap<QString, _status> *_status)
{
    QMap<QString, int>::iterator sensor;
    
    
    
    for (sensor = _measure->begin(); sensor != _measure->end(); ++sensor)
    {
        if (_sample->value(sensor.key())>0)
        {
            m_data->insert(sensor.key(), _measure->value(sensor.key())  + m_data->value(sensor.key()) );
            m_measure->insert(sensor.key(), m_measure->value(sensor.key()) + _sample->value(sensor.key()));
            m_status->insert(sensor.key(), _status->value(sensor.key()));
            _measure->insert(sensor.key(), 0 );
            _sample->insert(sensor.key(), 0);
            
            
        }
        
    }
    *_is_read = true;
    
}

void processor::fillSensorDataModbus( bool *_is_read, QMap<QString, int> *_measure, QMap<QString, int> *_sample)
{
    QMap<QString, int>::iterator sensor;
    
    
    
    for (sensor = _measure->begin(); sensor != _measure->end(); ++sensor)
    {
        if (_sample->value(sensor.key())>0)
        {
            m_data->insert(sensor.key(), _measure->value(sensor.key())  + m_data->value(sensor.key()) );
            m_measure->insert(sensor.key(), m_measure->value(sensor.key()) + _sample->value(sensor.key()));
            _measure->insert(sensor.key(), 0 );
            _sample->insert(sensor.key(), 0);
            
        }
        
    }
    *_is_read = true;
    
}

