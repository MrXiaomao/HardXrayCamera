#pragma once

#include <QByteArray>
#include <QVector>
#include <QtGlobal>

class Order
{
public:
    enum TransferMode : quint8 {
        Spectrum512 = 0, //512道能谱
        Waveform = 5, //波形图
        Spectrum16 = 3, //16道能谱
    };

    enum TriggerMode : quint32 {
        Stop = 0,
        SoftwareTrigger = 1,
        HardwareTrigger = 2
    };

    Order();
    ~Order();

    // Configuration commands
    static QByteArray setTransferMode(TransferMode mode);
    static QByteArray setSpectrumRefreshTime(quint32 ms);
    static QByteArray setSpectrumTriggerThreshold(quint16 threshold);
    static QByteArray setSpectrumDeadTime(quint16 deadTime16ns);
    static QByteArray setTriggerSignalTimeWidth(quint16 timeWidth16ns = 16);
    static QByteArray setTimeSpectrumRange(quint8 index, quint16 start, quint16 end);
    // 单通道 17 个道址边界 -> 9 条 0xFB 指令（序号 channelIndex*9 .. +8）
    static QVector<QByteArray> setTimeSpectrumRangeChannel(quint8 channelIndex,
                                                           const QVector<quint16>& boundaries);
    static QVector<QByteArray> setTimeSpectrumRanges(const QVector<quint16>& points);
    static QByteArray setWaveThresholdPair(quint8 pairIndex, quint16 firstThreshold, quint16 secondThreshold);
    static QVector<QByteArray> setWaveThresholds(const QVector<quint16>& thresholds);

    // Control commands
    static QByteArray controlWaveform(TriggerMode mode);
    static QByteArray controlSpectrum(TriggerMode mode);

    int waitingTime;

    QByteArray TransferModeSpectrum512;
    QByteArray TransferModeSpectrum16;
    QByteArray TransferModeWaveform;

    QByteArray SpectrumRefreshTime;
    QByteArray SpectrumTriggerThreshold;
    QByteArray SpectrumDeadTime;
    QByteArray TriggerSignalTimeWidth;

    QByteArray WaveformStop;
    QByteArray WaveformSoftwareTrigger;
    QByteArray WaveformHardwareTrigger;

    QByteArray SpectrumHardwareStart;
    QByteArray SpectrumSoftwareStart;
    QByteArray SpectrumStop;

private:
    static QByteArray makeCommand(quint8 type, quint8 code, quint32 value);
    static QByteArray makeCommand(quint8 type, quint8 code, quint16 highValue, quint16 lowValue);
};
