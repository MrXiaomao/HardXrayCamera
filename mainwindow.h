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

// 自定义X轴格式化器：将数值转换为CH1-CH18的标签
#ifdef QT_DATAVISUALIZATION_LIB
#include <QtDataVisualization>
#include <QSurface>
using namespace QtDataVisualization;

#include <QtDataVisualization/Q3DInputHandler>
#include <QtDataVisualization/Q3DSurface>
#include <QMouseEvent>
#include <QAction>
#include <QMenu>
class QValue3DAxisFormatterX: public QtDataVisualization::QValue3DAxisFormatter
{
   Q_OBJECT
public:
   explicit QValue3DAxisFormatterX(QObject *parent = nullptr) : QValue3DAxisFormatter(parent) {}

   virtual QString stringForValue(qreal value, const QString &/*format*/) const override
   {
       if (value >= 1 && value <= 14)
           return QString("CH %1").arg(QString::number(value / 1.0, 'f', 0));
       else
           return QString();
   }
};

class CustomSurface : public QtDataVisualization::Q3DSurface {
   Q_OBJECT
public:
   explicit CustomSurface(const QSurfaceFormat *format = nullptr, QWindow *parent = nullptr)
        : QtDataVisualization::Q3DSurface(format, parent) {

   }

protected:
   void mousePressEvent(QMouseEvent *event) override {
       // 交换左右键的按下状态
       if (event->button() == Qt::LeftButton) {
           // 模拟右键按下（触发旋转）
           QMouseEvent fakeRightPress(event->type(), event->pos(),
                                      Qt::RightButton, Qt::RightButton, event->modifiers());
           Q3DSurface::mousePressEvent(&fakeRightPress);
       } else if (event->button() == Qt::RightButton) {
           // 模拟左键按下（默认是选择/平移，此处改为旋转）
           QMouseEvent fakeLeftPress(event->type(), event->pos(),
                                     Qt::LeftButton, Qt::LeftButton, event->modifiers());
           Q3DSurface::mousePressEvent(&fakeLeftPress);

           QMenu menu(nullptr);
           QAction *resetAct = menu.addAction("恢复视图");
           connect(resetAct, &QAction::triggered, this, [=](){
               scene()->activeCamera()->setCameraPosition(-60.0f, 30.0f, 125.0f);
           });
           menu.exec(event->globalPos());
       } else {
           Q3DSurface::mousePressEvent(event);
       }
   }

   void mouseMoveEvent(QMouseEvent *event) override {
       // 移动时保持交换后的按键逻辑
       if (event->buttons() & Qt::LeftButton) {
           QMouseEvent fakeRightMove(event->type(), event->pos(),
                                     Qt::RightButton, Qt::RightButton, event->modifiers());
           Q3DSurface::mouseMoveEvent(&fakeRightMove);
       } else if (event->buttons() & Qt::RightButton) {
           QMouseEvent fakeLeftMove(event->type(), event->pos(),
                                    Qt::LeftButton, Qt::LeftButton, event->modifiers());
           Q3DSurface::mouseMoveEvent(&fakeLeftMove);
       } else {
           Q3DSurface::mouseMoveEvent(event);
       }
   }
};
#endif //QT_DATAVISUALIZATION

#include <QVector>
#include <cassert>

#include <QVector>
#include <cassert>

template<typename T>
class DynamicPreallocArray {
public:
    static constexpr int FIXED_CHANNEL_COUNT = 32;

    // 构造时传入自定义的单通道预分配容量，灵活适配1000/10000/100000等不同采集量级
    explicit DynamicPreallocArray(int perChannelPreallocSize) {
        resize(FIXED_CHANNEL_COUNT);
        resizePreallocSize(perChannelPreallocSize);
    }

