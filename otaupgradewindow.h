#ifndef OTAUPGRADEWINDOW_H
#define OTAUPGRADEWINDOW_H

#include <QWidget>
#include <QtConcurrent>
#include "commandhelper.h"

namespace Ui {
class OTAUpgradeWindow;
}

class OTAUpgradeWindow : public QWidget
{
    Q_OBJECT

public:
    explicit OTAUpgradeWindow(QWidget *parent = nullptr);
    ~OTAUpgradeWindow();

    Q_SIGNAL void sigWriteLog(const QString &msg, QtMsgType msgType = QtDebugMsg);
    Q_SLOT void slotWriteLog(const QString &msg, QtMsgType msgType = QtDebugMsg);

    // 异步发送进度更新槽函数（主线程执行）
    Q_SIGNAL void sigSendProgress(int index, int current, int total);
    Q_SLOT void slotSendProgress(int index, int current, int total);

    // 异步发送完成槽函数（主线程执行）
    Q_SIGNAL void sigSendFinished(int index, bool success);
    Q_SLOT void slotSendFinished(int index, bool success);

    Q_SIGNAL void sigUpgradeFinished();
    Q_SLOT void slotUpgradeFinished();

private slots:
    void on_pushButton_ok_clicked();

    void on_pushButton_exit_clicked();

private:
    Ui::OTAUpgradeWindow *ui;
    CommandHelper *commHelper = nullptr;

private:
    int m_runningTasks = 0; // 异步任务计数器（用于控制按钮状态）
    QList<QFutureWatcher<void>*> m_watchers; // 任务监控器列表（支持取消操作）

private:
    QList<int> m_taskQueue; // 任务队列：设备索引
    QString m_binFileName; // 当前正在发送的bin文件路径
    int m_currentTaskIndex = 0; // 当前执行的任务索引
    int m_currentUpgradeIndex = 0; // 当前等待回包的探测器索引
    bool m_eraseSucceeded = false;
    QEventLoop m_waitLoop; // 等待事件循环，用于等待擦除完成

    // 异步发送OTA数据的核心函数（子线程执行）
    void asyncSendOTAData(int index);
    void startNextTask();

private slots:


};

#endif // OTAUPGRADEWINDOW_H
