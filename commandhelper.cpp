/*
 * @Author: Maoxiaoqing
 * @Date: 2026-03-25 16:01:56
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-19 15:26:11
 * @Description: 请填写简介
 */
#include "commandhelper.h"
#include "globalsettings.h"
#include "order.h"
#include <QFile>

namespace {
constexpr int SpectrumPacketSize = 2064;
constexpr int SpectrumChannelCount = 512;
const QByteArray SpectrumHeader = QByteArray::fromHex("aa bb 00 00");
const QByteArray SpectrumTail = QByteArray::fromHex("cc dd 00 00");

// Waveform packet constants
constexpr int WaveformPacketSize = 520;
const QByteArray WaveformHeader = QByteArray::fromHex("aa bb");
const QByteArray WaveformTail = QByteArray::fromHex("cc dd");

QString transferModeText(Order::TransferMode mode)
{
    switch (mode) {
    case Order::Spectrum512:
        return "512道能谱";
    case Order::Spectrum16:
        return "16道能谱";
    case Order::Waveform:
        return "波形";
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

} // end anonymous namespace

CommandHelper::CommandHelper(QObject *parent)
    : QObject{parent}
{
    loadIPConfig();
    initCommand();
    
    client_det1 = new TcpClient(this); //FPGA板1
    client_det2 = new TcpClient(this); //FPGA板2
    client_arm1 = new TcpClient(this); //ARM设备1
    client_arm2 = new TcpClient(this); //ARM设备2
    client_relay = new TcpClient(this); //继电器
    
    //状态改变
    connect(client_det1, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "FPGA板1连接成功";
            emit sigDetector1Status(true);
        } else {
            // qInfo() << "FPGA板1断开连接";
            emit sigDetector1Status(false);
        }
    });

    connect(client_det2, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "FPGA板2连接成功";
            emit sigDetector2Status(true);
        } else {
            // qInfo() << "FPGA板2断开连接";
            emit sigDetector2Status(false);
        }
    });

    connect(client_arm1, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "ARM设备1连接成功";
            emit sigARM1Status(true);
        } else {
            // qInfo() << "ARM设备1断开连接";
            emit sigARM1Status(false);
        }
    });

    connect(client_arm2, &TcpClient::sigconnectStatusChanged, this, [=](bool connected){
        if(connected){
            // qInfo() << "ARM设备2连接成功";
            emit sigARM2Status(true);
        } else {
            qInfo() << "ARM设备2断开连接";
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
    connect(client_det1, &TcpClient::dataReceived, this, &CommandHelper::handleDet1Data);
    connect(client_det2, &TcpClient::dataReceived, this, &CommandHelper::handleDet2Data);
    connect(client_arm1, &TcpClient::dataReceived, this, &CommandHelper::handleARM1Data);
    connect(client_arm2, &TcpClient::dataReceived, this, &CommandHelper::handleARM2Data);
    
    connect(client_relay, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        emit sigRelayConnectError(error);
    });
    
    // 连接失败
    connect(client_det1, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError ){
        // qWarning() << "FPGA板1连接失败:" << error;
        // emit sigAppendMsg(QString("FPGA板1连接失败: %1\n").arg(error), QtWarningMsg);
        emit sigDetector1Fault();
    });

    connect(client_det2, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError){
        // qWarning() << "FPGA板2连接失败:" << error;
        // emit sigAppendMsg(QString("FPGA板2连接失败: %1\n").arg(error), QtWarningMsg);
        emit sigDetector2Fault();
    });

    connect(client_arm1, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        Q_UNUSED(error)
        emit sigARM1Fault();
    });

    connect(client_arm2, &TcpClient::sigconnectError, this, [=](QAbstractSocket::SocketError error){
        Q_UNUSED(error)
        emit sigARM2Fault();
    });
}

void CommandHelper::connectDetector()
{
    loadIPConfig(); //确保使用最新的网络配置
    client_det1->connectToHost(ip_det1, port_det1);
    client_det2->connectToHost(ip_det2, port_det2);
}

void CommandHelper::disconnectDetector()
{
    client_det1->disconnectFromHost();
    client_det2->disconnectFromHost();
}

void CommandHelper::connectARM()
{
    loadIPConfig(); //确保使用最新的网络配置
    client_arm1->connectToHost(ip_arm1, port_arm1);
    client_arm2->connectToHost(ip_arm2, port_arm2);
}

