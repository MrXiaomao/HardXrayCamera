/*
 * @Author: Maoxiaoqing
 * @Date: 2026-03-25 16:01:56
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-06-17 11:17:16
 * @Description: 请填写简介
 */
#include "commandhelper.h"
#include "globalsettings.h"
#include "order.h"
#include "detectorsetting.h"
#include <QFile>
#include <QDir>
#include <QMutexLocker>
// #include <algorithm>
// #include <chrono>
// #include <random>

// ARM2 无温度传感器时硬件回传 0xFF，临时使用占位温度（高斯分布，约 38℃ ±2℃）
#define ARM2_TEMP_STUB_BASE   38.0
#define ARM2_TEMP_STUB_DELTA  0.4

namespace {

/*double generateArm2StubTemperature()
{
    static std::mt19937 gen(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::normal_distribution<double> dist(0.0, ARM2_TEMP_STUB_DELTA / 3.0);

    double delta = dist(gen);
    delta = std::max(-ARM2_TEMP_STUB_DELTA, std::min(delta, ARM2_TEMP_STUB_DELTA));
    return ARM2_TEMP_STUB_BASE + delta;
}*/

double parseArm2Temperature(quint8 highByte, quint8 lowByte)
{
    //不再使用占位温度，直接返回 0xFF 0xFF 时的模拟温度
    // if (highByte == 0xFF && lowByte == 0xFF)
    //     return generateArm2StubTemperature();
    const quint16 rawValue = (static_cast<quint16>(highByte) << 8) | static_cast<quint16>(lowByte);
    return static_cast<double>(rawValue) / 10.0;
}

constexpr int Spectrum512PacketSize = 1032;
constexpr int Spectrum512BinCount = 512;
constexpr int Spectrum16PacketSize = 40; // 2+2+2+16*2+2
constexpr int Spectrum16BinCount = 16;
const QByteArray SpectrumHeader = QByteArray::fromHex("aa bb");
const QByteArray SpectrumTail = QByteArray::fromHex("cc dd");

// Waveform packet constants
constexpr int WaveformPacketSize = 1168;
const QByteArray WaveformHeader = QByteArray::fromHex("aa bb");
const QByteArray WaveformTail = QByteArray::fromHex("cc dd");

QString transferModeText(Order::TransferMode mode)
{
    switch (mode) {
    case Order::Spectrum512:
        return QStringLiteral("分时能谱");
    case Order::Spectrum16:
        return QStringLiteral("HXR能量道");
    case Order::Waveform:
        return QStringLiteral("波形");
    }
    return QString("未知模式(%1)").arg(static_cast<int>(mode));
}

QString triggerModeText(Order::TriggerMode mode)
{
    switch (mode) {
    case Order::Stop:
        return "停止";
    case Order::SoftwareTrigger:
        return "软件触发";
    case Order::HardwareTrigger:
        return "硬件触发";
    }
    return QString("未知触发(%1)").arg(static_cast<int>(mode));
}

quint32 readUInt32BE(const char* data)
{
    const uchar* p = reinterpret_cast<const uchar*>(data);
    return (static_cast<quint32>(p[0]) << 24)
        | (static_cast<quint32>(p[1]) << 16)
        | (static_cast<quint32>(p[2]) << 8)
        | static_cast<quint32>(p[3]);
}

quint16 readUInt16BE(const char* data)
{
    const uchar* p = reinterpret_cast<const uchar*>(data);
    return (static_cast<quint16>(p[0]) << 8) | static_cast<quint16>(p[1]);
}


int channelNumberFromMask(quint32 channelMask)
{
    for (int bit = 0; bit < 16; ++bit) {
        if (channelMask & (1u << bit))
            return bit + 1;
    }
    return 0;
}

void logTcpConnectFailure(const QString& deviceName, const QString& host, quint16 port,
                          const QString& errorString)
{
    qWarning().noquote() << QStringLiteral("%1连接失败，IP: %2，端口: %3，原因: %4")
                                .arg(deviceName, host, QString::number(port), errorString);
}

const char kFpga1MainPort[] = "水平相机主网口(控制/能谱)";
const char kFpga2MainPort[] = "垂直相机主网口(控制/能谱)";
const char kFpga1WavePort[] = "水平相机副网口(波形接收)";
const char kFpga2WavePort[] = "垂直相机副网口(波形接收)";

} // end anonymous namespace

