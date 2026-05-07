/*
 * @Author: MrPan
 * @Date: 2026-03-25 16:01:56
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-07 14:03:35
 * @Description: 请填写简介
 */
#ifndef COMMANDHELPER_H
#define COMMANDHELPER_H

#include <QObject>
#include "tcpclient.h"

struct CommandItem
{
    QString name;      // 指令名称（中文或英文描述）
    QByteArray data;   // 实际发送的指令内容

    CommandItem() {}
    CommandItem(const QString &n, const QByteArray &d)
        : name(n), data(d) {}
};

class CommandHelper : public QObject
{
    Q_OBJECT
public:
    explicit CommandHelper(QObject *parent = nullptr);

    void testSend();
    
    // 继电器控制,电源开
    void PowerOnRelay();
    // 继电器控制,电源关
    void PowerOffRelay();
    
    // 连接继电器网络
    void connectRelay();
    // 关闭继电器网络
    void disconnectRelay();

    void openDetector();
    void closeDetector();

private:
    // 初始化常用指令
    void initCommand(); 
    
    // 读取网络配置，IP和port
    void loadIPConfig();

    // 触发阈值
    QByteArray getCmdTriggerThold(quint16 ch1, quint16 ch2);

signals:
    //更新界面日志
    void sigAppendMsg(const QString &msg, QtMsgType msgType);
    // 继电器状态
    void sigRelayStatus(bool on);
    void sigRelayConnectError(QAbstractSocket::SocketError error);//故障，一般指网络不通
    void sigRelayPowerStatus(bool on);//继电器控制的电源状态

    // 探测器状态
    void sigDetector1Status(bool on);
    void sigDetector2Status(bool on);
    void sigDetector1Fault();//故障，一般指网络不通
    void sigDetector2Fault();

public slots:
    // 继电器数据处理
    void handleRelayData(const QByteArray &binaryData);

private:
    TcpClient* client_det1; //FPGA板1
    TcpClient* client_det2; //FPGA板2
    TcpClient* client_relay; //继电器

    QString ip_det1;
    QString ip_det2;
    QString ip_relay;
    quint16 port_det1;
    quint16 port_det2;
    quint16 port_relay;
    
    QByteArray cmdSoftTrigger;//软件触发模式，开始测量
    QVector<CommandItem> cmdPool; //常用指令池，可以根据需要添加更多指令
};

#endif // COMMANDHELPER_H
