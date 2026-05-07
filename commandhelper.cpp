/*
 * @Author: Maoxiaoqing
 * @Date: 2026-03-25 16:01:56
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-07 14:03:16
 * @Description: 请填写简介
 */
#include "commandhelper.h"
#include "globalsettings.h"

CommandHelper::CommandHelper(QObject *parent)
    : QObject{parent}
{
    loadIPConfig();
    initCommand();
    
    client_det1 = new TcpClient(this); //FPGA板1
    client_det2 = new TcpClient(this); //FPGA板2
    client_relay = new TcpClient(this); //继电器
    
    //状态改变
    connect(client_det1, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            qInfo() << "FPGA板1连接成功";
            emit sigAppendMsg("FPGA板1连接成功\n", QtInfoMsg);
            emit sigDetector1Status(true);
        } else {
            qWarning() << "FPGA板1断开连接";
            emit sigAppendMsg("FPGA板1断开连接\n", QtWarningMsg);
        }
    });

    connect(client_det2, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            qInfo() << "FPGA板2连接成功";
            emit sigAppendMsg("FPGA板2连接成功\n", QtInfoMsg);
            emit sigDetector2Status(true);
        } else {
            qWarning() << "FPGA板2断开连接";
            emit sigAppendMsg("FPGA板2断开连接\n", QtWarningMsg);
        }
    });

    connect(client_relay, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            //查询继电器状态指令
            QByteArray cmdStatusQuery = QByteArray::fromHex("48 3a 01 53 00 00 00 00 00 00 00 00 d6 45 44"); 
            client_relay->send(cmdStatusQuery);

            emit sigRelayStatus(true);
        } else {
            emit sigRelayStatus(false);
        }
    });

    connect(client_relay, &TcpClient::dataReceived, this, &CommandHelper::handleRelayData);

    // 连接失败
    connect(client_det1, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        // qWarning() << "FPGA板1连接失败:" << error;
        // emit sigAppendMsg(QString("FPGA板1连接失败: %1\n").arg(error), QtWarningMsg);
        emit sigDetector1Fault();
    });

    connect(client_det2, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        // qWarning() << "FPGA板2连接失败:" << error;
        // emit sigAppendMsg(QString("FPGA板2连接失败: %1\n").arg(error), QtWarningMsg);
        emit sigDetector2Fault();
    });

    connect(client_relay, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        emit sigRelayConnectError(error);
    });
}

void CommandHelper::openDetector()
{
    loadIPConfig(); //确保使用最新的网络配置
    client_det1->connectToHost(ip_det1, port_det1);
    // client_det2->connectToHost(ip_det2, port_det2);
}

void CommandHelper::closeDetector()
{
    client_det1->disconnectFromHost();
    // client_det2->disconnectFromHost();
}

void CommandHelper::connectRelay()
{
    loadIPConfig(); //确保使用最新的网络配置
    client_relay->connectToHost(ip_relay, port_relay);
}

void CommandHelper::disconnectRelay()
{
    client_relay->disconnectFromHost();
}

// 继电器开启电源
void CommandHelper::PowerOnRelay()
{
    if (client_relay) {
        QByteArray cmdPowerOn = QByteArray::fromHex("48 3a 01 57 01 01 00 00 00 00 00 00 dc 45 44"); 
        client_relay->send(cmdPowerOn);
    }
}

// 继电器关闭电源
void CommandHelper::PowerOffRelay()
{
    if (client_relay) {
        QByteArray cmdPowerOff = QByteArray::fromHex("48 3a 01 57 00 00 00 00 00 00 00 00 da 45 44"); 
        client_relay->send(cmdPowerOff);
    }
}

void CommandHelper::testSend()
{
    QByteArray data = QByteArray::fromHex("12 34 00 0A DA 11");
    client_relay->send(data);
}

void CommandHelper::initCommand()
{
    // 初始化常用指令
    cmdSoftTrigger = QByteArray::fromHex("12 34 00 0A DA 11");
    cmdPool.append(CommandItem("软件触发", cmdSoftTrigger));
}

// 读取网络配置，IP和port
// 注意要在每次连接前调用，以确保使用最新的网络配置
void CommandHelper::loadIPConfig()
{
    JsonSettings* settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    // 网络配置读取
    ip_det1 = settings->getValueByPath("network/ip1").toString();
    ip_det2 = settings->getValueByPath("network/ip2").toString();
    ip_relay = settings->getValueByPath("network/ip_relay").toString();

    port_det1 = settings->getValueByPath("network/port_det1").toUInt();
    port_det2 = settings->getValueByPath("network/port_det2").toUInt();
    port_relay = settings->getValueByPath("network/port_relay").toUInt();
}

void CommandHelper::handleRelayData(const QByteArray &binaryData)
{
    qDebug() << "Received relay data:" << binaryData.toHex(' ');
    if (binaryData.size() != 15) {
        return;
    }

    const quint8 *data = reinterpret_cast<const quint8 *>(binaryData.constData());
    if (data[0] != 0x48 || data[1] != 0x3a || data[2] != 0x01 || data[3] != 0x54) {
        return;
    }

    const bool relayOff = (data[4] == 0x00 && data[5] == 0x00);
    const bool relayOn = (data[4] == 0x01 && data[5] == 0x01);
    if (relayOff) {
        emit sigRelayPowerStatus(false);
    } else if (relayOn) {
        emit sigRelayPowerStatus(true);
    }
}