    void resize(int newChannelCount) {
        Q_ASSERT_X(newChannelCount >= 0, "DynamicSpectrumArray::resize", "通道数不能为负数");
        if (newChannelCount == m_channelCount) return;

        // 新建临时数组搬移已有数据，避免旧数据丢失
        QVector<T>* newData = new QVector<T>[newChannelCount];
        int copyCount = qMin(newChannelCount, m_channelCount);
        for (int i = 0; i < copyCount; i++) {
            newData[i] = std::move(m_data[i]);
        }
        // 新增通道自动执行预分配
        for (int i = copyCount; i < newChannelCount; i++) {
            newData[i].reserve(m_perChannelCapacity);
        }
        // 释放旧资源，替换内部指针
        delete[] m_data;
        m_data = newData;
        m_channelCount = newChannelCount;
    }

    int size() const noexcept { return m_channelCount; }

    // 支持运行时动态重新配置预分配大小，所有旧数据自动清空，重新申请统一容量
    void resizePreallocSize(int newPreallocSize) {
        assert(newPreallocSize > 0 && "预分配容量必须大于0");
        m_perChannelCapacity = newPreallocSize;
        // 批量重新为32个通道申请指定大小的连续内存，一次性完成无碎片
        for (int ch = 0; ch < m_channelCount; ch++) {
            m_data[ch].clear();
            m_data[ch].reserve(m_perChannelCapacity);
        }
    }

    // 追加元素接口，自动校验容量边界
    void append(int channelIdx, const T& entry) {
        assert(channelIdx >= 0 && channelIdx < m_channelCount);
        assert(m_data[channelIdx].size() < m_perChannelCapacity && "单通道预分配容量已满");
        m_data[channelIdx].push_back(entry);
    }

    // 极速清空：仅重置size到0，永远保留当前设定的预分配容量，不释放内存
    void clear() {
        for (int ch = 0; ch < m_channelCount; ch++) {
            m_data[ch].resize(0);
        }
    }

    int size(int channelIdx) const {
        assert(channelIdx >= 0 && channelIdx < m_channelCount);
        return m_data[channelIdx].size();
    }

    // 补上迭代器接口，适配范围for
    using ChannelIterator = QVector<T>*;
    using ConstChannelIterator = const QVector<T>*;

    // 非const遍历入口
    ChannelIterator begin() noexcept { return m_data; }
    ChannelIterator end() noexcept { return m_data + m_channelCount; }

    // const场景遍历入口
    ConstChannelIterator begin() const noexcept { return m_data; }
    ConstChannelIterator end() const noexcept { return m_data + m_channelCount; }
    ConstChannelIterator cbegin() const noexcept { return m_data; }
    ConstChannelIterator cend() const noexcept { return m_data; }

    QVector<T>& at(int channelIdx) {
        // 带边界检查，越界直接抛Qt风格断言，开发阶段快速定位非法通道号
        if (channelIdx < 0 || channelIdx >= m_channelCount) {
            qFatal("访问通道索引 %d 越界，合法范围0~31", channelIdx);
        }
        return m_data[channelIdx];
    }

    // const版本：const修饰场景下也能安全调用，符合Qt标准库的一致性设计
    const QVector<T>& at(int channelIdx) const {
        if (channelIdx < 0 || channelIdx >= m_channelCount) {
            qFatal("访问通道索引 %d 越界，合法范围0~31", channelIdx);
        }
        return m_data[channelIdx];
    }

    QVector<T>& operator[](int channelIdx) {
        assert(channelIdx >= 0 && channelIdx < m_channelCount);
        return m_data[channelIdx];
    }

    const QVector<T>& operator[](int channelIdx) const {
        assert(channelIdx >= 0 && channelIdx < m_channelCount);
        return m_data[channelIdx];
    }

