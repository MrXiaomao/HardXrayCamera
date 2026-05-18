#pragma once

#include <QByteArray>
#include <QVector>
#include <QtGlobal>

class Order
{
public:
    enum TransferMode : quint8 {
        Spectrum512 = 0,
        Spectrum16 = 3,
        Waveform = 5
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
    static QByteArray setTimeSpectrumRange(quint8 index, quint16 start, quint16 end);
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
