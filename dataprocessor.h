#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QObject>
#include <QMutex>
#include <QFile>
#include <QThread>
#include <QWaitCondition>
#include <QVector>
#include <QMetaType>
#include <atomic>
#include "order.h"

struct WaveformFrame {
    int detectorIndex = 0;
    int channelNumber = 0;
    quint32 timeUnits = 0;
    QVector<quint16> samples;
};
Q_DECLARE_METATYPE(WaveformFrame)
Q_DECLARE_METATYPE(QVector<WaveformFrame>)

struct SpectrumFrame {
    int detectorIndex = 0;
    int channelNumber = 0;
    quint32 timeMs = 0;
    QVector<quint32> counts;
};
Q_DECLARE_METATYPE(SpectrumFrame)
Q_DECLARE_METATYPE(QVector<SpectrumFrame>)

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
    static constexpr int WaveformBatchFlushSize = 480;
    // 能谱合帧：积分时长 >=1s 用 16；否则（如 1ms）用 160
    static constexpr int SpectrumBatchFlushSlow = 16;
    static constexpr int SpectrumBatchFlushFast = 480;
    const QByteArray WaveformHeader = QByteArray::fromHex("aa bb");
    const QByteArray WaveformTail = QByteArray::fromHex("cc dd");

    explicit DataProcessor(quint8 detectorIndex, QObject *parent = nullptr);
    ~DataProcessor();

    void setTransferMode(Order::TransferMode mode);
    void setTriggerMode(Order::TriggerMode mode);
    void setSpectrumBatchSize(int size);
    void reset();
    void prepareStartMeasure();
    void prepareStopMeasure();

    void enterQueryMode();
    void leaveQueryMode();

signals:
    void sigWaveformData(int detectorIndex, int channelNumber, quint32 timeUnits,
                         const QVector<quint16>& samples);
    void sigWaveformBatch(const QVector<WaveformFrame>& frames);

    void sigSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                         const QVector<quint32>& counts);
    void sigSpectrumBatch(const QVector<SpectrumFrame>& frames);

    void sigHardTriggeredSignalReceived();

    void sigSendNextCommand();
    void sigMessureStarted();
    void sigMessureStoped();

    void sigSpectrumRefreshTimelengthAck(quint16);
    void sigSpecSpectrumTriggerThresholdAck(quint16);
    void sigSpecSpectrumDieTimelengthAck(quint16);
    void sigSpecTriggerSignalTimeWidthAck(quint16);
    void sigSpecSpectrumTimeWindowAck(quint8, quint16, quint16);
    void sigOTAVersionAck(quint8);

public slots:
    void inputData(const QByteArray& data);
    void handleFpga1MainData(QByteArray &binaryData);
    void handleFpga2MainData(QByteArray &binaryData);
    void handleFpga1WaveData(QByteArray &binaryData);
    void handleFpga2WaveData(QByteArray &binaryData);
    void onProcessLoop();
    void clearWaveformBatch();
    void clearSpectrumBatch();

private:
    void handleMeasureStopAck(const QString& detName);
    void flushWaveformBatch();
    void flushSpectrumBatch();
    void enqueueSpectrumFrame(int detectorIndex, int channelNumber, quint32 timeMs,
                              QVector<quint32> &&counts);
    QThread m_workThread;

    QByteArray m_pendingData;

    std::atomic_bool m_stop = false;
    std::atomic_bool m_hasPendingData = false;
    std::atomic<quint64> m_resetEpoch{0};
    QWaitCondition m_condData;

    QByteArray m_cacheBuffer;
    Order::TransferMode m_transferMode = Order::Spectrum512;
    Order::TriggerMode m_trigMode = Order::SoftwareTrigger;

    quint8 mDetectorIndex = 1;
    mutable QMutex m_dataMutex;
    std::atomic_bool mHardTriggered = false;

    QVector<quint16> m_samples;
    QVector<WaveformFrame> m_waveBatch;
    QVector<SpectrumFrame> m_specBatch;
    std::atomic_int m_spectrumBatchFlushSize{SpectrumBatchFlushFast};
    std::atomic_bool m_measureStarted = false;
    std::atomic_bool m_measureStopPrepared = false;

    std::atomic_bool m_lastCommandIsQueryVersion = false;
    std::atomic_bool m_isQueryMode = false;

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