void CommandHelper::disconnectARM()
{
    client_arm1->disconnectFromHost();
    client_arm2->disconnectFromHost();
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

// FPGA板1的初始化指令
void CommandHelper::initFPGA1Commands()
{
    //传输模式设置指令
    QByteArray cmdSetTransferMode = Order::setTransferMode(Order::TransferMode::Spectrum512);
    sendCommand(client_det1, cmdSetTransferMode, "传输模式设置", "FPGA1 512道能谱");
    
}

void CommandHelper::initFPGA2Commands()
{
    // FPGA板2的初始化指令
    // 这里可以添加更多针对FPGA板2的常用指令
}

void CommandHelper::startMeasure(DetParameter detPara)
{
    m_det1Buffer.clear();
    m_det2Buffer.clear();
    m_detPara = detPara;
    //分为波形模式和能谱模式发送指令
    if(m_detPara.transferMode == Order::TransferMode::Waveform){// 发送波形模式相关指令
        // 传输模式设置
        sendCommand(client_det1, Order::setTransferMode(Order::TransferMode::Waveform),
                    "传输模式设置", "FPGA1 波形");
        sendCommand(client_det2, Order::setTransferMode(Order::TransferMode::Waveform),
                    "传输模式设置", "FPGA2 波形");

        // 波形触发阈值，两个FPGA各16通道
        QVector<quint16> thresholdsDet1;
        QVector<quint16> thresholdsDet2;
        thresholdsDet1.reserve(16);
        thresholdsDet2.reserve(16);
        for (int channel = 0; channel < 16; ++channel) {
            thresholdsDet1.append(static_cast<quint16>(m_detPara.waveformTriggerThreshold[channel]));
            thresholdsDet2.append(static_cast<quint16>(m_detPara.waveformTriggerThreshold[channel + 16]));
        }

        const QVector<QByteArray> thresholdCommandsDet1 = Order::setWaveThresholds(thresholdsDet1);
        const QVector<QByteArray> thresholdCommandsDet2 = Order::setWaveThresholds(thresholdsDet2);
        for (int i = 0; i < thresholdCommandsDet1.size(); ++i) {
            const int firstChannel = i * 2 + 1;
            const int secondChannel = firstChannel + 1;
            sendCommand(client_det1, thresholdCommandsDet1.at(i), "波形触发阈值",
                        QString("FPGA1 CH%1=%2, CH%3=%4")
                            .arg(firstChannel)
                            .arg(thresholdsDet1.at(i * 2))
                            .arg(secondChannel)
                            .arg(thresholdsDet1.at(i * 2 + 1)));
        }
        for (int i = 0; i < thresholdCommandsDet2.size(); ++i) {
            const int firstChannel = i * 2 + 17;
            const int secondChannel = firstChannel + 1;
            sendCommand(client_det2, thresholdCommandsDet2.at(i), "波形触发阈值",
                        QString("FPGA2 CH%1=%2, CH%3=%4")
                            .arg(firstChannel)
                            .arg(thresholdsDet2.at(i * 2))
                            .arg(secondChannel)
                            .arg(thresholdsDet2.at(i * 2 + 1)));
        }

        // 发送软件触发指令，开始测量
        sendCommand(client_det1, Order::controlWaveform(Order::TriggerMode::HardwareTrigger),
                    "波形测量控制", QString("FPGA1 %1").arg(triggerModeText(Order::HardwareTrigger)));
        sendCommand(client_det2, Order::controlWaveform(Order::TriggerMode::HardwareTrigger),
                    "波形测量控制", QString("FPGA2 %1").arg(triggerModeText(Order::HardwareTrigger)));

        qInfo() << "Measurement started with parameters:" << m_detPara.measureTime 
                << "ms, TransferMode:" << m_detPara.transferMode;
        //打印触发阈值，通道号和对应的阈值，一行打印两个通道的阈值
        for (int channel = 0; channel < 32; channel += 2) {
            qInfo() << QString("Threshold: CH%1=%2, CH%3=%4")
                .arg(channel + 1)
                .arg(m_detPara.waveformTriggerThreshold[channel])
                .arg(channel + 2)
                .arg(m_detPara.waveformTriggerThreshold[channel + 1]);
        }
    } else {// 发送能谱模式相关指令
        // 传输模式设置
        sendCommand(client_det1, Order::setTransferMode(m_detPara.transferMode),
                    "传输模式设置", QString("FPGA1 %1").arg(transferModeText(m_detPara.transferMode)));
        sendCommand(client_det2, Order::setTransferMode(m_detPara.transferMode),
                    "传输模式设置", QString("FPGA2 %1").arg(transferModeText(m_detPara.transferMode)));
        // 能谱刷新时间,ms
        sendCommand(client_det1, Order::setSpectrumRefreshTime(m_detPara.spectrumRefreshInterval),
                    "能谱刷新时间", QString("FPGA1 %1 ms").arg(m_detPara.spectrumRefreshInterval));
        sendCommand(client_det2, Order::setSpectrumRefreshTime(m_detPara.spectrumRefreshInterval),
                    "能谱刷新时间", QString("FPGA2 %1 ms").arg(m_detPara.spectrumRefreshInterval));
        // 能谱触发阈值
        sendCommand(client_det1, Order::setSpectrumTriggerThreshold(m_detPara.spectrumTriggerThreshold),
                    "能谱触发阈值", QString("FPGA1 %1 LSB").arg(m_detPara.spectrumTriggerThreshold));
        sendCommand(client_det2, Order::setSpectrumTriggerThreshold(m_detPara.spectrumTriggerThreshold),
                    "能谱触发阈值", QString("FPGA2 %1 LSB").arg(m_detPara.spectrumTriggerThreshold));
        // 能谱死时间,单位*16ns
        const int deadTime16ns = m_detPara.spectrumDeadTime / 16;
        sendCommand(client_det1, Order::setSpectrumDeadTime(deadTime16ns),
                    "能谱死时间", QString("FPGA1 %1 ns, %2*16ns").arg(m_detPara.spectrumDeadTime).arg(deadTime16ns));
        sendCommand(client_det2, Order::setSpectrumDeadTime(deadTime16ns),
                    "能谱死时间", QString("FPGA2 %1 ns, %2*16ns").arg(m_detPara.spectrumDeadTime).arg(deadTime16ns));

        // 发送软件触发指令，开始测量
        sendCommand(client_det1, Order::controlSpectrum(Order::SoftwareTrigger),
                    "能谱测量控制", QString("FPGA1 %1").arg(triggerModeText(Order::SoftwareTrigger)));
        sendCommand(client_det2, Order::controlSpectrum(Order::SoftwareTrigger),
                    "能谱测量控制", QString("FPGA2 %1").arg(triggerModeText(Order::SoftwareTrigger)));
            
        //打印测量基本参数
        qInfo() << "Measurement started with parameters:" << m_detPara.measureTime 
                << "ms, TransferMode:" << m_detPara.transferMode
                << "SpectrumRefreshInterval:" << m_detPara.spectrumRefreshInterval
                << "SpectrumTriggerThreshold:" << m_detPara.spectrumTriggerThreshold
                << "SpectrumDeadTime(ns):" << m_detPara.spectrumDeadTime;
    }
    
    // 文件名格式：保存路径/DetX_时间戳_测量时长.扩展名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    mfileNameDet1 = QString("%1/Det1_%2_%3.%4")
        .arg(mSavePath)
        .arg(timestamp)
        .arg(m_detPara.measureTime)
        .arg(mfileFormat == Binary ? "dat" : "txt");
    mfileNameDet2 = QString("%1/Det2_%2_%3.%4")
        .arg(mSavePath)
        .arg(timestamp)
        .arg(m_detPara.measureTime)
        .arg(mfileFormat == Binary ? "dat" : "txt");
}

void CommandHelper::stopMeasure()
{
    // 发送停止指令
    if(m_detPara.transferMode == Order::Waveform){
        // 发送停止波形传输指令
        sendCommand(client_det1, Order::controlWaveform(Order::Stop),
                    "波形测量控制", QString("FPGA1 %1").arg(triggerModeText(Order::Stop)));
        sendCommand(client_det2, Order::controlWaveform(Order::Stop),
                    "波形测量控制", QString("FPGA2 %1").arg(triggerModeText(Order::Stop)));
    } else {
        // 发送停止能谱传输指令
        sendCommand(client_det1, Order::controlSpectrum(Order::Stop),
                    "能谱测量控制", QString("FPGA1 %1").arg(triggerModeText(Order::Stop)));
        sendCommand(client_det2, Order::controlSpectrum(Order::Stop),
                    "能谱测量控制", QString("FPGA2 %1").arg(triggerModeText(Order::Stop)));
    }
    qDebug() << "Measurement stopped.";
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
    ip_det1 = settings->getValueByPath("network/ip1").toString();
    ip_det2 = settings->getValueByPath("network/ip2").toString();
    ip_arm1 = settings->getValueByPath("network/ip_arm1").toString();
    ip_arm2 = settings->getValueByPath("network/ip_arm2").toString();
    ip_relay = settings->getValueByPath("network/ip_relay").toString();

    port_det1 = settings->getValueByPath("network/port_det1").toUInt();
    port_det2 = settings->getValueByPath("network/port_det2").toUInt();
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

void CommandHelper::handleDet1Data(const QByteArray &binaryData)
{
    //存储数据到mfileNameDet1文件中。
    QFile file(mfileNameDet1);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(binaryData);
        file.close();
    }
    else {
        qWarning() << "Failed to write data to file:" << mfileNameDet1;
    }
    // 处理FPGA板1的数据，根据当前传输模式选择解析器
    if (m_detPara.transferMode == Order::TransferMode::Waveform) {
        processWaveformData(1, m_det1Buffer, binaryData);
    } else {
        processSpec512Data(1, m_det1Buffer, binaryData);
    }
}

// 处理FPGA板2的数据
void CommandHelper::handleDet2Data(const QByteArray &binaryData)
{
    //存储数据到mfileNameDet2文件中。
    QFile file(mfileNameDet2);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(binaryData);
        file.close();
    }
    else {
        qWarning() << "Failed to write data to file:" << mfileNameDet2;
    }
    // 处理FPGA板2的数据，根据当前传输模式选择解析器
    if (m_detPara.transferMode == Order::TransferMode::Waveform) {
        // processWaveformData(2, m_det2Buffer, binaryData);
    } else {
        processSpec512Data(2, m_det2Buffer, binaryData);
    }
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

        if (buffer.size() < SpectrumPacketSize)
            return;

        const QByteArray packet = buffer.left(SpectrumPacketSize);
        if (packet.mid(SpectrumPacketSize - SpectrumTail.size(), SpectrumTail.size()) != SpectrumTail) {
            buffer.remove(0, SpectrumHeader.size());
            qWarning() << "Invalid spectrum packet tail from detector" << detectorIndex;
            continue;
        }

        parseSpectrumPacket(detectorIndex, packet);
        buffer.remove(0, SpectrumPacketSize);
    }
}