    int currentPreallocSize() const { return m_perChannelCapacity; }

private:
    QVector<T>* m_data = nullptr;
    int m_channelCount = 0;
    int m_perChannelCapacity = 10000;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct ChannelProfileEntry {
        quint32 timeMs = 0; // 时间单位，ms
        quint32 energy;// 能量
    };

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void sigAppendMsg(const QString &msg, QtMsgType msgType);
    void showProfileChart(const int& detectorIndex, const QVector<QVector<ChannelProfileEntry>>& data);

public:
    virtual void closeEvent(QCloseEvent *event) override;

private slots:
    void slotAppendMsg(const QString &msg, QtMsgType msgType);

    void onSystemTimer();

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

    void onHardTriggeredSignalReceived();

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
    void enterUnattendedWaitingShot();
    void triggerUnattendedMeasureFromShot(const QString &shotNumber);
    void stopAutoMeasureSession();
    bool isMeasureSessionActive() const;
    int measureDurationMs() const;
    void startMeasureDurationTimer();
    void saveShotNumberFile(const QString &shotNumber) const;
    void appendUdpLog(const QString &line);

    //void onSelectSpectrumRange(const QCPRange& range);
    void onShowProfileChart(const int& detectorIndex, const QVector<QVector<ChannelProfileEntry>>& data);

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

    void on_action_connectMonitor_triggered();

    void on_action_disconnectMonitor_triggered();

    void on_action_exit_triggered();

    void on_action_about_triggered();

    void on_action_analyze_triggered();

    void on_action_signalWidth_triggered();

    void on_radioButton_spec_clicked();

    void on_radioButton_cps_clicked();

    void on_btn_exportProfile_clicked();

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

    struct SpectrumCountsEntry {
        int detectorIndex = 0;
        quint32 timeMs = 0; // 时间单位，ms
        quint32 count;
    };

    static constexpr int kChannelsPerDetector = 16;
    static constexpr int kSpectrumChannelCount = 32; // 探测器1: 1~16，探测器2: 17~32
    static constexpr int kDetector2ChannelOffset = 16; // 探测器2的逻辑通道号相对于物理通道的偏移

    static constexpr int kSpectrum512BinCount = 512;
    static constexpr int kSpectrum16BinCount = 17;
    static constexpr int kProfileChannelCount = 32;
    static constexpr int kVerticalCameraChannels = 16;
    static constexpr double kProfileZMin = -8.0;
    static constexpr double kProfileZMax = 8.0;
    static constexpr double kWaveformSampleIntervalNs = 16.0;
    static constexpr int kWaveformSampleCount = 1024;
    static constexpr int kWaveformMaxDisplayNs =
        static_cast<int>(kWaveformSampleCount * kWaveformSampleIntervalNs);

