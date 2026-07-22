#include "dataprocessor.h"
#include <QDebug>
#include <cstring>

DataProcessor::DataProcessor(quint8 detectorIndex, QObject *parent)
    : QObject{parent}
    , mDetectorIndex(detectorIndex)
{
    m_samples.resize(580);
    m_workThread.start();
    moveToThread(&m_workThread);
    // 启动处理循环
    QMetaObject::invokeMethod(this, &DataProcessor::onProcessLoop, Qt::QueuedConnection);
}

DataProcessor::~DataProcessor()
{
    m_stop = true;
    m_condData.wakeOne(); // 唤醒线程退出
    if (m_workThread.isRunning()) {
        m_workThread.quit();
        m_workThread.wait();
    }
}

quint16 DataProcessor::readUInt16BE(const char* data)
{
    const uchar* p = reinterpret_cast<const uchar*>(data);
    return (static_cast<quint16>(p[0]) << 8) | static_cast<quint16>(p[1]);
}

int DataProcessor::channelNumberFromMask(quint32 channelMask)
{
    for (int bit = 0; bit < 16; ++bit) {
        if (channelMask & (1u << bit))
            return bit + 1;
    }
    return 0;
}

double DataProcessor::parseArm2Temperature(quint8 highByte, quint8 lowByte)
{
    //不再使用占位温度，直接返回 0xFF 0xFF 时的模拟温度
    // if (highByte == 0xFF && lowByte == 0xFF)
    //     return generateArm2StubTemperature();
    const quint16 rawValue = (static_cast<quint16>(highByte) << 8) | static_cast<quint16>(lowByte);
    return static_cast<double>(rawValue) / 10.0;
}

void DataProcessor::onProcessLoop()
{
    while (!m_stop) {
        QByteArray localData;
        quint64 epoch = 0;
        {
            QMutexLocker locker(&m_dataMutex);
            // 缓存为空则等待新数据
            while (!m_hasPendingData && !m_stop) {
                m_condData.wait(locker.mutex());
            }
            if (m_stop) break;

            // 取出所有缓存数据到本地副本后解锁处理，避免与 reset()/inputData 竞态
            m_pendingData.append(m_cacheBuffer);
            m_cacheBuffer.clear();
            m_hasPendingData = false;
            localData.swap(m_pendingData);
            epoch = m_resetEpoch.load();
        }

        if (localData.isEmpty())
            continue;

        // 处理数据
        if (mDetectorIndex==1)
            handleFpga1MainData(localData);
        else if (mDetectorIndex==2)
            handleFpga2MainData(localData);
        else if (mDetectorIndex==3)
            handleFpga1WaveData(localData);
        else if (mDetectorIndex==4)
            handleFpga2WaveData(localData);

        // 未凑齐的半包写回；若期间已 reset，则丢弃残留
        if (!localData.isEmpty()) {
            QMutexLocker locker(&m_dataMutex);
            if (epoch == m_resetEpoch.load())
                m_pendingData.swap(localData);
        }
    }
}

void DataProcessor::inputData(const QByteArray& data)
{
    if (m_stop) return;
    QMutexLocker locker(&m_dataMutex);
    m_cacheBuffer.append(data);
    m_hasPendingData = true;
    m_condData.wakeOne();
}

void DataProcessor::setTransferMode(Order::TransferMode mode)
{
    m_transferMode = mode;
}

void DataProcessor::setTriggerMode(Order::TriggerMode mode)
{
    m_trigMode = mode;
}

void DataProcessor::reset()
{
    mHardTriggered.store(false);
    {
        QMutexLocker locker(&m_dataMutex);
        ++m_resetEpoch;
        m_pendingData.clear();
        m_cacheBuffer.clear();
        m_hasPendingData = false;
    }

    if (mDetectorIndex == 1)
        qDebug() << "1#能谱收包缓存已重置";
    else if (mDetectorIndex == 2)
        qDebug() << "2#能谱收包缓存已重置";
    else if (mDetectorIndex == 3)
        qDebug() << "1#硬触发信号已重置";
    else if (mDetectorIndex == 4)
        qDebug() << "2#硬触发信号已重置";
}

