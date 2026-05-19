/*
 * @Author: MrPan
 * @Date: 2026-03-23 10:31:29
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-18 20:21:16
 * @Description: 请填写简介
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
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

private:
    Ui::MainWindow *ui;
    CommandHelper *commandHelper = nullptr;//探测器网络
    // 定时测量定时器
    QTimer* measureTimer = nullptr;
};
#endif // MAINWINDOW_H
