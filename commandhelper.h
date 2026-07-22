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
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include "tcpclient.h"
#include "globalsettings.h"
#include "dataprocessor.h"

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
    ~CommandHelper() override;

    static CommandHelper *instance() {
        static CommandHelper commHelper;
        return &commHelper;
    }

    void testSend();
    
    // 连接继电器网络
    void connectRelay();
    // 关闭继电器网络
    void disconnectRelay();
    
    // 发送指令到FPGA主板主网口
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

    bool configureMeasure(const DetParameter &detPara);
    void beginRecording(const DetParameter &detPara);
    void sendSpectrumControl(Order::TriggerMode mode);

    void stopMeasure();
    void closeMeasurementFiles();
    
    // 文件存储格式
    void setSaveFileFormat(saveFileFormat format)
    {
        QMutexLocker locker(&m_measurementMutex);
        mfileFormat = format;
    }

    void setSavePath(const QString& path)
    {
        QMutexLocker locker(&m_measurementMutex);
        mSavePath = path;
    }

    void setShotNumber(const QString& shotNumber)
    {
        QMutexLocker locker(&m_measurementMutex);
        mShotNumber = shotNumber;
    }

    // OTA升级
    void startOTAUpgrade(quint8 index);
    bool sendOTAUpgradeData(quint8 index, const QByteArray& data);
    void endOTAUpgrade(quint8 index);
    // index 1=水平相机主网口, 2=垂直相机主网口
    bool isDetectorConnected(int index) const;

    // 设置触发信号时间宽度
    void sendTriggerSignalTimeWidth(quint8 detectorIndex = 0x11/*高位-板卡2，低位-板卡1*/, quint16 timeWidth = 2000/*默认值*/);

    struct energyCalib {
        float k_calib;//能量刻度系数-k
        float b_calib;//能量刻度系数-b
    };
    QVector<energyCalib> m_channelEnergyCalib;
    void loadEnergyCalibration();// 加载能量刻度
    void saveChannelBoundary(const QVector<QVector<quint16>>&);

    // 记录当前炮号和录像文件时间戳
    QString mSavePath = "data"; //默认保存路径
    QString mShotTag;
    QString mShotTimestamp;
    DetParameter m_detPara; //测量参数，包含触发模式、传输模式、测量时长等

private:
    // 初始化常用指令
    void initCommand(); 

    // FPGA主板1主网口初始化指令
    void initFPGA1Commands();

    // FPGA主板2主网口初始化指令
    void initFPGA2Commands();
    
    // 读取网络配置，IP和port
    void loadIPConfig();

    // 初始化数据处理器
    void initDataProcessor();

    // 触发阈值
    QByteArray getCmdTriggerThold(quint16 ch1, quint16 ch2);

    // 发送指令并打印十六进制、指令名称和参数
    void sendCommand(TcpClient* client, const QByteArray& command,
                     const QString& name, const QString& parameter = QString());

    void processSpec512Data(int detectorIndex, QByteArray& buffer, const QByteArray& data);
    void processSpec16Data(int detectorIndex, QByteArray& buffer, const QByteArray& data);
    void processWaveformData(int detectorIndex, QByteArray& buffer, const QByteArray& data);
    bool parseSpectrum512Packet(int detectorIndex, const QByteArray& packet);
    bool parseSpectrum16Packet(int detectorIndex, const QByteArray& packet);

    // 16 道能谱：按 CSV 向单块 FPGA 下发 144 条能窗指令（16 通道×9 条/通道）
    void send16SpecEnergyWindowCommands(TcpClient* client, const QString& fpgaLabel,
                                        const QVector<QVector<quint16>>& channelBoundaries,
                                        int csvChannelOffset);

