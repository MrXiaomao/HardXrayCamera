/*
 * @Author: Maoxiaoqing
 * @Date: 2026-03-25 16:01:56
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-03-25 16:51:23
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
            qInfo() << "继电器连接成功";
            emit sigAppendMsg("继电器连接成功\n", QtInfoMsg);
            emit sigRelayStatus(true);
        } else {
            qWarning() << "继电器断开连接";
            emit sigAppendMsg("继电器断开连接\n", QtWarningMsg);
        }
    });

    // 连接失败
    connect(client_det1, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        qWarning() << "FPGA板1连接失败:" << error;
        emit sigAppendMsg(QString("FPGA板1连接失败: %1\n").arg(error), QtWarningMsg);
    });

    connect(client_det2, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        qWarning() << "FPGA板2连接失败:" << error;
        emit sigAppendMsg(QString("FPGA板2连接失败: %1\n").arg(error), QtWarningMsg);
    });

    connect(client_relay, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        qWarning() << "继电器连接失败:" << error;
        emit sigAppendMsg(QString("继电器连接失败: %1\n").arg(error), QtWarningMsg);
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

void CommandHelper::openRelay(bool first)
{
    loadIPConfig(); //确保使用最新的网络配置
    client_relay->connectToHost(ip_relay, port_relay);
}

void CommandHelper::closeRelay()
{
    client_relay->disconnectFromHost();
}

void CommandHelper::testSend()
{
    QByteArray data = QByteArray::fromHex("12 34 00 0A DA 11");
    client_det1->send(data);
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