/*
 * @Author: MrPan
 * @Date: 2026-03-23 10:31:29
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-07 14:51:00
 * @Description: 请填写简介
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QToolButton>
#include <QAction>
#include <QTextCharFormat>
#include <QTextCursor>

#include "detectorsetting.h"
#include "commandhelper.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QAction *action = ui->le_savePath->addAction(QIcon(":/resource/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button = qobject_cast<QToolButton*>(action->associatedWidgets().last());
    button->setCursor(QCursor(Qt::PointingHandCursor));
    connect(button, &QToolButton::pressed, this, [=](){
        QString cacheDir = QFileDialog::getExistingDirectory(this);
        if (!cacheDir.isEmpty()){
            ui->le_savePath->setText(cacheDir);
        }
    });
    
    // 获取当前时间
    ui->plainTextEdit_log->clear();
    // 使用 QPlainTextEdit 内建限行能力，自动丢弃最早日志
    ui->plainTextEdit_log->document()->setMaximumBlockCount(2000);
    
    connect(this, SIGNAL(sigAppendMsg(const QString &, QtMsgType)), this, SLOT(slotAppendMsg(const QString &, QtMsgType)));
    qRegisterMetaType<QtMsgType>("QtMsgType");
    
    ui->plotWave->setTitle("波形");
    ui->plotWave->setXAxisLabel("时间(us)");
    ui->plotWave->setYAxisLabel("幅值");
    ui->plotWave->setXRange(0,10000);

    ui->plotSpec->setTitle("分时能谱");
    ui->plotSpec->setXAxisLabel("能量/keV");
    ui->plotSpec->setYAxisLabel("计数");
    ui->plotSpec->setXRange(0,100);

    ui->plotProfile->setTitle("剖面图");
    ui->plotProfile->setXAxisLabel("z轴位置");
    ui->plotProfile->setYAxisLabel("计数");
    ui->plotProfile->setXRange(-25,25);

    // ui->plotVoltage->setTitle("电压曲线");
    ui->plotVoltage->setXAxisLabel("时间");
    ui->plotVoltage->setYAxisLabel("电压 (V)");
    ui->plotVoltage->setTimeWindow(300);

    // ui->plotCurrent->setTitle("电流曲线");
    ui->plotCurrent->setXAxisLabel("时间");
    ui->plotCurrent->setYAxisLabel("电流 (A)");
    ui->plotCurrent->setTimeWindow(300);

    // ui->plotTemp->setTitle("温度曲线");
    ui->plotTemp->setXAxisLabel("时间");
    ui->plotTemp->setYAxisLabel("温度 (℃)");
    ui->plotTemp->setTimeWindow(300);

    commandHelper = new CommandHelper();
    connect(commandHelper, &CommandHelper::sigAppendMsg, this, &MainWindow::slotAppendMsg);
    connect(commandHelper, &CommandHelper::sigRelayStatus, this, [=](bool on){
        if (on){
            qInfo() << "继电器网络状态: 已连接";
        } else {
            qInfo() << "继电器网络状态: 已断开";
        }
    });

    connect(commandHelper, &CommandHelper::sigRelayConnectError, this, [=](QAbstractSocket::SocketError error){
        qWarning() << "继电器网络连接失败";
    });

    connect(commandHelper, &CommandHelper::sigRelayPowerStatus, this, [=](bool on){
        if (on){
            qInfo() << "继电器控制的电源状态: 已开启";
        } else {
            qInfo() << "继电器控制的电源状态: 已关闭";
        }
    });

    connect(commandHelper, &CommandHelper::sigDetector1Status, this, [=](bool on){
        if (on){
            qInfo() << "FPGA板1状态: 已连接";
        } else {
            qInfo() << "FPGA板1状态: 已断开";
        }
    });

    connect(commandHelper, &CommandHelper::sigDetector2Status, this, [=](bool on){
        if (on){
            qInfo() << "FPGA板2状态: 已连接";
        } else {
            qInfo() << "FPGA板2状态: 已断开";
        }
    });

    connect(commandHelper, &CommandHelper::sigDetector1Fault, this, [=](){
        qWarning() << "FPGA板1连接失败";
    });

    connect(commandHelper, &CommandHelper::sigDetector2Fault, this, [=](){
        qWarning() << "FPGA板2连接失败";
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_action_setting_triggered()
{   
    DetectorSetting* settDialog = new DetectorSetting();
    settDialog->setAttribute(Qt::WA_DeleteOnClose);
    settDialog->exec();
}

void MainWindow::slotAppendMsg(const QString &msg, QtMsgType msgType)
{
    QTextCharFormat format;
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz>>");
    QString logLine;

    if (msgType == QtWarningMsg) {
        format.setForeground(Qt::blue);
        logLine = QStringLiteral("%1 [WARN] %2").arg(ts).arg(msg);
    } else if (msgType == QtCriticalMsg || msgType == QtFatalMsg) {
        format.setForeground(Qt::red);
        logLine = QStringLiteral("%1 [ERROR] %2").arg(ts).arg(msg);
    } else {
        // QtDebugMsg、QtInfoMsg、QtSystemMsg 等：不打印级别字样
        logLine = QStringLiteral("%1 %2").arg(ts).arg(msg);
    }

    QTextCursor cursor = ui->plainTextEdit_log->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(logLine, format);
    cursor.insertBlock();
    ui->plainTextEdit_log->setTextCursor(cursor);
}


void MainWindow::on_btn_relayNetOpen_clicked()
{
    commandHelper->connectRelay();
}


void MainWindow::on_btn_relayNetClose_clicked()
{
    commandHelper->disconnectRelay();
}


void MainWindow::on_btn_startMeasure_clicked()
{
    commandHelper->testSend();
}

//打开探测器供电电源，通过继电器进行控制探测器的电源
void MainWindow::on_bt_powerOn_clicked()
{
    commandHelper->PowerOnRelay();
}

//关闭探测器供电电源，通过继电器进行控制探测器的电源
void MainWindow::on_bt_powerOff_clicked()
{
    commandHelper->PowerOffRelay();
}

