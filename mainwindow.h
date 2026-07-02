/*
 * @Author: MrPan
 * @Date: 2026-03-23 10:31:29
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-06-10 18:32:22
 * @Description: 请填写简介
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <QStateMachine>
#include "globalsettings.h"
class CommandHelper;
class UdpShotReceiver;

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

public:
    virtual void closeEvent(QCloseEvent *event) override;

private slots:
    void slotAppendMsg(const QString &msg, QtMsgType msgType);

    void on_pushButton_clearSysLog_clicked();

    void on_pushButton_clearNetLog_clicked();

    // 测量定时器到时自动停止
    void onMeasureTimerTimeout();

    // CommandHelper 设备状态与数据回调
    void onRelayStatusChanged(bool on);
    void onRelayPowerStatusChanged(bool on);
    void onDetector1StatusChanged(bool on);
    void onDetector2StatusChanged(bool on);
    void onDetector3StatusChanged(bool on);
    void onDetector4StatusChanged(bool on);
    void onDetector1ConnectFault();
    void onDetector2ConnectFault();
    void onDetector3ConnectFault();
    void onDetector4ConnectFault();
    void onArm1StatusChanged(bool on);
    void onArm2StatusChanged(bool on);
    void onArm1SensorData(const QVector<double>&/*温度*/, const QVector<double>&/*电压*/, const QVector<double>&/*电流*/);
    void onArm2SensorData(const QVector<double>&/*温度*/, const QVector<double>&/*电压*/, const QVector<double>&/*电流*/);
    void onSpectrumDataReceived(int detectorIndex, int channelNumber, quint32 timeMs,
                                const QVector<quint32> &counts);
    void onWaveformDataReceived(int detectorIndex, int channelNumber, quint32 timeUnits,
                                const QVector<quint16> &samples);

    // 能谱/波形显示控件联动
    void onChannelSpinBoxChanged(int value);

    void onUdpDatagramReceived(const QString &asciiText, const QString &senderInfo);
    void onUdpShotNumberChanged(const QString &shotNumber);
    void onUdpBindStateChanged(bool bound, const QString &message);
    quint16 udpBroadcastPort() const;
    void startUdpListening();
    void stopUdpListening();

    bool startMeasureInternal();
    bool buildDetParameter(DetParameter &detPara, Order::TriggerMode trigMode) const;
    void triggerAutoMeasureFromShot(const QString &shotNumber);
    void stopAutoMeasureSession();
    bool isMeasureSessionActive() const;
    int measureDurationMs() const;
    void startMeasureDurationTimer();
    void saveShotNumberFile(const QString &shotNumber) const;
    void appendUdpLog(const QString &line);

    void on_action_hardwareSetting_triggered();

    void on_actionFPGA_triggered();

    void on_action_relayNetOpen_triggered();

    void on_action_relayNetClose_triggered();

    void on_action_powerOn_triggered();

    void on_action_powerOff_triggered();

    void on_action_connectDet_triggered();

    void on_action_disconnectDet_triggered();

    void on_action_startMeasure_triggered();

    void on_action_stopMeasure_triggered();

    void on_btn_generateProfile_clicked();

    void on_comboBox_2_currentIndexChanged(int index);

    void on_action_connectMonitor_triggered();

    void on_action_disconnectMonitor_triggered();

    void on_action_exit_triggered();

    void on_action_about_triggered();

    void on_action_analyze_triggered();

