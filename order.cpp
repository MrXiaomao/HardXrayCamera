#include "order.h"

namespace {
constexpr quint8 Header0 = 0x12;
constexpr quint8 Header1 = 0x34;
constexpr quint8 Header2 = 0x00;
constexpr quint8 Header3 = 0x0F;
constexpr quint8 Tail0 = 0xAB;
constexpr quint8 Tail1 = 0xCD;

void appendUInt16(QByteArray& data, quint16 value)
{
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>(value & 0xFF));
}

void appendUInt32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>((value >> 24) & 0xFF));
    data.append(static_cast<char>((value >> 16) & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>(value & 0xFF));
}
}

Order::Order()
    : waitingTime(500)
    , TransferModeSpectrum512(setTransferMode(Spectrum512))
    , TransferModeSpectrum16(setTransferMode(Spectrum16))
    , TransferModeWaveform(setTransferMode(Waveform))
    , SpectrumRefreshTime(setSpectrumRefreshTime(1000))
    , SpectrumTriggerThreshold(setSpectrumTriggerThreshold(100))
    , SpectrumDeadTime(setSpectrumDeadTime(50))
    , TriggerSignalTimeWidth(setTriggerSignalTimeWidth(16))
    , WaveformStop(controlWaveform(Stop))
    , WaveformSoftwareTrigger(controlWaveform(SoftwareTrigger))
    , WaveformHardwareTrigger(controlWaveform(HardwareTrigger))
    , SpectrumHardwareStart(controlSpectrum(HardwareTrigger))
    , SpectrumSoftwareStart(controlSpectrum(SoftwareTrigger))
    , SpectrumStop(controlSpectrum(Stop))
{
}

Order::~Order() = default;

QByteArray Order::setTransferMode(TransferMode mode)
{
    return makeCommand(0xFA, 0x13, static_cast<quint32>(mode));
}

QByteArray Order::setSpectrumRefreshTime(quint32 ms)
{
    return makeCommand(0xFA, 0x11, ms);
}

QByteArray Order::setSpectrumTriggerThreshold(quint16 threshold)
{
    return makeCommand(0xFA, 0x12, 0, threshold);
}

QByteArray Order::setSpectrumDeadTime(quint16 deadTime16ns)
{
    return makeCommand(0xFA, 0x14, 0, deadTime16ns);
}

QByteArray Order::setTriggerSignalTimeWidth(quint16 timeWidth16ns)
{
    return makeCommand(0xFF, 0xB0, 0, timeWidth16ns);
}

QByteArray Order::setTimeSpectrumRange(quint8 index, quint16 start, quint16 end)
{
    return makeCommand(0xFB, index, start, end);
}

QVector<QByteArray> Order::setTimeSpectrumRangeChannel(quint8 channelIndex,
                                                       const QVector<quint16>& boundaries)
{
    QVector<QByteArray> commands;
    if (channelIndex > 15 || boundaries.size() < 17)
        return commands;

    auto clampBin = [](quint16 value) -> quint16 {
        if (value < 1)
            return 1;
        if (value > 512)
            return 512;
        return value;
    };

    commands.reserve(9);
    for (int cmd = 0; cmd < 9; ++cmd) {
        const int firstIndex = cmd * 2;
        const int secondIndex = qMin(firstIndex + 1, 16);
        const quint8 commandIndex = static_cast<quint8>(channelIndex * 9 + cmd);
        commands.append(setTimeSpectrumRange(commandIndex,
                                           clampBin(boundaries.at(firstIndex)),
                                           clampBin(boundaries.at(secondIndex))));
    }
    return commands;
}

QVector<QByteArray> Order::setTimeSpectrumRanges(const QVector<quint16>& points)
{
    QVector<QByteArray> commands;
    if (points.size() < 2)
        return commands;

    const int rangeCount = qMin(points.size() - 1, 144);
    commands.reserve(rangeCount);
    for (int i = 0; i < rangeCount; ++i) {
        commands.append(setTimeSpectrumRange(static_cast<quint8>(i),
                                             points.at(i),
                                             points.at(i + 1)));
    }
    return commands;
}

QByteArray Order::setWaveThresholdPair(quint8 pairIndex, quint16 firstThreshold, quint16 secondThreshold)
{
    if (pairIndex < 1)
        pairIndex = 1;
    if (pairIndex > 8)
        pairIndex = 8;

    return makeCommand(0xFE, static_cast<quint8>(0x10 + pairIndex), firstThreshold, secondThreshold);
}

QVector<QByteArray> Order::setWaveThresholds(const QVector<quint16>& thresholds)
{
    QVector<QByteArray> commands;
    const int pairCount = qMin(thresholds.size() / 2, 8);
    commands.reserve(pairCount);

    for (int i = 0; i < pairCount; ++i) {
        commands.append(setWaveThresholdPair(static_cast<quint8>(i + 1),
                                             thresholds.at(i * 2),
                                             thresholds.at(i * 2 + 1)));
    }
    return commands;
}

QByteArray Order::controlWaveform(TriggerMode mode)
{
    if (mode == HardwareTrigger)
        return makeCommand(0xFE, 0x10, static_cast<quint32>(1));
    if (mode == SoftwareTrigger)
        return makeCommand(0xFE, 0x10, static_cast<quint32>(2));
    return makeCommand(0xFE, 0x10, static_cast<quint32>(0));
}

QByteArray Order::controlSpectrum(TriggerMode mode)
{
    return makeCommand(0xFF, 0xA0, static_cast<quint32>(mode));
}

QByteArray Order::makeCommand(quint8 type, quint8 code, quint32 value)
{
    QByteArray command;
    command.reserve(12);
    command.append(static_cast<char>(Header0));
    command.append(static_cast<char>(Header1));
    command.append(static_cast<char>(Header2));
    command.append(static_cast<char>(Header3));
    command.append(static_cast<char>(type));
    command.append(static_cast<char>(code));
    appendUInt32(command, value);
    command.append(static_cast<char>(Tail0));
    command.append(static_cast<char>(Tail1));
    return command;
}

QByteArray Order::makeCommand(quint8 type, quint8 code, quint16 highValue, quint16 lowValue)
{
    QByteArray command;
    command.reserve(12);
    command.append(static_cast<char>(Header0));
    command.append(static_cast<char>(Header1));
    command.append(static_cast<char>(Header2));
    command.append(static_cast<char>(Header3));
    command.append(static_cast<char>(type));
    command.append(static_cast<char>(code));
    appendUInt16(command, highValue);
    appendUInt16(command, lowValue);
    command.append(static_cast<char>(Tail0));
    command.append(static_cast<char>(Tail1));
    return command;
}