signals:
    // 波形数据: timeUnits 单位为 500us, samples 为 1024 个采样点
    void sigWaveformData(int detectorIndex, int channelNumber, quint32 timeUnits,
                         const QVector<quint16>& samples);

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
    void sigStatus_fpga1_wave(bool on);
    void sigStatus_fpga2_wave(bool on);
    void sigFault_fpga1_wave();//故障，一般指网络不通
    void sigFault_fpga2_wave();

    // ARM状态
    void sigARM1Status(bool on);
    void sigARM2Status(bool on);
    void sigARM1Fault();//故障，一般指网络不通
    void sigARM2Fault();
    void sigArm1SensorData(QVector<double>/*温度*/, QVector<double>/*电压*/, QVector<double>/*电流*/);//ARM1传感器数据
    void sigArm2SensorData(QVector<double>/*温度*/, QVector<double>/*电压*/, QVector<double>/*电流*/);//ARM2传感器数据

    // 能谱数据
    void sigSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                         const QVector<quint32>& counts);

    void sigOTAUpgradeData(quint8, const QByteArray& data); // 上报OTA升级数据

    void sigHardTriggeredSignalReceived();

public slots:
    // 继电器数据处理
    void handleRelayData(const QByteArray &binaryData);
    void handleFpga1MainData(const QByteArray &binaryData);
    void handleFpga2MainData(const QByteArray &binaryData);
    void handleFpga1WaveData(const QByteArray &binaryData);
    void handleFpga2WaveData(const QByteArray &binaryData);
    void handleARM1Data(const QByteArray &binaryData);
    void handleARM2Data(const QByteArray &binaryData);

private:
    TcpClient* client_fpga1_main; // FPGA主板1主网口(控制/能谱)
    DataProcessor* dataProcessor_fpga1_main;
    TcpClient* client_fpga2_main; // FPGA主板2主网口(控制/能谱)
    DataProcessor* dataProcessor_fpga2_main;
    TcpClient* client_fpga1_wave; // 副网口，仅接收波形（FPGA主板1）
    DataProcessor* dataProcessor_fpga1_wave;
    TcpClient* client_fpga2_wave; // 副网口，仅接收波形（FPGA主板2）
    DataProcessor* dataProcessor_fpga2_wave;
    TcpClient* client_arm1; //ARM设备1
    TcpClient* client_arm2; //ARM设备2
    TcpClient* client_relay; //继电器
    QTimer* armWorkTimer;

    QString ip_fpga1_main;
    QString ip_fpga2_main;
    QString ip_fpga1_wave;
    QString ip_fpga2_wave;
    QString ip_arm1;
    QString ip_arm2;
    QString ip_relay;
    quint16 port_fpga1_main;
    quint16 port_fpga2_main;
    quint16 port_fpga1_wave;
    quint16 port_fpga2_wave;
    quint16 port_arm1;
    quint16 port_arm2;
    quint16 port_relay;
    
    //文件存储格式，.dat(二进制)或者.txt(文本)，默认.dat
    saveFileFormat mfileFormat = Binary;
    QString mShotNumber; // 当前炮号，用于数据文件命名
    QString mfileNameFpga1Main; // 水平相机主网口能谱数据文件名
    QString mfileNameFpga2Main; // 垂直相机主网口能谱数据文件名
    QString mfileNameFpga1Wave; // 水平相机副网口波形数据文件名
    QString mfileNameFpga2Wave; // 垂直相机副网口波形数据文件名
    QByteArray m_fpga1MainBuffer;
    QByteArray m_fpga2MainBuffer;
    QByteArray m_fpga1WaveBuffer;
    QByteArray m_fpga2WaveBuffer;
    QByteArray m_arm1Buffer;
    QByteArray m_arm2Buffer;

    QByteArray cmdSoftTrigger;//软件触发模式，开始测量
    QVector<CommandItem> cmdPool; //常用指令池，可以根据需要添加更多指令

    //硬触发信号
    std::atomic_bool mHardTriggered = false;

    std::atomic_bool measure_started = false;
    std::atomic_bool mIsUpgrading = false;
    quint8 mCurrentUpgradeDetectorIndex = 1;

    QFile m_fpga1MainFile;
    QFile m_fpga2MainFile;
    QFile m_fpga1WaveFile;
    QFile m_fpga2WaveFile;

    mutable QMutex m_measurementMutex;

    void closeMeasurementFilesLocked();    
};

#endif // COMMANDHELPER_H