CommandHelper::CommandHelper(QObject *parent)
    : QObject{parent}
{
    loadIPConfig();
    initCommand();
    loadEnergyCalibration();

    client_fpga1_main = new TcpClient(this); // FPGA主板1主网口(控制/能谱)
    client_fpga2_main = new TcpClient(this); // FPGA主板2主网口(控制/能谱)
    client_fpga1_wave = new TcpClient(this); // 副网口，仅接收波形（FPGA主板1）
    client_fpga2_wave = new TcpClient(this); // 副网口，仅接收波形（FPGA主板2）
    client_arm1 = new TcpClient(this); //ARM设备1
    client_arm2 = new TcpClient(this); //ARM设备2
    client_relay = new TcpClient(this); //继电器
    client_arm1->setAutoReconnect(true);
    client_arm2->setAutoReconnect(true);

    // 温度检测定时器
    armWorkTimer = new QTimer(this);
    connect(armWorkTimer, &QTimer::timeout, this, [=]{
        const QByteArray queryCommand = QByteArray::fromHex("12 34 01 00 00 00 00 00 00 00 AB CD");

        if (client_arm1->isConnected()){
            client_arm1->send(queryCommand);
        }

        if (client_arm2->isConnected()){
            client_arm2->send(queryCommand);
        }
    });

    //状态改变
    connect(client_fpga1_main, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "FPGA主板1主网口连接成功";
            emit sigDetector1Status(true);
        } else {
            // qInfo() << "FPGA主板1主网口断开连接";
            emit sigDetector1Status(false);
        }
    });

    connect(client_fpga2_main, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "FPGA主板2主网口连接成功";
            emit sigDetector2Status(true);
        } else {
            // qInfo() << "FPGA主板2主网口断开连接";
            emit sigDetector2Status(false);
        }
    });

    connect(client_fpga1_wave, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        emit sigStatus_fpga1_wave(connected);
    });

    connect(client_fpga2_wave, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        emit sigStatus_fpga2_wave(connected);
    });

    connect(client_arm1, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "ARM设备1连接成功";
            // 发送检测指令
            QByteArray command = QByteArray::fromHex("12 34 01 00 00 00 00 00 00 00 AB CD");
            client_arm1->send(command);

            emit sigARM1Status(true);
        } else {
            // qInfo() << "ARM设备1断开连接";
            emit sigARM1Status(false);
        }
    });

    connect(client_arm2, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "ARM设备2连接成功";
            // 发送检测指令
            QByteArray command = QByteArray::fromHex("12 34 01 00 00 00 00 00 00 00 AB CD");
            client_arm1->send(command);

            emit sigARM2Status(true);
        } else {
            // qInfo() << "ARM设备2断开连接";
            emit sigARM2Status(false);
        }
    });

    connect(client_relay, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            //查询继电器状态指令
            QByteArray cmdStatusQuery = QByteArray::fromHex("48 3a 01 53 00 00 00 00 00 00 00 00 d6 45 44"); 
            sendCommand(client_relay, cmdStatusQuery, "继电器状态查询", "查询吸合状态");

            emit sigRelayStatus(true);
        } else {
            emit sigRelayStatus(false);
        }
    });

    connect(client_relay, &TcpClient::dataReceived, this, &CommandHelper::handleRelayData);
    connect(client_fpga1_main, &TcpClient::dataReceived, this, &CommandHelper::handleFpga1MainData, Qt::DirectConnection);
    connect(client_fpga2_main, &TcpClient::dataReceived, this, &CommandHelper::handleFpga2MainData, Qt::DirectConnection);
    connect(client_fpga1_wave, &TcpClient::dataReceived, this, &CommandHelper::handleFpga1WaveData, Qt::DirectConnection);
    connect(client_fpga2_wave, &TcpClient::dataReceived, this, &CommandHelper::handleFpga2WaveData, Qt::DirectConnection);
    connect(client_arm1, &TcpClient::dataReceived, this, &CommandHelper::handleARM1Data, Qt::DirectConnection);
    connect(client_arm2, &TcpClient::dataReceived, this, &CommandHelper::handleARM2Data, Qt::DirectConnection);
    
    connect(client_relay, &TcpClient::sigconnectError, this,
            [=](QAbstractSocket::SocketError error, const QString& host, quint16 port,
                const QString& errorString) {
        logTcpConnectFailure(QStringLiteral("继电器"), host, port, errorString);
        emit sigRelayConnectError(error);
    });
    
    // 连接失败
    connect(client_fpga1_main, &TcpClient::sigconnectError, this,
            [=](QAbstractSocket::SocketError, const QString& host, quint16 port,
                const QString& errorString) {
        logTcpConnectFailure(QString::fromUtf8(kFpga1MainPort), host, port, errorString);
        emit sigDetector1Fault();
    });

    connect(client_fpga2_main, &TcpClient::sigconnectError, this,
            [=](QAbstractSocket::SocketError, const QString& host, quint16 port,
                const QString& errorString) {
        logTcpConnectFailure(QString::fromUtf8(kFpga2MainPort), host, port, errorString);
        emit sigDetector2Fault();
    });

    connect(client_fpga1_wave, &TcpClient::sigconnectError, this,
            [=](QAbstractSocket::SocketError, const QString& host, quint16 port,
                const QString& errorString) {
        logTcpConnectFailure(QString::fromUtf8(kFpga1WavePort), host, port, errorString);
        emit sigFault_fpga1_wave();
    });

    connect(client_fpga2_wave, &TcpClient::sigconnectError, this,
            [=](QAbstractSocket::SocketError, const QString& host, quint16 port,
                const QString& errorString) {
        logTcpConnectFailure(QString::fromUtf8(kFpga2WavePort), host, port, errorString);
        emit sigFault_fpga2_wave();
    });

    connect(client_arm1, &TcpClient::sigconnectError, this,
            [=](QAbstractSocket::SocketError, const QString& host, quint16 port,
                const QString& errorString) {
        logTcpConnectFailure(QStringLiteral("状态监测设备1"), host, port, errorString);
        emit sigARM1Fault();
    });

    connect(client_arm2, &TcpClient::sigconnectError, this,
            [=](QAbstractSocket::SocketError, const QString& host, quint16 port,
                const QString& errorString) {
        logTcpConnectFailure(QStringLiteral("状态监测设备2"), host, port, errorString);
        emit sigARM2Fault();
    });
}

void CommandHelper::connectDetector()
{
    loadIPConfig(); //确保使用最新的网络配置
    client_fpga1_main->connectToHost(ip_fpga1_main, port_fpga1_main);
    client_fpga2_main->connectToHost(ip_fpga2_main, port_fpga2_main);
    client_fpga1_wave->connectToHost(ip_fpga1_wave, port_fpga1_wave);
    client_fpga2_wave->connectToHost(ip_fpga2_wave, port_fpga2_wave);
}

void CommandHelper::disconnectDetector()
{
    client_fpga1_main->disconnectFromHost();
    client_fpga2_main->disconnectFromHost();
    client_fpga1_wave->disconnectFromHost();
    client_fpga2_wave->disconnectFromHost();
}

void CommandHelper::connectARM()
{
    loadIPConfig(); //确保使用最新的网络配置

    client_arm1->connectToHost(ip_arm1, port_arm1);
    client_arm2->connectToHost(ip_arm2, port_arm2);

    armWorkTimer->start(5000);
}

void CommandHelper::disconnectARM()
{
    client_arm1->setAutoReconnect(false);
    client_arm2->setAutoReconnect(false);
    client_arm1->disconnectFromHost();
    client_arm2->disconnectFromHost();

    armWorkTimer->stop();
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
        sendCommand(client_relay, cmdPowerOn, "继电器电源控制", "开启");
    }
}