bool CommandHelper::parseSpectrumPacket(int detectorIndex, const QByteArray& packet)
{
    if (packet.size() != SpectrumPacketSize)
        return false;
    if (!packet.startsWith(SpectrumHeader) || !packet.endsWith(SpectrumTail))
        return false;

    const quint32 timeMs = readUInt32BE(packet.constData() + 4);
    const quint32 channelMask = readUInt32BE(packet.constData() + 8);
    const int channelNumber = channelNumberFromMask(channelMask);

    QVector<quint32> counts;
    counts.reserve(SpectrumChannelCount);

    const char* spectrumData = packet.constData() + 12;
    for (int i = 0; i < SpectrumChannelCount; ++i) {
        counts.append(readUInt32BE(spectrumData + i * 4));
    }

    // qInfo() << "Spectrum packet parsed. Detector:" << detectorIndex
    //         << "Channel:" << (channelNumber > 0 ? QString("CH%1").arg(channelNumber) : "Unknown")
    //         << "Time(ms):" << timeMs;
    emit sigSpectrumData(detectorIndex, channelNumber, timeMs, counts);
    return true;
}

void CommandHelper::processWaveformData(int detectorIndex, QByteArray& buffer, const QByteArray& data)
{
    buffer.append(data);

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
            qWarning() << "Invalid waveform packet tail from detector" << detectorIndex;
            continue;
        }

        // parse
        const char* p = packet.constData();
        const quint32 timeUnits = readUInt32BE(p + WaveformHeader.size());
        const quint32 channelMask = readUInt32BE(p + WaveformHeader.size() + 4);
        const int channelNumber = channelNumberFromMask(channelMask);

        QVector<quint16> samples;
        samples.reserve(254);
        const char* sampleData = p + WaveformHeader.size() + 8;
        for (int i = 0; i < 254; ++i) {
            samples.append(readUInt16BE(sampleData + i * 2));
        }

        emit sigWaveformData(detectorIndex, channelNumber, timeUnits, samples);
        buffer.remove(0, WaveformPacketSize);
    }
}

void CommandHelper::handleARM1Data(const QByteArray &binaryData)
{
    qDebug() << "Received data from ARM 1:" << binaryData.toHex(' ');
}

void CommandHelper::handleARM2Data(const QByteArray &binaryData)
{
    qDebug() << "Received data from ARM 2:" << binaryData.toHex(' ');
}
