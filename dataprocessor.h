#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QObject>
#include <QMutex>
#include <QFile>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include "order.h"

class DataProcessor : public QObject
{
    Q_OBJECT
public:
    static constexpr int Spectrum512PacketSize = 1032;
    static constexpr int Spectrum512BinCount = 512;
    static constexpr int Spectrum16PacketSize = 40; // 2+2+2+16*2+2
    static constexpr int Spectrum16BinCount = 16;
    const QByteArray SpectrumHeader = QByteArray::fromHex("aa bb");
    const QByteArray SpectrumTail = QByteArray::fromHex("cc dd");

    // Waveform packet constants
    static constexpr int WaveformPacketSize = 1168;
    const QByteArray WaveformHeader = QByteArray::fromHex("aa bb");
    const QByteArray WaveformTail = QByteArray::fromHex("cc dd");

    explicit DataProcessor(quint8 detectorIndex, QObject *parent = nullptr);
    ~DataProcessor();

    void inputData(const QByteArray&);
    void setTransferMode(Order::TransferMode mode);
    void setTriggerMode(Order::TriggerMode mode);
    void reset();

signals:
    // 波形数据: timeUnits 单位为 500us, samples 为 1024 个采样点
    void sigWaveformData(int detectorIndex, int channelNumber, quint32 timeUnits,
                         const QVector<quint16>& samples);

    // 能谱数据
    void sigSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                         const QVector<quint32>& counts);

    void sigOTAUpgradeData(quint8, const QByteArray& data); // 上报OTA升级数据

    void sigHardTriggeredSignalReceived();

public slots:
    // 继电器数据处理
    void handleFpga1MainData(QByteArray &binaryData);
    void handleFpga2MainData(QByteArray &binaryData);
    void handleFpga1WaveData(QByteArray &binaryData);
    void handleFpga2WaveData(QByteArray &binaryData);
    void onProcessLoop();

private:
    QThread m_workThread;

    QByteArray m_pendingData;

    // 线程退出标识
    std::atomic_bool m_stop = false;
    std::atomic_bool m_hasPendingData = false;
    // reset() 时递增，避免处理线程把残留半包写回已清空的缓冲
    std::atomic<quint64> m_resetEpoch{0};
    QWaitCondition m_condData;

    QByteArray m_cacheBuffer;
    Order::TransferMode m_transferMode;
    Order::TriggerMode m_trigMode;

    std::atomic_bool mIsUpgrading = false;
    // std::atomic_bool measure_started = false;
    quint8 mDetectorIndex = 1;
    //mutable QMutex m_measurementMutex;
    mutable QMutex m_dataMutex;
    //硬触发信号
    std::atomic_bool mHardTriggered = false;

    QVector<quint16> m_samples;

    quint16 readUInt16BE(const char* data);
    int channelNumberFromMask(quint32 channelMask);
    double parseArm2Temperature(quint8 highByte, quint8 lowByte);

    void processSpec16Data(int detectorIndex, QByteArray& buffer);
    bool parseSpectrum16Packet(int detectorIndex, const QByteArray& packet);

    void processSpec512Data(int detectorIndex, QByteArray& buffer);
    bool parseSpectrum512Packet(int detectorIndex, const QByteArray& packet);

    void processWaveformData(int detectorIndex, QByteArray& buffer);
};

#endif // DATAPROCESSOR_H