// 继电器关闭电源
void CommandHelper::PowerOffRelay()
{
    if (client_relay) {
        QByteArray cmdPowerOff = QByteArray::fromHex("48 3a 01 57 00 00 00 00 00 00 00 00 da 45 44"); 
        sendCommand(client_relay, cmdPowerOff, "继电器电源控制", "关闭");
    }
}

void CommandHelper::testSend()
{
    QByteArray data = QByteArray::fromHex("12 34 00 0A DA 11");
    sendCommand(client_relay, data, "测试指令", "继电器测试发送");
}

void CommandHelper::initCommand()
{
    // 初始化常用指令
    cmdSoftTrigger = QByteArray::fromHex("12 34 00 0A DA 11");
    cmdPool.append(CommandItem("软件触发", cmdSoftTrigger));
}

// FPGA主板1主网口初始化指令
void CommandHelper::initFPGA1Commands()
{
    //传输模式设置指令
    QByteArray cmdSetTransferMode = Order::setTransferMode(Order::TransferMode::Spectrum512);
    sendCommand(client_fpga1_main, cmdSetTransferMode, "传输模式设置",
                QString("%1 512道能谱").arg(QString::fromUtf8(kFpga1MainPort)));
}

void CommandHelper::initFPGA2Commands()
{
    // FPGA主板2主网口初始化指令
    // 这里可以添加更多针对FPGA主板2主网口的常用指令
}

bool CommandHelper::configureMeasure(const DetParameter &measurement)
{
    m_detPara = measurement;

    sendCommand(client_fpga1_main, Order::setTransferMode(measurement.transferMode),
                "传输模式设置",
                QString("%1 %2").arg(QString::fromUtf8(kFpga1MainPort),
                                     transferModeText(measurement.transferMode)));
    sendCommand(client_fpga2_main, Order::setTransferMode(measurement.transferMode),
                "传输模式设置",
                QString("%1 %2").arg(QString::fromUtf8(kFpga2MainPort),
                                     transferModeText(measurement.transferMode)));
    sendCommand(client_fpga1_main, Order::setSpectrumRefreshTime(measurement.spectrumRefreshInterval),
                "能谱刷新时间",
                QString("%1 %2 ms").arg(QString::fromUtf8(kFpga1MainPort))
                    .arg(measurement.spectrumRefreshInterval));
    sendCommand(client_fpga2_main, Order::setSpectrumRefreshTime(measurement.spectrumRefreshInterval),
                "能谱刷新时间",
                QString("%1 %2 ms").arg(QString::fromUtf8(kFpga2MainPort))
                    .arg(measurement.spectrumRefreshInterval));
    sendCommand(client_fpga1_main, Order::setSpectrumTriggerThreshold(measurement.spectrumTriggerThreshold),
                "能谱触发阈值",
                QString("%1 %2 LSB").arg(QString::fromUtf8(kFpga1MainPort))
                    .arg(measurement.spectrumTriggerThreshold));
    sendCommand(client_fpga2_main, Order::setSpectrumTriggerThreshold(measurement.spectrumTriggerThreshold),
                "能谱触发阈值",
                QString("%1 %2 LSB").arg(QString::fromUtf8(kFpga2MainPort))
                    .arg(measurement.spectrumTriggerThreshold));

    const int deadTime16ns = measurement.spectrumDeadTime / 16;
    sendCommand(client_fpga1_main, Order::setSpectrumDeadTime(deadTime16ns),
                "能谱死时间",
                QString("%1 %2 ns, %3*16ns").arg(QString::fromUtf8(kFpga1MainPort))
                    .arg(measurement.spectrumDeadTime).arg(deadTime16ns));
    sendCommand(client_fpga2_main, Order::setSpectrumDeadTime(deadTime16ns),
                "能谱死时间",
                QString("%1 %2 ns, %3*16ns").arg(QString::fromUtf8(kFpga2MainPort))
                    .arg(measurement.spectrumDeadTime).arg(deadTime16ns));

    if (measurement.transferMode == Order::TransferMode::Spectrum16) {
        QString csvError;
        if (!DetectorSetting::reloadEnergyBoundaries(&csvError)
            || DetectorSetting::energyBoundaries().isEmpty()) {
            if (csvError.isEmpty())
                csvError = tr("未配置16道能谱能窗CSV文件路径");
            qWarning() << "16道能谱能窗CSV加载失败:" << csvError;
            emit sigAppendMsg(tr("16道能谱能窗CSV无效，已取消测量：%1").arg(csvError), QtWarningMsg);
            return false;
        }

        const QVector<QVector<quint16>>& energyBoundaries = DetectorSetting::energyBoundaries();

        // 将能量转换为道址
        QVector<QVector<quint16>> channelBoundaries;
        int chIdx = 0;
        for (const auto& boundar : energyBoundaries){
            QVector<quint16> channelBoundary;
            for (const auto& a : boundar){
                int channelAddr = static_cast<int>((a - m_channelEnergyCalib[chIdx].b_calib) / m_channelEnergyCalib[chIdx].k_calib);
                if (channelAddr < 0) channelAddr = 0;
                channelBoundary.push_back(channelAddr);
            }

            channelBoundaries.push_back(channelBoundary);
            chIdx++;
        }
        send16SpecEnergyWindowCommands(client_fpga1_main, QString::fromUtf8(kFpga1MainPort),
                                       channelBoundaries, 0);
        send16SpecEnergyWindowCommands(client_fpga2_main, QString::fromUtf8(kFpga2MainPort),
                                       channelBoundaries, 16);
    }

    return true;
}