private:
    struct EnergyCalibration {
        double k = 1.0;
        double b = 0.0;
    };

    struct ProfileSnapshot {
        quint32 timeMs = 0;
        QVector<quint64> counts; // 32 个逻辑通道在能量区间内的总计数
    };

    struct SpectrumEntry {
        int detectorIndex = 0;
        quint32 timeMs = 0; //单位，ms
        QVector<quint32> counts;
    };

    struct WaveformEntry {
        int detectorIndex = 0;
        quint32 timeUnits = 0; // 时间单位，ms
        QVector<quint16> samples;
    };

    static constexpr int kChannelsPerDetector = 16;
    static constexpr int kSpectrumChannelCount = 32; // 探测器1: 1~16，探测器2: 17~32
    static constexpr int kDetector2ChannelOffset = 16; // 探测器2的逻辑通道号相对于物理通道的偏移

    static constexpr int kSpectrum512BinCount = 512;
    static constexpr int kProfileChannelCount = 32;
    static constexpr int kVerticalCameraChannels = 16;
    static constexpr double kProfileZMin = -8.0;
    static constexpr double kProfileZMax = 8.0;
    static constexpr double kWaveformSampleIntervalNs = 16.0;
    static constexpr int kWaveformSampleCount = 1024;
    static constexpr int kWaveformMaxDisplayNs =
        static_cast<int>(kWaveformSampleCount * kWaveformSampleIntervalNs);

    int logicalChannelNumber(int detectorIndex, int channelNumber) const;
    void clearSpectrumData();
    void clearWaveformData();
    void appendSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                            const QVector<quint32> &counts);
    void appendWaveformData(int detectorIndex, int channelNumber, quint32 timeUnits,
                            const QVector<quint16> &samples);
    void ensureSpectrumBinAddresses(int binCount);
    void updateSpecIdSpinBoxRange();
    void updateWaveIdSpinBoxRange();
    void updateSpectrumRefreshIntervalRange();
    void updateHxrDisplayBinControls();
    void updateProfileControls();
    void updateUnattendedControls();
    int hxrDisplayBinCount() const;
    bool loadEnergyCalibration(QVector<EnergyCalibration> &calibration, QString *errorMessage) const;
    QString energyCalibrationFilePath() const;
    void energyToBinRange(double energyLeft, double energyRight, const EnergyCalibration &cal,
                          int &binStart, int &binEnd) const;
    quint64 sumCountsInBinRange(const QVector<quint32> &counts, int binStart, int binEnd) const;
    double profilePointPosition(int pointIndex) const;
    void generateProfileSnapshots();
    void updateProfileIdSpinBoxRange();
    void clearProfileData();
    void refreshProfilePlot();
    void refreshSpectrumPlot();
    void refreshWaveformPlot();
    void resetWaveformCounters();
    void resetSpectrumSequenceTracking();
    void printWaveformCollectionSummary() const;
    void printSpectrumSequenceSummary() const;
    void showHardwareStartupWaitDialog();
    void syncPowerSwitchFromRelay(bool powerOn);
    void setPowerSwitchEnabled(bool enabled);
    void syncDetectorConnectButton();
    void syncArmMonitorButton();
    void loadMonitorAlarmSettings();
    void saveMonitorAlarmSettings();
    void loadMeasureSettings();
    void saveMeasureSettings();
    QString armMonitorSaveDir() const;
    void saveArmMonitorData(int armIndex, const QVector<double> &temperature,
                            const QVector<double> &voltage, const QVector<double> &current);
    void checkArmMonitorAlarm(int armIndex, const QVector<double> &temperature,
                              const QVector<double> &voltage, const QVector<double> &current);
    void handleArmSensorData(int armIndex, const QVector<double> &temperature,
                             const QVector<double> &voltage, const QVector<double> &current);

    Ui::MainWindow *ui;
    CommandHelper *commandHelper = nullptr;//探测器网络
    UdpShotReceiver *m_udpShotReceiver = nullptr;
    QString m_currentShotNumber;
    bool m_udpListening = false;
    DetParameter mdetPara; //测量参数，包含触发模式、传输模式、测量时长等
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
    // 按逻辑通道(1~32)存储波形历史
    QVector<QVector<WaveformEntry>> m_waveformByChannel;
    QVector<QVector<quint32>> m_waveformSequenceNumbersByChannel;
    QVector<QVector<quint32>> m_missingWaveformNumbersByChannel;
    QVector<quint32> m_lastWaveformSequenceByChannel;
    QVector<bool> m_hasWaveformSequenceByChannel;
    QTimer* waveformPlotTimer = nullptr;
    QVector<EnergyCalibration> m_energyCalibration;
    QVector<ProfileSnapshot> m_profileSnapshots;

    // 继电器电源开关状态
    bool replayPowerOn = false;
    bool replayOnline = false;
    // 探测器在线状态
    bool detectOnline[4] = {false, false, false, false};

    //系统时钟，目前用来随机产生arm传感器数据
    bool armSensorOnline[2] = {false, false};
    bool m_armMonitorInAlarm[2] = {false, false};

    // 无人值守
    QTimer *startTimer = nullptr;
    QTimer *stopTimer = nullptr;
    bool isTaskRunning = false;

    enum class AutoMeasureState { Idle, WaitingShot, Measuring };
    AutoMeasureState m_autoMeasureState = AutoMeasureState::Idle;
    bool m_autoMeasureDurationTimerStarted = false;

public:
    // 无人值守
    bool m_enableAutoMated;
    QStateMachine *machine;
    QState *stIdle, *stStep1, *stStep2, *stStep3, *stStep4, *stFinish;
    void initStateMachine();

Q_SIGNALS:
    // 无人值守共分四步：
    void step1Finished();// 1、连接远程控制
    void step2Finished();// 2、开启电源
    void step3Finished();// 3、连接采集系统
    void step4Finished();// 4、连接实时监测系统
};
#endif // MAINWINDOW_H