void DataProcessor::handleFpga1MainData(QByteArray &binaryData)
{
    if (mIsUpgrading.load())
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    {
        // 处理水平相机主网口能谱数据，根据当前传输模式选择解析器
        if (m_transferMode == Order::TransferMode::Spectrum16) {
            processSpec16Data(1, binaryData);
        } else {
            processSpec512Data(1, binaryData);
        }
    }
}

// 处理FPGA主板2主网口能谱数据
void DataProcessor::handleFpga2MainData(QByteArray &binaryData)
{
    if (mIsUpgrading.load())
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    // 处理垂直相机主网口能谱数据，根据当前传输模式选择解析器
    if (m_transferMode == Order::TransferMode::Spectrum16) {
        processSpec16Data(2, binaryData);
    } else {
        processSpec512Data(2, binaryData);
    }
}

// 处理FPGA主板1副网口波形数据
void DataProcessor::handleFpga1WaveData(QByteArray &binaryData)
{
    if (mIsUpgrading.load())
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    // 处理水平相机副网口波形数据
    processWaveformData(1, binaryData);
}

// 处理FPGA主板2副网口波形数据
void DataProcessor::handleFpga2WaveData(QByteArray &binaryData)
{
    if (mIsUpgrading.load())
    {
        emit sigOTAUpgradeData(mDetectorIndex, binaryData);
        return;
    }

    // 处理垂直相机副网口波形数据
    processWaveformData(2, binaryData);
}

// 处理512道能谱数据，按照协议解析出时间戳、通道号和计数，并通过信号发送给界面更新
void DataProcessor::processSpec512Data(int detectorIndex, QByteArray& buffer)
{
    int offset = 0;
    auto hasHeadAt = [&](int at, const QByteArray& head) -> bool {
        return (buffer.size() - at >= head.size()) &&
               (memcmp(buffer.constData() + at, head.constData(), static_cast<size_t>(head.size())) == 0);
    };

    while (true) {
        // 先判断总数据包大小
        if (buffer.size() - offset < Spectrum512PacketSize)
            break; // 数据长度不足，等待下次处理

        // 再判断包头
        if (hasHeadAt(offset, SpectrumHeader)) {

            // 接着判断包尾
            if (buffer.mid(offset + Spectrum512PacketSize - SpectrumTail.size(), SpectrumTail.size()) != SpectrumTail) {
                qWarning() << "Invalid 512-bin spectrum packet tail from detector" << detectorIndex;

                offset += static_cast<int>(SpectrumHeader.size());
                continue;
            }

            // parse
            const QByteArray packet = buffer.mid(offset, Spectrum512PacketSize);
            parseSpectrum512Packet(detectorIndex, packet);
            offset += static_cast<int>(Spectrum512PacketSize);

        } else {
            // 包头不正确
            offset++;
        }

    }

    if (offset > 0)
        buffer.remove(0, offset);

    // while (buffer.size() >= SpectrumHeader.size()) {
    //     const int headerIndex = buffer.indexOf(SpectrumHeader);
    //     if (headerIndex < 0) {
    //         buffer.clear();
    //         return;
    //     }

    //     if (headerIndex > 0)
    //         buffer.remove(0, headerIndex);

    //     if (buffer.size() < Spectrum512PacketSize)
    //         return;

    //     const QByteArray packet = buffer.left(Spectrum512PacketSize);
    //     if (packet.mid(Spectrum512PacketSize - SpectrumTail.size(), SpectrumTail.size()) != SpectrumTail) {
    //         // 包尾不对，继续寻找下一个包头
    //         buffer.remove(0, SpectrumHeader.size());
    //         qWarning() << "Invalid 512-bin spectrum packet tail from detector" << detectorIndex;
    //         continue;
    //     }

    //     parseSpectrum512Packet(detectorIndex, packet);
    //     buffer.remove(0, Spectrum512PacketSize);
    // }
}

bool DataProcessor::parseSpectrum512Packet(int detectorIndex, const QByteArray& packet)
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

    const char* spectrumData = packet.constData() + 6;
    for (int i = 0; i < Spectrum512BinCount; ++i) {
        counts.append(readUInt16BE(spectrumData + i * 2));        
    }

    emit sigSpectrumData(detectorIndex, channelNumber, timeMs, counts);
    return true;
}