    void initStatusbar();
    int logicalChannelNumber(int detectorIndex, int channelNumber) const;
    void clearSpectrumData();
    void clearWaveformData();
    void resetMeasurementPlotData();
#ifdef QT_DATAVISUALIZATION_LIB
    void clear3DSurface(CustomSurface *surface);
#endif
    void appendSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                            const QVector<quint32> &counts);
    void appendWaveformData(int detectorIndex, int channelNumber, quint32 timeUnits,
                            const QVector<quint16> &samples);
    void ensureSpectrumBinAddresses(int binCount);
    void updateSpecIdSpinBoxRange();
    void updateWaveIdSpinBoxRange();
    void syncSpectrumSpinBoxToLatest();
    void syncWaveformSpinBoxToLatest();
    void finalizeMeasurementPlots();
    void updateSpectrumRefreshIntervalRange();
    void updateHxrDisplayBinControls();
    void updateProfileControls();
    void updateUnattendedControls();
    void updateMeasureParamsGroupEnabled();
    int hxrDisplayBinCount() const;
    bool loadEnergyCalibration(QVector<EnergyCalibration> &calibration, QString *errorMessage) const;
    QString energyCalibrationFilePath() const;
    void energyToBinRange512(double energyLeft, double energyRight, const EnergyCalibration &cal,
                          int &binStart, int &binEnd) const;
    void energyToBinRange16(double energyLeft, double energyRight, const EnergyCalibration &cal,
                             int &binStart, int &binEnd) const;
    quint64 sumCountsInBinRange(const QVector<quint32> &counts, int binStart, int binEnd) const;
    void generateProfileSnapshots();
    void refreshSpectrumPlot();
    void refreshWaveformPlot();
    void refreshSpectrumCountsPlot();
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

    CustomSurface* init3DSurface(const int& detectorIndex, QWidget* wigetContainer, const QString& title);//3D剖面图

    Ui::MainWindow *ui;
    CommandHelper *commandHelper = nullptr;//探测器网络
    UdpShotReceiver *m_udpShotReceiver = nullptr;
    QString m_currentShotNumber;
    bool m_udpListening = false;
    DetParameter mdetPara; //测量参数，包含触发模式、传输模式、测量时长等
    // 定时测量定时器
    QTimer* measureTimer = nullptr;
    //QElapsedTimer spectrumPlotThrottle;
    // 按逻辑通道(1~32)存储：探测器1为1~16，探测器2为17~32
    DynamicPreallocArray<SpectrumEntry> m_spectrumByChannel{600000};// 预分配60秒数据存储量
    // QVector<QVector<SpectrumEntry>> m_spectrumByChannel;
    QVector<quint32> m_spectrumBinAddresses; // 统一道址 1..N，绘图时复用
    QVector<QVector<quint32>> m_spectrumSequenceNumbersByChannel;
    QVector<QVector<quint32>> m_missingSpectrumNumbersByChannel;
    QVector<quint32> m_lastSpectrumSequenceByChannel;
    DynamicPreallocArray<SpectrumCountsEntry> m_spectrumCountsByChannel{60000};//记录计数率
    //QVector<QVector<SpectrumCountsEntry>> m_spectrumCountsByChannel;//记录计数率
    QVector<bool> m_hasSpectrumSequenceByChannel;
    // 按逻辑通道(1~32)存储波形历史
    DynamicPreallocArray<WaveformEntry> m_waveformByChannel{60000};// 预分配60秒数据存储量
    // QVector<QVector<WaveformEntry>> m_waveformByChannel;
    QVector<QVector<quint32>> m_waveformSequenceNumbersByChannel;
    QVector<QVector<quint32>> m_missingWaveformNumbersByChannel;
    QVector<quint32> m_lastWaveformSequenceByChannel;
    QVector<bool> m_hasWaveformSequenceByChannel;
    QTimer* m_plotRefreshTimer = nullptr;
    QVector<EnergyCalibration> m_energyCalibration;

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

    // 3D剖面图
    CustomSurface *m_hor3DSurface = nullptr;
    CustomSurface *m_ver3DSurface = nullptr;
    QVector<QVector<ChannelProfileEntry>> m_currentProfileData[2];

    enum class MeasureMode { ManualMode/*手动模式*/, AutoMode/*自动模式*/, AutoMatedMode/*无人值守模式*/ };
    MeasureMode m_measureMode;

public:
    // 无人值守
    bool m_enableAutoMated;
    QStateMachine *machine;
    QState *stIdle, *stStep1, *stStep2, *stStep3, *stStep4, *stFinish;
    void initStateMachine();

Q_SIGNALS:
    // 无人值守共分五步：
    void relayConnected();// 1、连接远程控制
    void relayPowerOpened();// 2、开启电源
    void detectorConnected();// 3、连接采集系统
    void monitorConnected();// 5、连接实时监测系统

private:
    struct LogItem {
        QString text;
        QColor color;
    };
    QVector<LogItem> m_logBuffer; // 日志缓存队列
    QTimer* m_logFlushTimer;      // 定时刷新定时器
    QMutex m_bufferMutex;         // 多线程防护（如果日志来自后台线程）
    void appendColoredText(const QString &text, const QColor &color);
};
#endif // MAINWINDOW_H