void CommandHelper::beginRecording(const DetParameter &measurement)
{
    QMutexLocker locker(&m_measurementMutex);
    closeMeasurementFilesLocked();
    m_fpga1MainBuffer.clear();
    m_fpga2MainBuffer.clear();
    m_fpga1WaveBuffer.clear();
    m_fpga2WaveBuffer.clear();
    measure_started = true;
    m_detPara = measurement;

    const QString shotTag = mShotNumber.isEmpty() ? QStringLiteral("00000") : mShotNumber;
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    mfileNameFpga1Main = QString("%1/Fpga1_%2_%3_%4_spec.%5")
                             .arg(mSavePath)
                             .arg(shotTag)
                             .arg(timestamp)
                             .arg(m_detPara.measureTime)
                             .arg(mfileFormat == Binary ? "dat" : "txt");
    mfileNameFpga2Main = QString("%1/Fpga2_%2_%3_%4_spec.%5")
                             .arg(mSavePath)
                             .arg(shotTag)
                             .arg(timestamp)
                             .arg(m_detPara.measureTime)
                             .arg(mfileFormat == Binary ? "dat" : "txt");
    mfileNameFpga1Wave = QString("%1/Fpga1_%2_%3_%4_wave.%5")
                             .arg(mSavePath)
                             .arg(shotTag)
                             .arg(timestamp)
                             .arg(m_detPara.measureTime)
                             .arg(mfileFormat == Binary ? "dat" : "txt");
    mfileNameFpga2Wave = QString("%1/Fpga2_%2_%3_%4_wave.%5")
                             .arg(mSavePath)
                             .arg(shotTag)
                             .arg(timestamp)
                             .arg(m_detPara.measureTime)
                             .arg(mfileFormat == Binary ? "dat" : "txt");

    QDir dir(mSavePath);
    if (!dir.exists())
        dir.mkpath(".");

    m_fpga1MainFile.setFileName(mfileNameFpga1Main);
    m_fpga2MainFile.setFileName(mfileNameFpga2Main);
    m_fpga1WaveFile.setFileName(mfileNameFpga1Wave);
    m_fpga2WaveFile.setFileName(mfileNameFpga2Wave);

    qInfo()<<"数据保存路径:"<<mSavePath;
    qInfo()<<"水平相机能谱数据文件名:"<<QFileInfo(mfileNameFpga1Main).fileName();;
    qInfo()<<"垂直相机能谱数据文件名:"<<QFileInfo(mfileNameFpga2Main).fileName();
    qInfo()<<"水平相机波形数据文件名:"<<QFileInfo(mfileNameFpga1Wave).fileName();
    qInfo()<<"垂直相机波形数据文件名:"<<QFileInfo(mfileNameFpga2Wave).fileName();

    if (!m_fpga1MainFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "水平相机能谱数据创建失败，文件名：" << mfileNameFpga1Main;
    }
    if (!m_fpga2MainFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "垂直相机能谱数据创建失败，文件名：" << mfileNameFpga2Main;
    }
    if (!m_fpga1WaveFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "水平相机波形数据创建失败，文件名：" << mfileNameFpga1Wave;
    }
    if (!m_fpga2WaveFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "垂直相机波形数据创建失败，文件名：" << mfileNameFpga2Wave;
    }
}

void CommandHelper::sendSpectrumControl(Order::TriggerMode mode)
{
    sendCommand(client_fpga1_main, Order::controlSpectrum(mode),
                "能谱测量控制",
                QString("%1 %2").arg(QString::fromUtf8(kFpga1MainPort), triggerModeText(mode)));
    sendCommand(client_fpga2_main, Order::controlSpectrum(mode),
                "能谱测量控制",
                QString("%1 %2").arg(QString::fromUtf8(kFpga2MainPort), triggerModeText(mode)));
}

void CommandHelper::startMeasure(DetParameter detPara)
{
    const DetParameter measurement = detPara;

    if (!configureMeasure(measurement))
        return;

    beginRecording(measurement);
    sendSpectrumControl(measurement.trigMode);

    qInfo().noquote() << QStringLiteral("测量已开始，参数：时长 %1 ms，传输模式：%2，能谱刷新间隔：%3 ms，能谱触发阈值：%4，能谱死时间：%5 ns，触发模式：%6")
                             .arg(measurement.measureTime)
                             .arg(transferModeText(measurement.transferMode))
                             .arg(measurement.spectrumRefreshInterval)
                             .arg(measurement.spectrumTriggerThreshold)
                             .arg(measurement.spectrumDeadTime)
                             .arg(triggerModeText(measurement.trigMode));
}

void CommandHelper::stopMeasure()
{
    sendSpectrumControl(Order::Stop);

    QMutexLocker locker(&m_measurementMutex);
    measure_started = false;
    closeMeasurementFilesLocked();

    qDebug() << "Measurement stopped.";
}

void CommandHelper::closeMeasurementFiles()
{
    QMutexLocker locker(&m_measurementMutex);
    closeMeasurementFilesLocked();
}

void CommandHelper::closeMeasurementFilesLocked()
{
    if (m_fpga1MainFile.isOpen())
        m_fpga1MainFile.close();
    if (m_fpga2MainFile.isOpen())
        m_fpga2MainFile.close();
    if (m_fpga1WaveFile.isOpen())
        m_fpga1WaveFile.close();
    if (m_fpga2WaveFile.isOpen())
        m_fpga2WaveFile.close();
}

CommandHelper::~CommandHelper()
{
    closeMeasurementFiles();
}

void CommandHelper::send16SpecEnergyWindowCommands(TcpClient* client, const QString& fpgaLabel,
                                                 const QVector<QVector<quint16>>& energyBoundaries,
                                                 int csvChannelOffset)
{
    for (int channel = 0; channel < 16; ++channel) {
        const int csvIndex = csvChannelOffset + channel;
        if (csvIndex < 0 || csvIndex >= energyBoundaries.size())
            continue;

        const QVector<quint16>& boundaries = energyBoundaries.at(csvIndex);
        const QVector<QByteArray> commands =
            Order::setTimeSpectrumRangeChannel(static_cast<quint8>(channel), boundaries);
        for (int cmd = 0; cmd < commands.size(); ++cmd) {
            const quint8 commandIndex = static_cast<quint8>(channel * 9 + cmd);
            const int firstIndex = cmd * 2;
            const int secondIndex = qMin(firstIndex + 1, 16);            
            sendCommand(client, commands.at(cmd), QStringLiteral("分时能谱能窗"),
                        QStringLiteral("%1 逻辑CH%2 序号0x%3 能量%4-%5-%6")
                            .arg(fpgaLabel)
                            .arg(channel + 1)
                            .arg(commandIndex, 2, 16, QChar('0'))
                            .arg(boundaries.at(firstIndex))
                            .arg(boundaries.at(secondIndex)));
        }
    }
}

