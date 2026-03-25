#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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

private:
    Ui::MainWindow *ui;
    CommandHelper *commandHelper = nullptr;//探测器网络
};
#endif // MAINWINDOW_H
