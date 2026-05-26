/*
 * @Author: MrPan
 * @Date: 2026-03-23 10:31:29
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-19 15:27:23
 * @Description: 请填写简介
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
class CommandHelper;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void sigAppendMsg(const QString &msg, QtMsgType msgType);

private slots:
    void on_action_setting_triggered();
    void onSysTimerTimeout();

    void slotAppendMsg(const QString &msg, QtMsgType msgType);

    void on_btn_relayNetOpen_clicked();

    void on_btn_relayNetClose_clicked();

    void on_btn_startMeasure_clicked();

    void on_bt_powerOn_clicked();

    void on_bt_powerOff_clicked();

    void on_bt_connectDet_clicked();

    void on_bt_disconnectDet_clicked();

    void on_pushButton_clearSysLog_clicked();

    void on_pushButton_clearNetLog_clicked();

    void on_btn_stopMeasure_clicked();

private:
    struct SpectrumEntry {
        int detectorIndex = 0;
        quint32 timeMs = 0;
        QVector<quint32> counts;
    };

    static constexpr int kChannelsPerDetector = 16;
    static constexpr int kSpectrumChannelCount = 32; // 探测器1: 1~16，探测器2: 17~32
    static constexpr int kDetector2ChannelOffset = 16;

    int logicalChannelNumber(int detectorIndex, int channelNumber) const;
    void clearSpectrumData();
    void appendSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                            const QVector<quint32> &counts);
    void ensureSpectrumBinAddresses(int binCount);
    void updateSpecIdSpinBoxRange();
    void refreshSpectrumPlot();
    void refreshWaveformPlot();
    void resetWaveformCounters();
    void resetSpectrumSequenceTracking();
    void printWaveformCollectionSummary() const;
    void printSpectrumSequenceSummary() const;

    Ui::MainWindow *ui;
    CommandHelper *commandHelper = nullptr;//探测器网络
    // 定时测量定时器
    QTimer* measureTimer = nullptr;
    QElapsedTimer spectrumPlotThrottle;
    // 按逻辑通道(1~32)存储：探测器1为1~16，探测器2为17~32
    QVector<QVector<SpectrumEntry>> m_spectrumByChannel;
    QVector<quint32> m_spectrumBinAddresses; // 统一道址 1..N，绘图时复用
    QVector<QVector<quint32>> m_spectrumSequenceNumbersByChannel;
    QVector<QVector<quint32>> m_missingSpectrumNumbersByChannel;
    QVector<quint32> m_lastSpectrumSequenceByChannel;
    QVector<bool> m_hasSpectrumSequenceByChannel;
    // 波形按逻辑通道存储的最新一帧数据（覆盖式存储）
    QVector<QVector<quint16>> m_waveformByChannel;
    QVector<int> m_waveformCountByChannel;
    QVector<QVector<quint32>> m_waveformSequenceNumbersByChannel;
    QVector<QVector<quint32>> m_missingWaveformNumbersByChannel;
    QVector<quint32> m_lastWaveformSequenceByChannel;
    QVector<bool> m_hasWaveformSequenceByChannel;
    QTimer* waveformPlotTimer = nullptr;

    // 继电器电源开关状态
    bool replayPowerOn = false;
    // 探测器在线状态
    bool detectOnline[2] = {false, false};

    //系统时钟，目前用来随机产生arm传感器数据
    QTimer* sysTimer = nullptr;
    bool armSensorOnline[2] = {false, false};

};
#endif // MAINWINDOW_H