// 16道能谱：包头(2) + 通道号(2) + 时间(2) + 16道计数(32) + 包尾(2) = 40 字节
void DataProcessor::processSpec16Data(int detectorIndex, QByteArray& buffer)
{
    int offset = 0;
    auto hasHeadAt = [&](int at, const QByteArray& head) -> bool {
        return (buffer.size() - at >= head.size()) &&
               (memcmp(buffer.constData() + at, head.constData(), static_cast<size_t>(head.size())) == 0);
    };

    while (true) {
        // 先判断总数据包大小
        if (buffer.size() - offset < Spectrum16PacketSize)
            break; // 数据长度不足，等待下次处理

        // 再判断包头
        if (hasHeadAt(offset, SpectrumHeader)) {

            // 接着判断包尾
            if (buffer.mid(offset + Spectrum16PacketSize - SpectrumTail.size(), SpectrumTail.size()) != SpectrumTail) {
                qWarning() << "Invalid 16-bin spectrum packet tail from detector" << detectorIndex;

                offset += static_cast<int>(SpectrumHeader.size());
                continue;
            }

            // parse
            const QByteArray packet = buffer.mid(offset, Spectrum16PacketSize);
            parseSpectrum16Packet(detectorIndex, packet);
            offset += static_cast<int>(Spectrum16PacketSize);

        } else {
            // 包头不正确
            offset++;
        }

    }

    if (offset > 0)
        buffer.remove(0, offset);

    // while (buffer.size() >= SpectrumHeader.size()) {
    //     const int headerIndex = buffer.indexOf(SpectrumHeader);
    //     if (headerIndex < 0) {
    //         buffer.clear();
    //         return;
    //     }

    //     if (headerIndex > 0)
    //         buffer.remove(0, headerIndex);

    //     if (buffer.size() < Spectrum16PacketSize)
    //         return;

    //     const QByteArray packet = buffer.left(Spectrum16PacketSize);
    //     if (packet.mid(Spectrum16PacketSize - SpectrumTail.size(), SpectrumTail.size()) != SpectrumTail) {
    //         buffer.remove(0, SpectrumHeader.size());
    //         qWarning() << "Invalid 16-bin spectrum packet tail from detector" << detectorIndex;
    //         continue;
    //     }

    //     parseSpectrum16Packet(detectorIndex, packet);
    //     buffer.remove(0, Spectrum16PacketSize);
    // }
}

bool DataProcessor::parseSpectrum16Packet(int detectorIndex, const QByteArray& packet)
{
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
    const char* spectrumData = packet.constData() + 6;
    for (int i = 0; i < Spectrum16BinCount; ++i) {
        counts.append(readUInt16BE(spectrumData + i * 2));        
    }

    emit sigSpectrumData(detectorIndex, channelNumber, timeMs, counts);
    return true;
}

