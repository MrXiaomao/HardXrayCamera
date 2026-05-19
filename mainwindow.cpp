/*
 * @Author: MrPan
 * @Date: 2026-03-23 10:31:29
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-19 15:27:05
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
#include "globalsettings.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    measureTimer = new QTimer(this);
    connect(measureTimer, &QTimer::timeout, this, [=](){
        if (commandHelper) {
            commandHelper->stopMeasure();
            measureTimer->stop();
            qInfo() << "定时测量已停止";
        }
    });
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
    ui->plotSpec->setXAxisLabel("道址");
    ui->plotSpec->setYAxisLabel("计数");
    ui->plotSpec->setXRange(1,512);

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

    connect(commandHelper, &CommandHelper::sigRelayConnectError, this, [=](QAbstractSocket::SocketError){
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

    m_spectrumByChannel.resize(kSpectrumChannelCount);

    connect(commandHelper, &CommandHelper::sigSpectrumData, this,
            [=](int detectorIndex, int channelNumber, quint32 timeMs, 
                const QVector<quint32>& counts){
        const int logicalChannel = logicalChannelNumber(detectorIndex, channelNumber);
        appendSpectrumData(detectorIndex, channelNumber, timeMs, counts);

        const int currentChannel = ui->spb_channel->value();
        if (logicalChannel != currentChannel)
            return;

        const int specCount = m_spectrumByChannel.at(currentChannel - 1).size();
        ui->spb_specID->blockSignals(true);
        ui->spb_specID->setValue(specCount - 1);
        ui->spb_specID->blockSignals(false);
        updateSpecIdSpinBoxRange();

        if (spectrumPlotThrottle.isValid() && spectrumPlotThrottle.elapsed() < 500)
            return;
        spectrumPlotThrottle.restart();
        refreshSpectrumPlot();
    });

    connect(ui->spb_channel, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int) {
        updateSpecIdSpinBoxRange();
        refreshSpectrumPlot();
    });
    connect(ui->spb_specID, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int) {
        refreshSpectrumPlot();
    });
    ui->spb_specID->setMinimum(0);
    updateSpecIdSpinBoxRange();
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

int MainWindow::logicalChannelNumber(int detectorIndex, int channelNumber) const
{
    if (channelNumber < 1 || channelNumber > kChannelsPerDetector)
        return 0;
    if (detectorIndex == 2)
        return channelNumber + kDetector2ChannelOffset;
    if (detectorIndex == 1)
        return channelNumber;
    return 0;
}

void MainWindow::clearSpectrumData()
{
    for (auto &channelSpectra : m_spectrumByChannel)
        channelSpectra.clear();

    ui->spb_specID->blockSignals(true);
    ui->spb_specID->setValue(0);
    ui->spb_specID->blockSignals(false);
    updateSpecIdSpinBoxRange();
    ui->plotSpec->clearData();
    ui->plotSpec->refreshPlot();
}

void MainWindow::appendSpectrumData(int detectorIndex, int channelNumber, quint32 timeMs,
                                    const QVector<quint32> &counts)
{
    const int storageChannel = logicalChannelNumber(detectorIndex, channelNumber);
    if (storageChannel < 1 || storageChannel > m_spectrumByChannel.size())
        return;

    SpectrumEntry entry;
    entry.detectorIndex = detectorIndex;
    entry.timeMs = timeMs;
    entry.counts = counts;
    m_spectrumByChannel[storageChannel - 1].append(entry);
}

void MainWindow::ensureSpectrumBinAddresses(int binCount)
{
    if (m_spectrumBinAddresses.size() == binCount)
        return;

    m_spectrumBinAddresses.resize(binCount);
    for (int i = 0; i < binCount; ++i)
        m_spectrumBinAddresses[i] = i + 1;
}

void MainWindow::updateSpecIdSpinBoxRange()
{
    const int channel = ui->spb_channel->value();
    const int specCount = (channel >= 1 && channel <= m_spectrumByChannel.size())
                              ? m_spectrumByChannel.at(channel - 1).size()
                              : 0;
    const int maxSpecId = qMax(0, specCount - 1);

    ui->spb_specID->blockSignals(true);
    ui->spb_specID->setMaximum(maxSpecId);
    if (ui->spb_specID->value() > maxSpecId)
        ui->spb_specID->setValue(maxSpecId);
    ui->spb_specID->blockSignals(false);
}

void MainWindow::refreshSpectrumPlot()
{
    const int channel = ui->spb_channel->value();
    const int specId = ui->spb_specID->value();
    if (channel < 1 || channel > m_spectrumByChannel.size()) {
        ui->plotSpec->clearData();
        ui->plotSpec->refreshPlot();
        return;
    }

    const QVector<SpectrumEntry> &spectra = m_spectrumByChannel.at(channel - 1);
    if (specId < 0 || specId >= spectra.size()) {
        ui->plotSpec->clearData();
        ui->plotSpec->setTitle(QString("能谱 CH%1 #%2 (无数据)").arg(channel).arg(specId));
        ui->plotSpec->refreshPlot();
        return;
    }

    const SpectrumEntry &entry = spectra.at(specId);
    ui->plotSpec->setTitle(QString("能谱 Det%1 CH%2 #%3 t=%4ms")
                               .arg(entry.detectorIndex)
                               .arg(channel)
                               .arg(specId)
                               .arg(entry.timeMs)
                               );
    ensureSpectrumBinAddresses(entry.counts.size());
    ui->plotSpec->setData(m_spectrumBinAddresses, entry.counts);
    ui->plotSpec->refreshPlot();
}

// 开始测量
void MainWindow::on_btn_startMeasure_clicked()
{
    clearSpectrumData();
    spectrumPlotThrottle.invalidate();

    DetParameter detPara = {};
    //触发模式：外触发、软件触发
    detPara.trigMode = Order::TriggerMode::SoftwareTrigger; //先固定位软件触发

    //能谱模式
    int mode = ui->cmb_transferMode->currentIndex();
    if(mode == 0){
        detPara.transferMode = Order::TransferMode::Spectrum512;
    } else if(mode == 1){
        detPara.transferMode = Order::TransferMode::Waveform;
    } else if(mode == 2){
        detPara.transferMode = Order::TransferMode::Spectrum16;
    }
    
    //测量时长，ms
    detPara.measureTime = ui->spb_measureTime->value();

    JsonSettings* settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    //能谱相关参数，和 DetectorSetting::loadSettings() 使用同一组配置项
    detPara.spectrumRefreshInterval = settings->getValueByPath("FPGA/spec_refash_time").toInt();
    detPara.spectrumTriggerThreshold = settings->getValueByPath("FPGA/threshold").toInt();
    detPara.spectrumDeadTime = settings->getValueByPath("FPGA/deadTime").toInt();

    //波形触发阈值，CH1~CH32
    for (int channel = 1; channel <= 32; ++channel) {
        detPara.waveformTriggerThreshold[channel - 1] =
            settings->getValueByPath(QString("FPGA/wave/threshold%1").arg(channel), 50).toInt();
    }

    // 文件名产生：存储路径+炮号+测量时间
    QString savePath = ui->le_savePath->text();
    if (savePath.isEmpty()){
        qWarning() << "请先选择数据保存路径";
        return;
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

    // 设置存储文件格式
    commandHelper->setSaveFileFormat(ui->cmb_saveFormat->currentIndex() == 0 ? Binary : Text);
    commandHelper->setSavePath(savePath);
    commandHelper->startMeasure(detPara);
    measureTimer->start(detPara.measureTime);
    qInfo() << "定时测量已开始，测量时长:" << detPara.measureTime << "ms";
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

