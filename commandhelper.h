/*
 * @Author: MrPan
 * @Date: 2026-03-25 16:01:56
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-19 15:21:40
 * @Description: 请填写简介
 */
#ifndef COMMANDHELPER_H
#define COMMANDHELPER_H

#include <QObject>
#include "tcpclient.h"
#include "globalsettings.h"
struct CommandItem
{
    QString name;      // 指令名称（中文或英文描述）
    QByteArray data;   // 实际发送的指令内容

    CommandItem() {}
    CommandItem(const QString &n, const QByteArray &d)
        : name(n), data(d) {}
};

enum saveFileFormat{
    Binary,
    Text
};
class CommandHelper : public QObject
{
    Q_OBJECT
public:
    explicit CommandHelper(QObject *parent = nullptr);

    void testSend();
    
    // 连接继电器网络
    void connectRelay();
    // 关闭继电器网络
    void disconnectRelay();
    
    // 发送指令到FPGA板1
    // void sendFPGA1(const QByteArray& data);
    // void sendFPGA2(const QByteArray& data);

    void connectDetector();
    void disconnectDetector();

    void connectARM();
    void disconnectARM();

    // 继电器控制,电源开
    void PowerOnRelay();
    
    // 继电器控制,电源关
    void PowerOffRelay();

    void startMeasure(DetParameter detPara);

    void stopMeasure();
    
    // 文件存储格式
    void setSaveFileFormat(saveFileFormat format) { mfileFormat = format; }

    void setSavePath(const QString& path) { mSavePath = path; }

private:
    // 初始化常用指令
    void initCommand(); 

    // FPGA1初始化指令
    void initFPGA1Commands();

    // FPGA2初始化指令
    void initFPGA2Commands();
    
    // 读取网络配置，IP和port
    void loadIPConfig();

    // 触发阈值
    QByteArray getCmdTriggerThold(quint16 ch1, quint16 ch2);

    // 发送指令并打印十六进制、指令名称和参数
    void sendCommand(TcpClient* client, const QByteArray& command,
                     const QString& name, const QString& parameter = QString());

    void processSpec512Data(int detectorIndex, QByteArray& buffer, const QByteArray& data);
    void processWaveformData(int detectorIndex, QByteArray& buffer, const QByteArray& data);
    bool parseSpectrumPacket(int detectorIndex, const QByteArray& packet);

signals:
    // 波形数据: timeUnits 单位为 500us, samples 为 1024 个采样点
    void sigWaveformData(int detectorIndex, int channelNumber, quint32 timeUnits,
                         const QVector<quint16>& samples);

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

    // ARM状态
    void sigARM1Status(bool on);
    void sigARM2Status(bool on);
    void sigARM1Fault();//故障，一般指网络不通
    void sigARM2Fault();

    // 能谱数据
    void sigSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                         const QVector<quint32>& counts);

public slots:
    // 继电器数据处理
    void handleRelayData(const QByteArray &binaryData);
    void handleDet1Data(const QByteArray &binaryData);
    void handleDet2Data(const QByteArray &binaryData);
    void handleARM1Data(const QByteArray &binaryData);
    void handleARM2Data(const QByteArray &binaryData);

private:
    TcpClient* client_det1; //FPGA板1
    TcpClient* client_det2; //FPGA板2
    TcpClient* client_arm1; //ARM设备1
    TcpClient* client_arm2; //ARM设备2
    TcpClient* client_relay; //继电器

    QString ip_det1;
    QString ip_det2;
    QString ip_arm1;
    QString ip_arm2;
    QString ip_relay;
    quint16 port_det1;
    quint16 port_det2;
    quint16 port_arm1;
    quint16 port_arm2;
    quint16 port_relay;
    
    //文件存储格式，.dat(二进制)或者.txt(文本)，默认.dat
    saveFileFormat mfileFormat = Binary;
    QString mSavePath = "data"; //默认保存路径
    QString mfileNameDet1; //当前测量的文件名，包含路径
    QString mfileNameDet2; //当前测量的文件名，包含路径
    DetParameter m_detPara; //测量参数，包含触发模式、传输模式、测量时长等
    QByteArray m_det1Buffer;
    QByteArray m_det2Buffer;

    QByteArray cmdSoftTrigger;//软件触发模式，开始测量
    QVector<CommandItem> cmdPool; //常用指令池，可以根据需要添加更多指令
};

#endif // COMMANDHELPER_H