void CommandHelper::sendTriggerSignalTimeWidth(quint8 detectorIndex, quint16 timeWidth)
{
    const quint16 timeWidth16ns = (quint32)timeWidth * 16 / 16;
    if (detectorIndex & 0x01){
        sendCommand(client_fpga1_main, Order::setTriggerSignalTimeWidth(timeWidth16ns),
                "板卡#1触发信号宽度",
                QString("%1 ns").arg(timeWidth16ns));
    }

    if (detectorIndex & 0x10){
        sendCommand(client_fpga2_main, Order::setTriggerSignalTimeWidth(timeWidth16ns),
                "板卡#2触发信号宽度",
                QString("%1 ns").arg(timeWidth16ns));
    }
}

void CommandHelper::sendCommand(TcpClient* client, const QByteArray& command,
                                const QString& name, const QString& parameter)
{
    if (!client)
        return;

    client->send(command);

    const QString detail = parameter.isEmpty()
        ? name
        : QString("%1：%2").arg(name, parameter);
    qDebug().noquote() << QString("Send HEX: %1[%2]")
        .arg(QString(command.toHex(' ')))
        .arg(detail);
}

// 读取网络配置，IP和port
// 注意要在每次连接前调用，以确保使用最新的网络配置
void CommandHelper::loadIPConfig()
{
    JsonSettings* settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    // 网络配置读取
    ip_fpga1_main = settings->getValueByPath("network/ip_fpga1_main").toString();
    ip_fpga2_main = settings->getValueByPath("network/ip_fpga2_main").toString();
    ip_fpga1_wave = settings->getValueByPath("network/ip_fpga1_wave").toString();
    ip_fpga2_wave = settings->getValueByPath("network/ip_fpga2_wave").toString();
    ip_arm1 = settings->getValueByPath("network/ip_arm1").toString();
    ip_arm2 = settings->getValueByPath("network/ip_arm2").toString();
    ip_relay = settings->getValueByPath("network/ip_relay").toString();

    port_fpga1_main = settings->getValueByPath("network/port_fpga1_main").toUInt();
    port_fpga2_main = settings->getValueByPath("network/port_fpga2_main").toUInt();
    port_fpga1_wave = settings->getValueByPath("network/port_fpga1_wave").toUInt();
    port_fpga2_wave = settings->getValueByPath("network/port_fpga2_wave").toUInt();
    port_arm1 = settings->getValueByPath("network/port_arm1").toUInt();
    port_arm2 = settings->getValueByPath("network/port_arm2").toUInt();
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

void CommandHelper::handleFpga1MainData(const QByteArray &binaryData)
{
    if (mIsUpgrading.load() && mDetectorIndex == 1)
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    {
        QMutexLocker locker(&m_measurementMutex);
        if (!measure_started){
            // 测量未开始不应该进入到这里，可能是上次未点击停止测量
            return ;
        }

        //存储数据到mfileNameFpga1Main文件中。
        if (mfileFormat == Binary){
            if (m_fpga1MainFile.isOpen()) {
                m_fpga1MainFile.write(binaryData);
                m_fpga1MainFile.flush();
            }
        }
        // 处理水平相机主网口能谱数据，根据当前传输模式选择解析器
        if (m_detPara.transferMode == Order::TransferMode::Spectrum16) {
            processSpec16Data(1, m_fpga1MainBuffer, binaryData);
        } else {
            processSpec512Data(1, m_fpga1MainBuffer, binaryData);
        }
    }
}

// 处理FPGA主板2主网口能谱数据
void CommandHelper::handleFpga2MainData(const QByteArray &binaryData)
{
    if (mIsUpgrading.load() && mDetectorIndex == 2)
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    QMutexLocker locker(&m_measurementMutex);
    if (!measure_started){
        // 测量未开始不应该进入到这里，可能是上次未点击停止测量
        return ;
    }

    //存储数据到mfileNameFpga2Main文件中。
    if (mfileFormat == Binary){
        if (m_fpga2MainFile.isOpen()) {
            m_fpga2MainFile.write(binaryData);
            m_fpga2MainFile.flush();
        }
    }

    // 处理垂直相机主网口能谱数据，根据当前传输模式选择解析器
    if (m_detPara.transferMode == Order::TransferMode::Spectrum16) {
        processSpec16Data(2, m_fpga2MainBuffer, binaryData);
    } else {
        processSpec512Data(2, m_fpga2MainBuffer, binaryData);
    }
}

// 处理FPGA主板1副网口波形数据
void CommandHelper::handleFpga1WaveData(const QByteArray &binaryData)
{
    if (mIsUpgrading.load() && mDetectorIndex == 3)
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    QMutexLocker locker(&m_measurementMutex);
    if (!measure_started){
        // 测量未开始不应该进入到这里，可能是上次未点击停止测量
        return ;
    }

    //存储数据到mfileNameFpga1Wave文件中。
    if (mfileFormat == Binary){
        if (m_fpga1WaveFile.isOpen()) {
            m_fpga1WaveFile.write(binaryData);
            m_fpga1WaveFile.flush();
        }
    }

    // 处理水平相机副网口波形数据
    processWaveformData(1, m_fpga1WaveBuffer, binaryData);
}

// 处理FPGA主板2副网口波形数据
void CommandHelper::handleFpga2WaveData(const QByteArray &binaryData)
{
    if (mIsUpgrading.load() && mDetectorIndex == 4)
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    QMutexLocker locker(&m_measurementMutex);
    if (!measure_started){
        // 测量未开始不应该进入到这里，可能是上次未点击停止测量
        return ;
    }

    //存储数据到mfileNameFpga2Wave文件中。
    if (mfileFormat == Binary){
        if (m_fpga2WaveFile.isOpen()) {
            m_fpga2WaveFile.write(binaryData);
            m_fpga2WaveFile.flush();
        }
    }

    // 处理垂直相机副网口波形数据
    processWaveformData(2, m_fpga2WaveBuffer, binaryData);
}

// 处理512道能谱数据，按照协议解析出时间戳、通道号和计数，并通过信号发送给界面更新
void CommandHelper::processSpec512Data(int detectorIndex, QByteArray& buffer, const QByteArray& data)
{
    buffer.append(data);

    while (buffer.size() >= SpectrumHeader.size()) {
        const int headerIndex = buffer.indexOf(SpectrumHeader);
        if (headerIndex < 0) {
            buffer.clear();
            return;
        }

        if (headerIndex > 0)
            buffer.remove(0, headerIndex);

        if (buffer.size() < Spectrum512PacketSize)
            return;

        const QByteArray packet = buffer.left(Spectrum512PacketSize);
        if (packet.mid(Spectrum512PacketSize - SpectrumTail.size(), SpectrumTail.size()) != SpectrumTail) {
            // 包尾不对，继续寻找下一个包头
            buffer.remove(0, SpectrumHeader.size());
            qWarning() << "Invalid 512-bin spectrum packet tail from detector" << detectorIndex;
            continue;
        }

        parseSpectrum512Packet(detectorIndex, packet);
        buffer.remove(0, Spectrum512PacketSize);
    }
}

bool CommandHelper::parseSpectrum512Packet(int detectorIndex, const QByteArray& packet)
{
    if (packet.size() != Spectrum512PacketSize)
        return false;
    if (!packet.startsWith(SpectrumHeader) || !packet.endsWith(SpectrumTail))
        return false;

    const quint32 timeMs = readUInt16BE(packet.constData() + 4);
    const quint32 channelMask = readUInt16BE(packet.constData() + 2);
    const int channelNumber = channelNumberFromMask(channelMask);

    QVector<quint32> counts;
    counts.reserve(Spectrum512BinCount);

    QStringList strCounts;
    strCounts << QString::number(timeMs) << QString::number(channelNumber);
    const char* spectrumData = packet.constData() + 6;
    for (int i = 0; i < Spectrum512BinCount; ++i) {
        counts.append(readUInt16BE(spectrumData + i * 2));
        strCounts << QString::number(readUInt16BE(spectrumData + i * 2));
    }

    if (mfileFormat == Text){
        QFile* fpgaMainFile[] = {&m_fpga1MainFile, &m_fpga2MainFile};
        if (fpgaMainFile[detectorIndex-1]->isOpen()) {
            fpgaMainFile[detectorIndex-1]->write(strCounts.join(',').toLatin1());
            fpgaMainFile[detectorIndex-1]->write("\n");
            fpgaMainFile[detectorIndex-1]->flush();
        }
    }

    emit sigSpectrumData(detectorIndex, channelNumber, timeMs, counts);
    return true;
}

// 16道能谱：包头(2) + 通道号(2) + 时间(2) + 16道计数(32) + 包尾(2) = 40 字节
void CommandHelper::processSpec16Data(int detectorIndex, QByteArray& buffer, const QByteArray& data)
{
    buffer.append(data);

    while (buffer.size() >= SpectrumHeader.size()) {
        const int headerIndex = buffer.indexOf(SpectrumHeader);
        if (headerIndex < 0) {
            buffer.clear();
            return;
        }

        if (headerIndex > 0)
            buffer.remove(0, headerIndex);

        if (buffer.size() < Spectrum16PacketSize)
            return;

        const QByteArray packet = buffer.left(Spectrum16PacketSize);
        if (packet.mid(Spectrum16PacketSize - SpectrumTail.size(), SpectrumTail.size()) != SpectrumTail) {
            buffer.remove(0, SpectrumHeader.size());
            qWarning() << "Invalid 16-bin spectrum packet tail from detector" << detectorIndex;
            continue;
        }

        parseSpectrum16Packet(detectorIndex, packet);
        buffer.remove(0, Spectrum16PacketSize);
    }
}

bool CommandHelper::parseSpectrum16Packet(int detectorIndex, const QByteArray& packet)
{
    if (packet.size() != Spectrum16PacketSize)
        return false;
    if (!packet.startsWith(SpectrumHeader) || !packet.endsWith(SpectrumTail))
        return false;

    const quint32 channelMask = readUInt16BE(packet.constData() + 2);
    const quint32 timeMs = readUInt16BE(packet.constData() + 4);
    const int channelNumber = channelNumberFromMask(channelMask);
    if (channelNumber < 1 || channelNumber > 16) {
        qWarning() << "Invalid 16-bin spectrum channel from detector" << detectorIndex
                   << "raw:" << channelMask;
        return false;
    }

    QVector<quint32> counts;
    counts.reserve(Spectrum16BinCount);

    QStringList strCounts;
    strCounts << QString::number(timeMs) << QString::number(channelNumber);
    const char* spectrumData = packet.constData() + 6;
    for (int i = 0; i < Spectrum16BinCount; ++i) {
        counts.append(readUInt16BE(spectrumData + i * 2));
        strCounts << QString::number(readUInt16BE(spectrumData + i * 2));
    }

    if (mfileFormat == Text){
        QFile* fpgaMainFile[] = {&m_fpga1MainFile, &m_fpga2MainFile};
        if (fpgaMainFile[detectorIndex-1]->isOpen()) {
            fpgaMainFile[detectorIndex-1]->write(strCounts.join(',').toLatin1());
            fpgaMainFile[detectorIndex-1]->write("\n");
            fpgaMainFile[detectorIndex-1]->flush();
        }
    }

    emit sigSpectrumData(detectorIndex, channelNumber, timeMs, counts);
    return true;
}

void CommandHelper::processWaveformData(int detectorIndex, QByteArray& buffer, const QByteArray& data)
{
    buffer.append(data);

    if (m_detPara.trigMode == Order::TriggerMode::HardwareTrigger){
        if (!mHardTriggered[detectorIndex-1].load())
        {
            const QByteArray hardTriggerCommand = QByteArray::fromHex("12 34 00 AB FF C0 00 00 00 01 AB CD");

            // 波形数据来临之前先判断硬触发信号
            if (buffer.contains(hardTriggerCommand)){
                buffer.remove(0, hardTriggerCommand.size());

                if (!mHardTriggered[0] && !mHardTriggered[1]){
                    emit sigMeasureStarted();
                }

                mHardTriggered[detectorIndex-1].store(true);
            }
            else {
                buffer.clear();
                return;
            }
        }
    }

    while (buffer.size() >= WaveformHeader.size()) {
        const int headerIndex = buffer.indexOf(WaveformHeader);
        if (headerIndex < 0) {
            buffer.clear();
            return;
        }

        if (headerIndex > 0)
            buffer.remove(0, headerIndex);

        if (buffer.size() < WaveformPacketSize)
            return;

        const QByteArray packet = buffer.left(WaveformPacketSize);
        if (packet.mid(WaveformPacketSize - WaveformTail.size(), WaveformTail.size()) != WaveformTail) {
            buffer.remove(0, WaveformHeader.size());
            //打印通道号和时间戳
            const char* p = packet.constData();
            const quint32 channelMask = readUInt16BE(p + WaveformHeader.size());
            const quint32 timeUnits = readUInt16BE(p + WaveformHeader.size() + 2); // 时间单位，10ms
            const int channelNumber = channelNumberFromMask(channelMask);
            qDebug() << "Invalid waveform packet tail from detector" << detectorIndex << "Channel:" << channelNumber << "Time(units):" << timeUnits;
            continue;
        }

        // parse
        const char* p = packet.constData();
        const quint32 channelMask = readUInt16BE(p + WaveformHeader.size());
        const quint32 timeUnits = readUInt16BE(p + WaveformHeader.size() + 2)*10;
        const int channelNumber = channelNumberFromMask(channelMask);

        QVector<quint16> samples;
        samples.reserve(580);

        QStringList strCounts;
        strCounts << QString::number(timeUnits-10) << QString::number(channelNumber);
        const char* sampleData = p + WaveformHeader.size() + 4;
        for (int i = 0; i < 580; ++i) {
            samples.append(readUInt16BE(sampleData + i * 2));
            strCounts << QString::number(readUInt16BE(sampleData + i * 2));
        }

        if (mfileFormat == Text){
            QFile* fpgaWaveFile[] = {&m_fpga1WaveFile, &m_fpga2WaveFile};
            if (fpgaWaveFile[detectorIndex-1]->isOpen()) {
                fpgaWaveFile[detectorIndex-1]->write(strCounts.join(',').toLatin1());
                fpgaWaveFile[detectorIndex-1]->write("\n");
                fpgaWaveFile[detectorIndex-1]->flush();
            }
        }

        emit sigWaveformData(detectorIndex, channelNumber, timeUnits, samples);
        buffer.remove(0, WaveformPacketSize);
    }
}

void CommandHelper::handleARM1Data(const QByteArray &binaryData)
{
    //信号采集机箱
    // qDebug() << "Received data from ARM 1:" << binaryData.toHex(' ');

    m_arm1Buffer.append(binaryData);
    const int baseFrameLength = 23; // 一个完整的包长度是23字节
    int offset = 0;

    auto equalAt = [&](int at, const QByteArray& head) -> bool {
        return (m_arm1Buffer.size() - at >= head.size()) &&
               (memcmp(m_arm1Buffer.constData() + at, head.constData(), static_cast<size_t>(head.size())) == 0);
    };

    while (m_arm1Buffer.size() - offset >= baseFrameLength){
        // 满足一个包的基本长度

        // 判断包头+包尾
        if (equalAt(offset, QByteArray::fromHex("AA BB")) && equalAt(offset+21, QByteArray::fromHex("CC DD"))) {
            // data[2] 为电流监测模块的485编号，默认为0x01

            QVector<double>/*温度*/ temperature;
            QVector<double>/*电压*/ voltage;
            QVector<double>/*电流*/ current;

            // 电压数据：=  (data[3]*256 +data[4]) / 100
            //             data[3]为高8位，data[4]为低8位
            const quint16 voltageRaw = readUInt16BE(m_arm1Buffer.constData() + offset + 3);
            voltage.push_back(static_cast<double>(voltageRaw) / 100.0);

            // 电流数据：=  (data[5]*256 +data[6]) / 1000
            //             data[5]为高8位，data[6]为低8位
            const quint16 currentRaw = readUInt16BE(m_arm1Buffer.constData() + offset + 5);
            current.push_back(static_cast<double>(currentRaw) / 1000.0);

            // data[7] data[8] data[9] data[10] data[11]	data[12] data[13]		为第一个温度监测模块的数据帧
            //         data[7] 为第一个温度监测模块的485编号，默认为0x02
            //         CH1的温度为： (data[8]*256+data[9])/10
            //     data[8]为高8位，data[9]为低8位
            //         CH2的温度为： (data[10]*256+data[11])/10
            //     data[10]为高8位，data[11]为低8位
            //         CH3的温度为： (data[12]*256+data[13])/10
            //     data[12]为高8位，data[13]为低8位
            for (int i=0; i<=2; ++i){
                temperature.push_back(double((quint8)m_arm1Buffer[offset+8+i*2]*256 + (quint8)m_arm1Buffer[offset+8+i*2+1]) / 10);
            }

            // data[14] data[15] data[16] data[17] data[18]	data[19] data[20]		为预留第二个温度监测模块的数据帧，暂未使用
            // data[21] 	data[22] 	为0xCC  0xDD  包尾

            emit sigArm1SensorData(temperature, voltage, current);
            offset += baseFrameLength;
        } else
        {
            ++offset;
        }
    }

    if (offset > 0){
        m_arm1Buffer.remove(0, offset);
    }
}

void CommandHelper::handleARM2Data(const QByteArray &binaryData)
{    
    //对电源机箱
    // qDebug() << "Received data from ARM 2:" << binaryData.toHex(' ');

    m_arm2Buffer.append(binaryData);
    const int baseFrameLength = 38; // 一个完整的包长度是38字节
    int offset = 0;

    auto equalAt = [&](int at, const QByteArray& head) -> bool {
        return (m_arm2Buffer.size() - at >= head.size()) &&
               (memcmp(m_arm2Buffer.constData() + at, head.constData(), static_cast<size_t>(head.size())) == 0);
    };

    while (m_arm2Buffer.size() - offset >= baseFrameLength){
        // 满足一个包的基本长度

        // 判断包头+包尾
        if (equalAt(offset, QByteArray::fromHex("AA BB")) && equalAt(offset+36, QByteArray::fromHex("CC DD"))) {
            // data[0] 	data[1] 		为0xAA  0xBB  包头
            // data[2] data[3] data[4] data[5] data[6]		为电流监测1数据帧
            // data[7] data[8] data[9] data[10] data[11]		为电流监测2数据帧
            // data[12] data[13] data[14] data[15] data[16]		为电流监测3数据帧
            // data[17] data[18] data[19] data[20] data[21]		为电流监测4数据帧
            // data[22] data[23] data[24] data[25] data[26] data[27] data[28]		为温度监测1数据帧
            // data[29] data[30] data[31] data[32] data[33] data[34] data[35]		为温度监测2数据帧
            // data[36] data[37]		为0xCC  0xDD  包尾

            QVector<double>/*温度*/ temperature;
            QVector<double>/*电压*/ voltage;
            QVector<double>/*电流*/ current;

            const quint16 voltageRaw1 = readUInt16BE(m_arm2Buffer.constData() + offset + 3);
            const quint16 currentRaw1 = readUInt16BE(m_arm2Buffer.constData() + offset + 5);
            voltage.push_back(static_cast<double>(voltageRaw1) / 100.0);
            current.push_back(static_cast<double>(currentRaw1) / 1000.0);

            const quint16 voltageRaw2 = readUInt16BE(m_arm2Buffer.constData() + offset + 8);
            const quint16 currentRaw2 = readUInt16BE(m_arm2Buffer.constData() + offset + 10);
            voltage.push_back(static_cast<double>(voltageRaw2) / 100.0);
            current.push_back(static_cast<double>(currentRaw2) / 1000.0);

            const quint16 voltageRaw3 = readUInt16BE(m_arm2Buffer.constData() + offset + 13);
            const quint16 currentRaw3 = readUInt16BE(m_arm2Buffer.constData() + offset + 15);
            voltage.push_back(static_cast<double>(voltageRaw3) / 100.0);
            current.push_back(static_cast<double>(currentRaw3) / 1000.0);

            const quint16 voltageRaw4 = readUInt16BE(m_arm2Buffer.constData() + offset + 18);
            const quint16 currentRaw4 = readUInt16BE(m_arm2Buffer.constData() + offset + 20);
            voltage.push_back(static_cast<double>(voltageRaw4) / 100.0);
            current.push_back(static_cast<double>(currentRaw4) / 1000.0);

            for (int i = 0; i <= 2; ++i) {
                const quint8 hi = static_cast<quint8>(m_arm2Buffer[offset + 23 + i * 2]);
                const quint8 lo = static_cast<quint8>(m_arm2Buffer[offset + 24 + i * 2]);
                temperature.push_back(parseArm2Temperature(hi, lo));
            }
            for (int i = 0; i <= 2; ++i) {
                const quint8 hi = static_cast<quint8>(m_arm2Buffer[offset + 30 + i * 2]);
                const quint8 lo = static_cast<quint8>(m_arm2Buffer[offset + 31 + i * 2]);
                temperature.push_back(parseArm2Temperature(hi, lo));
            }

            // data[14] data[15] data[16] data[17] data[18]	data[19] data[20]		为预留第二个温度监测模块的数据帧，暂未使用
            // data[21] data[22] data[23] 		为预留的空白数据帧
            // data[24] 	data[25] 	为0xCC  0xDD  包尾

            emit sigArm2SensorData(temperature, voltage, current);
            offset += baseFrameLength;
        } else
        {
            ++offset;
        }
    }

    if (offset > 0){
        m_arm2Buffer.remove(0, offset);
    }
}

void CommandHelper::startOTAUpgrade(quint8 index)
{
    mDetectorIndex = index;
    mIsUpgrading = true;
}

void CommandHelper::endOTAUpgrade(quint8 /*index*/)
{
    mIsUpgrading = false;
}

bool CommandHelper::isDetectorConnected(int index) const
{
    switch (index) {
    case 1:
        return client_fpga1_main && client_fpga1_main->isConnected();
    case 2:
        return client_fpga2_main && client_fpga2_main->isConnected();
    default:
        return false;
    }
}

bool CommandHelper::sendOTAUpgradeData(quint8 index, const QByteArray& data)
{
    TcpClient* client = nullptr;
    if (1 == index)
        client = client_fpga1_main;
    else if (2 == index)
        client = client_fpga2_main;
    else if (3 == index)
        client = client_fpga1_wave;
    else
        client = client_fpga2_wave;

    client->send(data);
    return true;
}

void CommandHelper::loadEnergyCalibration()
{
    // 加载能量刻度
    // 1. 初始化资源与路径定义
    m_channelEnergyCalib.reserve(32);
    for (int i=0; i<32; ++i)
    {
        m_channelEnergyCalib.append(energyCalib{1, 0});
    }

    QString csvPath = "./能量刻度.csv";
    QFile file(csvPath);

    // 2. 文件合法性校验
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "打开能量刻度CSV失败:" << file.errorString();
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    in.setAutoDetectUnicode(true);

    // 3. 跳过表头行（匹配第一行"channel,k,b"）
    if (!in.atEnd()) in.readLine();

    // 4. 逐行解析映射到结构体
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        // 跳过空行/注释行
        if (line.isEmpty() || line.startsWith('#')) continue;

        QStringList cols = line.split(',', Qt::SkipEmptyParts);
        // 严格校验每行必须有3列（通道号+K+B）
        if (cols.size() < 3)
        {
            qWarning() << "跳过非法行:" << line;
            continue;
        }

        energyCalib item;
        // 转浮点数容错处理
        bool okK = false, okB = false;
        item.k_calib = cols[1].toFloat(&okK);
        item.b_calib = cols[2].toFloat(&okB);

        if (okK && okB)
        {
            m_channelEnergyCalib.append(item);
        }
        else
        {
            qWarning() << "行数据格式错误:" << line;
        }
    }

    file.close();
    qDebug() << "成功加载" << m_channelEnergyCalib.size() << "路通道能量刻度";
}
