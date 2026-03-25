#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
