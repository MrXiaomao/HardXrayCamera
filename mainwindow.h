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
    void slotAppendMsg(const QString &msg, QtMsgType msgType);

    void on_btn_relayNetOpen_clicked();

    void on_btn_relayNetClose_clicked();

    void on_btn_startMeasure_clicked();

    void on_bt_powerOn_clicked();

    void on_bt_powerOff_clicked();

    void on_bt_connectDet_clicked();

    void on_bt_disconnectDet_clicked();

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

    Ui::MainWindow *ui;
    CommandHelper *commandHelper = nullptr;//探测器网络
    // 定时测量定时器
    QTimer* measureTimer = nullptr;
    QElapsedTimer spectrumPlotThrottle;
    // 按逻辑通道(1~32)存储：探测器1为1~16，探测器2为17~32
    QVector<QVector<SpectrumEntry>> m_spectrumByChannel;
    QVector<quint32> m_spectrumBinAddresses; // 统一道址 1..N，绘图时复用
};
#endif // MAINWINDOW_H