void DataProcessor::processWaveformData(int detectorIndex, QByteArray& buffer)
{
    if (m_trigMode == Order::TriggerMode::HardwareTrigger){
        if (!mHardTriggered.load())
        {
            static const QByteArray hardTriggerCommand =
                QByteArray::fromHex("12 34 00 AB FF C0 00 00 00 01 AB CD");
            const int cmdSize = hardTriggerCommand.size();

            // 波形数据来临之前先判断硬触发信号（按实际位置切除，避免 TCP 半包被误清）
            const int idx = buffer.indexOf(hardTriggerCommand);
            if (idx >= 0) {
                buffer.remove(0, idx + cmdSize);

                qDebug().nospace() << "探测器#" << detectorIndex << "收到硬触发信号";
                emit sigHardTriggeredSignalReceived();
                mHardTriggered.store(true);
            } else {
                // 保留可能构成指令前缀的尾部，等待后续字节拼完整包；其余噪声丢弃
                int keep = 0;
                const int maxKeep = cmdSize - 1;
                for (int len = qMin(maxKeep, buffer.size()); len >= 1; --len) {
                    if (memcmp(hardTriggerCommand.constData(),
                               buffer.constData() + buffer.size() - len,
                               static_cast<size_t>(len)) == 0) {
                        keep = len;
                        break;
                    }
                }
                if (keep > 0)
                    buffer.remove(0, buffer.size() - keep);
                else
                    buffer.clear();
                return;
            }
        }
    }

    int offset = 0;
    auto hasHeadAt = [&](int at, const QByteArray& head) -> bool {
        return (buffer.size() - at >= head.size()) &&
               (memcmp(buffer.constData() + at, head.constData(), static_cast<size_t>(head.size())) == 0);
    };

    while (true) {
        // 先判断总数据包大小
        if (buffer.size() - offset < WaveformPacketSize)
            break; // 数据长度不足，等待下次处理

        // 再判断包头
        if (hasHeadAt(offset, WaveformHeader)) {

            const char* p = buffer.constData() + offset;
            // 接着判断包尾
            if (buffer.mid(offset + WaveformPacketSize - WaveformTail.size(), WaveformTail.size()) != WaveformTail) {
                //打印通道号和时间戳
                const quint32 channelMask = readUInt16BE(p + WaveformHeader.size());
                const quint32 timeUnits = readUInt16BE(p + WaveformHeader.size() + 2); // 时间单位，10ms
                const int channelNumber = channelNumberFromMask(channelMask);
                //qWarning() << "Invalid waveform packet tail from detector" << detectorIndex << "Channel:" << channelNumber << "Time(units):" << timeUnits;

                offset += static_cast<int>(WaveformHeader.size());
                continue;
            }

            // parse
            const quint32 channelMask = readUInt16BE(p + WaveformHeader.size());
            const quint32 timeUnits = readUInt16BE(p + WaveformHeader.size() + 2)*10;
            const int channelNumber = channelNumberFromMask(channelMask);

            //QVector<quint16> samples;
            //samples.reserve(580);

            const char* sampleData = p + WaveformHeader.size() + 4;
            for (int i = 0; i < 580; ++i) {
                //samples.append(readUInt16BE(sampleData + i * 2));
                m_samples[i] = readUInt16BE(sampleData + i * 2);
            }

            emit sigWaveformData(detectorIndex, channelNumber, timeUnits, m_samples);
            offset += static_cast<int>(WaveformPacketSize);

        } else {
            // 包头不正确
            offset++;
        }

    }

    if (offset > 0)
        buffer.remove(0, offset);

    // while (buffer.size() >= WaveformHeader.size()) {
    //     const int headerIndex = buffer.indexOf(WaveformHeader);
    //     if (headerIndex < 0) {
    //         buffer.clear();
    //         return;
    //     }

    //     if (headerIndex > 0)
    //         buffer.remove(0, headerIndex);

    //     if (buffer.size() < WaveformPacketSize)
    //         return;

    //     const QByteArray packet = buffer.left(WaveformPacketSize);
    //     if (packet.mid(WaveformPacketSize - WaveformTail.size(), WaveformTail.size()) != WaveformTail) {
    //         buffer.remove(0, WaveformHeader.size());
    //         //打印通道号和时间戳
    //         const char* p = packet.constData();
    //         const quint32 channelMask = readUInt16BE(p + WaveformHeader.size());
    //         const quint32 timeUnits = readUInt16BE(p + WaveformHeader.size() + 2); // 时间单位，10ms
    //         const int channelNumber = channelNumberFromMask(channelMask);
    //         qWarning() << "Invalid waveform packet tail from detector" << detectorIndex << "Channel:" << channelNumber << "Time(units):" << timeUnits;
    //         continue;
    //     }

    //     // parse
    //     const char* p = packet.constData();
    //     const quint32 channelMask = readUInt16BE(p + WaveformHeader.size());
    //     const quint32 timeUnits = readUInt16BE(p + WaveformHeader.size() + 2)*10;
    //     const int channelNumber = channelNumberFromMask(channelMask);

    //     QVector<quint16> samples;
    //     samples.reserve(580);

    //     QStringList strCounts;
    //     strCounts << QString::number(timeUnits/10) << QString::number(channelNumber);
    //     const char* sampleData = p + WaveformHeader.size() + 4;
    //     for (int i = 0; i < 580; ++i) {
    //         samples.append(readUInt16BE(sampleData + i * 2));
    //         strCounts << QString::number(readUInt16BE(sampleData + i * 2));
    //     }

    //     emit sigWaveformData(detectorIndex, channelNumber, timeUnits, samples);
    //     buffer.remove(0, WaveformPacketSize);
    // }
}
