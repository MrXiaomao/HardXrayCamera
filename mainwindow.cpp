/*
 * @Author: MrPan
 * @Date: 2026-03-23 10:31:29
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-28 11:37:33
 * @Description: 请填写简介
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QToolButton>
#include <QAction>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QDir>
#include <QMessageBox>
#include <algorithm>

#include "detectorsetting.h"
#include "commandhelper.h"
#include "globalsettings.h"
#include "udpshotreceiver.h"
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    measureTimer = new QTimer(this);
    connect(measureTimer, &QTimer::timeout, this, &MainWindow::onMeasureTimerTimeout);
    ui->setupUi(this);

    QAction *action = ui->le_savePath->addAction(QIcon(":/resource/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button = qobject_cast<QToolButton*>(action->associatedWidgets().last());
    button->setCursor(QCursor(Qt::PointingHandCursor));
    // 保存路径选择：仅初始化时绑定一次，逻辑简单保留 lambda
    connect(button, &QToolButton::pressed, this, [=]() {
        const QString cacheDir = QFileDialog::getExistingDirectory(this);
        if (!cacheDir.isEmpty())
            ui->le_savePath->setText(cacheDir);
    });
    
    // 获取当前时间
    ui->plainTextEdit_log->clear();
    // 使用 QPlainTextEdit 内建限行能力，自动丢弃最早日志
    ui->plainTextEdit_log->document()->setMaximumBlockCount(2000);
    
    connect(this, SIGNAL(sigAppendMsg(const QString &, QtMsgType)), this, SLOT(slotAppendMsg(const QString &, QtMsgType)));
    qRegisterMetaType<QtMsgType>("QtMsgType");
    
    ui->plotWave->setTitle("波形");
    ui->plotWave->setXAxisLabel("时间(ns)");
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

    {
        //信号采集机箱
        // ui->plotVoltage->setTitle("电压曲线");
        ui->plotVoltage->setXAxisLabel("时间");
        ui->plotVoltage->setYAxisLabel("电压 (V)");
        ui->plotVoltage->setTimeWindow(180);

        // ui->plotCurrent->setTitle("电流曲线");
        ui->plotCurrent->setXAxisLabel("时间");
        ui->plotCurrent->setYAxisLabel("电流 (A)");
        ui->plotCurrent->setTimeWindow(180);

        // ui->plotTemp->setTitle("温度曲线");// 共3条
        ui->plotTemp->addGraph();
        ui->plotTemp->addGraph();
        ui->plotTemp->setXAxisLabel("时间");
        ui->plotTemp->setYAxisLabel("温度 (℃)");
        ui->plotTemp->setTimeWindow(180);
    }
    {
        //电源机箱
        // ui->plotVoltage->setTitle("电压曲线");
        ui->plotVoltage_2->addGraph(3);
        ui->plotVoltage_2->setXAxisLabel("时间");
        ui->plotVoltage_2->setYAxisLabel("电压 (V)");
        ui->plotVoltage_2->setTimeWindow(180);

        // ui->plotCurrent->setTitle("电流曲线");
        ui->plotCurrent_2->addGraph(3);
        ui->plotCurrent_2->setXAxisLabel("时间");
        ui->plotCurrent_2->setYAxisLabel("电流 (A)");
        ui->plotCurrent_2->setTimeWindow(180);

        // ui->plotTemp->setTitle("温度曲线");// 共6条
        ui->plotTemp_2->addGraph(5);
        ui->plotTemp_2->setXAxisLabel("时间");
        ui->plotTemp_2->setYAxisLabel("温度 (℃)");
        ui->plotTemp_2->setTimeWindow(180);
    }
    commandHelper = CommandHelper::instance();
    connect(commandHelper, &CommandHelper::sigAppendMsg, this, &MainWindow::slotAppendMsg);
    connect(commandHelper, &CommandHelper::sigRelayStatus, this, &MainWindow::onRelayStatusChanged);
    connect(commandHelper, &CommandHelper::sigRelayConnectError, this, [=](QAbstractSocket::SocketError) {
        qWarning() << "继电器网络连接失败";
        ui->btn_relayNetOpen->blockSignals(true);
        ui->btn_relayNetOpen->setChecked(false);
        ui->btn_relayNetOpen->setText(QStringLiteral("网络连接"));
        ui->btn_relayNetOpen->blockSignals(false);
    });
    connect(commandHelper, &CommandHelper::sigRelayPowerStatus, this, &MainWindow::onRelayPowerStatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector1Status, this, &MainWindow::onDetector1StatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector2Status, this, &MainWindow::onDetector2StatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector3Status, this, &MainWindow::onDetector3StatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector4Status, this, &MainWindow::onDetector4StatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector1Fault, this, &MainWindow::onDetector1ConnectFault);
    connect(commandHelper, &CommandHelper::sigDetector2Fault, this, &MainWindow::onDetector2ConnectFault);
    connect(commandHelper, &CommandHelper::sigDetector3Fault, this, &MainWindow::onDetector3ConnectFault);
    connect(commandHelper, &CommandHelper::sigDetector4Fault, this, &MainWindow::onDetector4ConnectFault);
    connect(commandHelper, &CommandHelper::sigARM1Status, this, &MainWindow::onArm1StatusChanged);
    connect(commandHelper, &CommandHelper::sigARM2Status, this, &MainWindow::onArm2StatusChanged);
    connect(commandHelper, &CommandHelper::sigArm1SensorData, this, &MainWindow::onArm1SensorData, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigArm2SensorData, this, &MainWindow::onArm2SensorData, Qt::QueuedConnection);
    // 能谱/波形数据由工作线程发出，排队到主线程处理
    connect(commandHelper, &CommandHelper::sigSpectrumData, this,
        &MainWindow::onSpectrumDataReceived, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigWaveformData, this,
        &MainWindow::onWaveformDataReceived, Qt::QueuedConnection);

    m_spectrumByChannel.resize(kSpectrumChannelCount);
    m_spectrumSequenceNumbersByChannel.resize(kSpectrumChannelCount);
    m_missingSpectrumNumbersByChannel.resize(kSpectrumChannelCount);
    m_lastSpectrumSequenceByChannel.resize(kSpectrumChannelCount);
    m_hasSpectrumSequenceByChannel.resize(kSpectrumChannelCount);
    m_waveformByChannel.resize(kSpectrumChannelCount);
    m_waveformCountByChannel.resize(kSpectrumChannelCount);
    m_waveformSequenceNumbersByChannel.resize(kSpectrumChannelCount);
    m_missingWaveformNumbersByChannel.resize(kSpectrumChannelCount);
    m_lastWaveformSequenceByChannel.resize(kSpectrumChannelCount);
    m_hasWaveformSequenceByChannel.resize(kSpectrumChannelCount);

    waveformPlotTimer = new QTimer(this);
    waveformPlotTimer->setInterval(200); // 每200ms刷新一次波形
    connect(waveformPlotTimer, &QTimer::timeout, this, &MainWindow::refreshWaveformPlot);
    // waveformPlotTimer 仅在波形测量模式下启动

    connect(ui->spb_channel, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onChannelSpinBoxChanged);
    connect(ui->spb_specID, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshSpectrumPlot);
    ui->spb_specID->setMinimum(0);
    updateSpecIdSpinBoxRange();

    connect(ui->cmb_transferMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateSpectrumRefreshIntervalRange);
    updateSpectrumRefreshIntervalRange();

    m_currentShotNumber = ui->lineEdit_shotID->text().trimmed();

    m_udpShotReceiver = new UdpShotReceiver(this);
    connect(m_udpShotReceiver, &UdpShotReceiver::datagramReceived,
            this, &MainWindow::onUdpDatagramReceived);
    connect(m_udpShotReceiver, &UdpShotReceiver::shotNumberChanged,
            this, &MainWindow::onUdpShotNumberChanged);
    connect(m_udpShotReceiver, &UdpShotReceiver::bindStateChanged,
            this, &MainWindow::onUdpBindStateChanged);

    ui->btn_relayNetOpen->setCheckable(true);
    ui->btn_relayNetOpen->setChecked(false);
    ui->btn_relayNetOpen->setText(QStringLiteral("网络连接"));
    ui->btn_relayNetClose->hide();
    ui->bt_powerOn->setEnabled(false);
    ui->bt_powerOff->setEnabled(false);
    ui->bt_connectDet->setEnabled(false);
    ui->bt_disconnectDet->setEnabled(false);
    ui->btn_startMeasure->setEnabled(false);
    ui->btn_stopMeasure->setEnabled(false);

    // 开机自动最大化：一次性延迟调用，保留 lambda
    QTimer::singleShot(0, this, [this] { showMaximized(); });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onMeasureTimerTimeout()
{
    if (!commandHelper)
        return;

    waveformPlotTimer->stop();
    commandHelper->stopMeasure();
    printWaveformCollectionSummary();
    printSpectrumSequenceSummary();
    measureTimer->stop();
    qInfo() << "定时测量已停止";
}

void MainWindow::onRelayStatusChanged(bool on)
{
    ui->btn_relayNetOpen->blockSignals(true);
    ui->btn_relayNetOpen->setChecked(on);
    ui->btn_relayNetOpen->setText(on ? QStringLiteral("网络断开") : QStringLiteral("网络连接"));
    ui->btn_relayNetOpen->blockSignals(false);

    if (on){
        ui->bt_powerOn->setEnabled(true);
        ui->bt_powerOff->setEnabled(true);

        qInfo() << "继电器网络状态: 已连接";
    }
    else{
        ui->bt_powerOn->setEnabled(false);
        ui->bt_powerOff->setEnabled(false);
        ui->bt_connectDet->setEnabled(false);
        ui->bt_disconnectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "继电器网络状态: 已断开";
    }
}

void MainWindow::onRelayPowerStatusChanged(bool on)
{
    if (on) {
        ui->bt_powerOn->setEnabled(false);
        ui->bt_powerOff->setEnabled(true);
        ui->bt_connectDet->setEnabled(true);
        ui->bt_disconnectDet->setEnabled(false);

        qInfo() << "继电器控制的电源状态: 已开启";
        replayPowerOn = true;
    } else {
        ui->bt_powerOn->setEnabled(true);
        ui->bt_powerOff->setEnabled(false);
        ui->bt_connectDet->setEnabled(false);
        ui->bt_disconnectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);

        qInfo() << "继电器控制的电源状态: 已关闭";
        replayPowerOn = false;
        stopUdpListening();
        // 断电后清除各设备在线标记
        detectOnline[0] = false;
        detectOnline[1] = false;
        detectOnline[2] = false;
        detectOnline[3] = false;
        armSensorOnline[0] = false;
        armSensorOnline[1] = false;
    }
}

void MainWindow::onDetector1StatusChanged(bool on)
{
    if (on) {
        ui->bt_connectDet->setEnabled(false);
        ui->bt_disconnectDet->setEnabled(true);
        ui->btn_startMeasure->setEnabled(true);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板1状态: 已连接";
        detectOnline[0] = true;
    } else {
        ui->bt_connectDet->setEnabled(true);
        ui->bt_disconnectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板1状态: 已断开";
        detectOnline[0] = false;
    }
}

void MainWindow::onDetector2StatusChanged(bool on)
{
    if (on) {
        ui->bt_connectDet->setEnabled(false);
        ui->bt_disconnectDet->setEnabled(true);
        ui->btn_startMeasure->setEnabled(true);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板2状态: 已连接";
        detectOnline[1] = true;
    } else {
        ui->bt_connectDet->setEnabled(true);
        ui->bt_disconnectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板2状态: 已断开";
        detectOnline[1] = false;
    }
}

void MainWindow::onDetector3StatusChanged(bool on)
{
    if (on) {
        ui->bt_connectDet->setEnabled(false);
        ui->bt_disconnectDet->setEnabled(true);
        ui->btn_startMeasure->setEnabled(true);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板3状态: 已连接";
        detectOnline[2] = true;
    } else {
        ui->bt_connectDet->setEnabled(true);
        ui->bt_disconnectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板3状态: 已断开";
        detectOnline[2] = false;
    }
}

void MainWindow::onDetector4StatusChanged(bool on)
{
    if (on) {
        ui->bt_connectDet->setEnabled(false);
        ui->bt_disconnectDet->setEnabled(true);
        ui->btn_startMeasure->setEnabled(true);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板4状态: 已连接";
        detectOnline[3] = true;
    } else {
        ui->bt_connectDet->setEnabled(true);
        ui->bt_disconnectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "FPGA板4状态: 已断开";
        detectOnline[3] = false;
    }
}

void MainWindow::onDetector1ConnectFault()
{
    qWarning() << "FPGA板1连接失败";
    detectOnline[0] = false;
}

void MainWindow::onDetector2ConnectFault()
{
    qWarning() << "FPGA板2连接失败";
    detectOnline[1] = false;
}

void MainWindow::onDetector3ConnectFault()
{
    qWarning() << "FPGA板3连接失败";
    detectOnline[2] = false;
}

void MainWindow::onDetector4ConnectFault()
{
    qWarning() << "FPGA板4连接失败";
    detectOnline[3] = false;
}

void MainWindow::onArm1StatusChanged(bool on)
{
    if (on) {
        qInfo() << "ARM传感器设备1状态：已连接";
        armSensorOnline[0] = true;
    } else {
        qInfo() << "ARM传感器设备1状态：已断开";
        armSensorOnline[0] = false;
    }
}

void MainWindow::onArm2StatusChanged(bool on)
{
    if (on) {
        qInfo() << "ARM传感器设备2状态：已连接";
        armSensorOnline[1] = true;
    } else {
        qInfo() << "ARM传感器设备2状态：已断开";
        armSensorOnline[1] = false;
    }
}

void MainWindow::onArm1SensorData(const QVector<double>&/*温度*/ temperature, const QVector<double>&/*电压*/ voltage, const QVector<double>&/*电流*/ current)
{
    ui->plotTemp->appendPoints(0, temperature);
    ui->plotVoltage->appendPoints(0, voltage);
    ui->plotCurrent->appendPoints(0, current);
    ui->plotTemp->refreshPlot();
    ui->plotVoltage->refreshPlot();
    ui->plotCurrent->refreshPlot();

    // 超出阈值且探测器在线时，通过继电器切断电源
    bool isAlarm = false;
    for (int i=0; i<temperature.size(); ++i){
        if (temperature[i] > ui->doubleSpinBox_temp->value())
        {
            isAlarm = true;
            break;
        }
    }

    if (!isAlarm){
        for (int i=0; i<voltage.size(); ++i){
            if (voltage[i] > ui->doubleSpinBox_voltage->value())
            {
                isAlarm = true;
                break;
            }
        }
    }

    if (!isAlarm){
        for (int i=0; i<voltage.size(); ++i){
            if (voltage[i] > ui->doubleSpinBox_current->value())
            {
                isAlarm = true;
                break;
            }
        }
    }

    if (isAlarm) {
        // 判断是否无人值守模式
        if (isTaskRunning)
        {
            commandHelper->PowerOffRelay();
        }
    }
}

void MainWindow::onArm2SensorData(const QVector<double>&/*温度*/ temperature, const QVector<double>&/*电压*/ voltage, const QVector<double>&/*电流*/ current)
{
    ui->plotTemp_2->appendPoints(0, temperature);
    ui->plotVoltage_2->appendPoints(0, voltage);
    ui->plotCurrent_2->appendPoints(0, current);
    ui->plotTemp_2->refreshPlot();
    ui->plotVoltage_2->refreshPlot();
    ui->plotCurrent_2->refreshPlot();

    // 超出阈值且探测器在线时，通过继电器切断电源
    bool isAlarm = false;
    for (int i=0; i<temperature.size(); ++i){
        if (temperature[i] > ui->doubleSpinBox_temp->value())
        {
            isAlarm = true;
            break;
        }
    }

    if (!isAlarm){
        for (int i=0; i<voltage.size(); ++i){
            if (voltage[i] > ui->doubleSpinBox_voltage->value())
            {
                isAlarm = true;
                break;
            }
        }
    }

    if (!isAlarm){
        for (int i=0; i<voltage.size(); ++i){
            if (voltage[i] > ui->doubleSpinBox_current->value())
            {
                isAlarm = true;
                break;
            }
        }
    }

    if (isAlarm) {
        // 判断是否无人值守模式
        if (isTaskRunning)
        {
            commandHelper->PowerOffRelay();
        }
    }
}

void MainWindow::onSpectrumDataReceived(int detectorIndex, int channelNumber, quint32 timeMs,
                                        const QVector<quint32> &counts)
{
    const int logicalChannel = logicalChannelNumber(detectorIndex, channelNumber);
    if (logicalChannel < 1 || logicalChannel > m_spectrumByChannel.size())
        return;

    const int channelIndex = logicalChannel - 1;
    appendSpectrumData(detectorIndex, channelNumber, timeMs, counts);

    // 以能谱刷新间隔为单位的序列号，用于检测丢帧
    const quint32 spectrumSequence = timeMs / mdetPara.spectrumRefreshInterval;
    m_spectrumSequenceNumbersByChannel[channelIndex].append(spectrumSequence);
    if (!m_hasSpectrumSequenceByChannel[channelIndex]) {
        m_hasSpectrumSequenceByChannel[channelIndex] = true;
        m_lastSpectrumSequenceByChannel[channelIndex] = spectrumSequence;
    } else if (spectrumSequence > m_lastSpectrumSequenceByChannel[channelIndex]) {
        for (quint32 missingSequence = m_lastSpectrumSequenceByChannel[channelIndex] + 1;
             missingSequence < spectrumSequence;
             ++missingSequence) {
            m_missingSpectrumNumbersByChannel[channelIndex].append(missingSequence);
        }
        m_lastSpectrumSequenceByChannel[channelIndex] = spectrumSequence;
    }

    const int currentChannel = ui->spb_channel->value();
    if (logicalChannel != currentChannel)
        return;

    const int specCount = m_spectrumByChannel.at(currentChannel - 1).size();
    ui->spb_specID->blockSignals(true);
    ui->spb_specID->setValue(specCount - 1);
    ui->spb_specID->blockSignals(false);
    updateSpecIdSpinBoxRange();

    // 限流刷新，避免高频能谱拖慢 UI
    if (spectrumPlotThrottle.isValid() && spectrumPlotThrottle.elapsed() < 500)
        return;

    spectrumPlotThrottle.restart();
    refreshSpectrumPlot();
}

void MainWindow::onWaveformDataReceived(int detectorIndex, int channelNumber, quint32 timeUnits,
                                        const QVector<quint16> &samples)
{
    const int logicalChannel = logicalChannelNumber(detectorIndex, channelNumber);
    if (logicalChannel < 1 || logicalChannel > m_waveformByChannel.size())
        return;

    const int channelIndex = logicalChannel - 1;
    m_waveformByChannel[channelIndex] = samples; // 覆盖最新一帧
    ++m_waveformCountByChannel[channelIndex];

    const quint32 waveformSequence = timeUnits;
    m_waveformSequenceNumbersByChannel[channelIndex].append(waveformSequence);
    if (!m_hasWaveformSequenceByChannel[channelIndex]) {
        m_hasWaveformSequenceByChannel[channelIndex] = true;
        m_lastWaveformSequenceByChannel[channelIndex] = waveformSequence;
        return;
    }

    if (waveformSequence > m_lastWaveformSequenceByChannel[channelIndex]) {
        for (quint32 missingSequence = m_lastWaveformSequenceByChannel[channelIndex] + 1;
             missingSequence < waveformSequence;
             ++missingSequence) {
            m_missingWaveformNumbersByChannel[channelIndex].append(missingSequence);
        }
    }
    m_lastWaveformSequenceByChannel[channelIndex] = waveformSequence;
}

void MainWindow::onChannelSpinBoxChanged(int /*value*/)
{
    updateSpecIdSpinBoxRange();
    refreshSpectrumPlot();
    refreshWaveformPlot();
}

void MainWindow::on_action_setting_triggered()
{
    DetectorSetting *settDialog = new DetectorSetting();
    settDialog->setAttribute(Qt::WA_DeleteOnClose);
    settDialog->setUdpPortEditable(!m_udpListening);
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
    if (ui->btn_relayNetOpen->isChecked())
        commandHelper->connectRelay();
    else
        commandHelper->disconnectRelay();
}


void MainWindow::on_btn_relayNetClose_clicked()
{
    // 功能已合并至 btn_relayNetOpen 切换按钮，保留此槽避免 UI 变更时链接报错
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

void MainWindow::resetWaveformCounters()
{
    std::fill(m_waveformCountByChannel.begin(), m_waveformCountByChannel.end(), 0);
    std::fill(m_lastWaveformSequenceByChannel.begin(), m_lastWaveformSequenceByChannel.end(), 0);
    std::fill(m_hasWaveformSequenceByChannel.begin(), m_hasWaveformSequenceByChannel.end(), false);
    for (auto &samples : m_waveformByChannel)
        samples.clear();
    for (auto &sequenceNumbers : m_waveformSequenceNumbersByChannel)
        sequenceNumbers.clear();
    for (auto &missingSequenceNumbers : m_missingWaveformNumbersByChannel)
        missingSequenceNumbers.clear();
}

void MainWindow::resetSpectrumSequenceTracking()
{
    std::fill(m_lastSpectrumSequenceByChannel.begin(), m_lastSpectrumSequenceByChannel.end(), 0);
    std::fill(m_hasSpectrumSequenceByChannel.begin(), m_hasSpectrumSequenceByChannel.end(), false);
    for (auto &sequenceNumbers : m_spectrumSequenceNumbersByChannel)
        sequenceNumbers.clear();
    for (auto &missingSequenceNumbers : m_missingSpectrumNumbersByChannel)
        missingSequenceNumbers.clear();
}

void MainWindow::printWaveformCollectionSummary() const
{
    for (int channel = 1; channel <= m_missingWaveformNumbersByChannel.size(); ++channel) {
        const QVector<quint32> &missingSequences = m_missingWaveformNumbersByChannel.at(channel - 1);
        if (missingSequences.isEmpty())
            continue;

        QStringList missingLabels;
        missingLabels.reserve(missingSequences.size());
        for (quint32 missingSequence : missingSequences)
            missingLabels << QString::number(missingSequence);

        qInfo() << QString("通道%1缺失波形序号: %2")
            .arg(channel)
            .arg(missingLabels.join(", "));
    }
}

void MainWindow::printSpectrumSequenceSummary() const
{
    for (int channel = 1; channel <= m_missingSpectrumNumbersByChannel.size(); ++channel) {
        const QVector<quint32> &missingSequences = m_missingSpectrumNumbersByChannel.at(channel - 1);
        if (missingSequences.isEmpty())
            continue;

        QStringList missingLabels;
        missingLabels.reserve(missingSequences.size());
        for (quint32 missingSequence : missingSequences)
            missingLabels << QString::number(missingSequence);

        qInfo() << QString("通道%1缺失能谱序号: %2")
            .arg(channel)
            .arg(missingLabels.join(", "));
    }
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

void MainWindow::updateSpectrumRefreshIntervalRange()
{
    // 传输模式：0=512道能谱(min 10ms)，1=16道能谱(min 1ms)，2=波形(min 10ms)
    const int mode = ui->cmb_transferMode->currentIndex();
    const int minMs = (mode == 1) ? 1 : 10;

    ui->spb_specRefashTime->setMinimum(minMs);
    if (ui->spb_specRefashTime->value() < minMs)
        ui->spb_specRefashTime->setValue(minMs);
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

void MainWindow::refreshWaveformPlot()
{
    const int channel = ui->spb_channel->value();
    if (channel < 1 || channel > m_waveformByChannel.size()) {
        ui->plotWave->clearData();
        ui->plotWave->refreshPlot();
        return;
    }

    const QVector<quint16> &samples = m_waveformByChannel.at(channel - 1);
    if (samples.isEmpty()) {
        ui->plotWave->clearData();
        ui->plotWave->setTitle(QString("波形图 - 通道 %1 (无数据)").arg(channel));
        ui->plotWave->refreshPlot();
        return;
    }

    const int n = samples.size();
    QVector<double> xs(n), ys(n);
    for (int i = 0; i < n; ++i) {
        xs[i] = static_cast<double>(i) * 16.0;
        ys[i] = static_cast<double>(samples.at(i));
    }

    ui->plotWave->setData(xs, ys);
    ui->plotWave->setTitle(QString("波形图 - 通道 %1").arg(channel));
    ui->plotWave->refreshPlot();
}

void MainWindow::appendUdpLog(const QString &line)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->tbLog_UDP->append(QStringLiteral("[%1] %2").arg(ts, line));
}

void MainWindow::onUdpDatagramReceived(const QString &asciiText, const QString &senderInfo)
{
    appendUdpLog(QStringLiteral("RECV from %1: %2").arg(senderInfo, asciiText));
}

void MainWindow::onUdpBindStateChanged(bool bound, const QString &message)
{
    appendUdpLog(message);
    if (bound)
        qInfo() << message;
    else
        qWarning() << message;
}

quint16 MainWindow::udpBroadcastPort() const
{
    JsonSettings *runSettings = GlobalSettings::instance()->mRunSettings;
    ScopedFileLock lock(runSettings);
    return static_cast<quint16>(runSettings->getValueByPath("Network/udpBroadcastPort", 6000).toInt());
}

void MainWindow::startUdpListening()
{
    if (m_udpListening)
        return;

    if (!m_udpShotReceiver->startListening(udpBroadcastPort()))
        return;

    m_udpListening = true;
}

void MainWindow::stopUdpListening()
{
    if (!m_udpListening && !m_udpShotReceiver->isListening())
        return;

    m_udpShotReceiver->stopListening();
    m_udpListening = false;
}

void MainWindow::saveShotNumberFile(const QString &shotNumber) const
{
    const QString savePath = ui->le_savePath->text().trimmed();
    if (savePath.isEmpty())
        return;

    QDir dir(savePath);
    if (!dir.exists() && !dir.mkpath("."))
        return;

    const QString filePath = dir.filePath(QStringLiteral("ShotNumber.txt"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QByteArray payload = shotNumber.toUtf8();
    payload.append('\n');
    file.write(payload);
}

void MainWindow::onUdpShotNumberChanged(const QString &shotNumber)
{
    if (shotNumber == m_currentShotNumber)
        return;

    m_currentShotNumber = shotNumber;
    ui->lineEdit_shotID->setText(shotNumber);
    saveShotNumberFile(shotNumber);

    const QString info = tr("炮号已刷新：%1").arg(shotNumber);
    qInfo() << info;
    appendUdpLog(info);

    // 自动测量：收到新炮号后自动开始一次测量
    const int measureMode = ui->comboBox_2->currentIndex();
    if (measureMode != 1)
        return;

    if (measureTimer->isActive())
        on_btn_stopMeasure_clicked();

    QTimer::singleShot(100, this, [this]() {
        if (!startMeasureInternal())
            qWarning() << "UDP 触发自动测量失败";
    });
}

bool MainWindow::startMeasureInternal()
{
    ui->plotWave->clearData();
    ui->plotWave->refreshPlot();
    ui->plotSpec->clearData();
    ui->plotSpec->refreshPlot();
    clearSpectrumData();
    resetWaveformCounters();
    resetSpectrumSequenceTracking();
    spectrumPlotThrottle.invalidate();

    DetParameter detPara = {};
    detPara.trigMode = Order::TriggerMode::SoftwareTrigger;

    const int mode = ui->cmb_transferMode->currentIndex();
    if (mode == 0) {
        detPara.transferMode = Order::TransferMode::Spectrum512;
    } else if (mode == 1) {
        detPara.transferMode = Order::TransferMode::Spectrum16;
    }
    detPara.measureTime = ui->spb_measureTime->value();

    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    detPara.spectrumRefreshInterval = ui->spb_specRefashTime->value();
    detPara.spectrumTriggerThreshold = settings->getValueByPath("FPGA/threshold").toInt();
    detPara.spectrumDeadTime = settings->getValueByPath("FPGA/deadTime").toInt();

    for (int channel = 1; channel <= 32; ++channel) {
        detPara.waveformTriggerThreshold[channel - 1] =
            settings->getValueByPath(QString("FPGA/wave/threshold%1").arg(channel), 50).toInt();
    }

    {
        waveformPlotTimer->start();

        if (detPara.transferMode == Order::TransferMode::Spectrum16) {
            ui->plotSpec->setXRange(1, 16);
        } else {
            ui->plotSpec->setXRange(1, 512);
        }
    }

    const QString savePath = ui->le_savePath->text();
    if (savePath.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择数据保存路径"));
        return false;
    }

    mdetPara = detPara;

    QDir dir;
    if (!dir.exists(savePath) && !dir.mkpath(savePath)) {
        QMessageBox::information(this, tr("提示"), tr("请先选择数据保存路径"));
        return false;
    }

    const QString shotNumber = ui->lineEdit_shotID->text().trimmed();
    m_currentShotNumber = shotNumber;

    commandHelper->setSaveFileFormat(ui->cmb_saveFormat->currentIndex() == 0 ? Binary : Text);
    commandHelper->setSavePath(QDir::toNativeSeparators(QFileInfo(savePath).absoluteFilePath()));
    commandHelper->setShotNumber(shotNumber);

    const int measureMode = ui->comboBox_2->currentIndex();
    if (measureMode == 2){
        // 开启无人值守模式
        // 1. 定义两个定时器和成员变量
        if (startTimer == nullptr)
            startTimer = new QTimer(this);
        if (stopTimer == nullptr)
            stopTimer = new QTimer(this);
        isTaskRunning = true;

        // 2. 设置时间点A（启动时间），计算时间差后启动启动定时器
        QDateTime targetA = ui->dateTimeEdit->dateTime();
        int msecToA = QDateTime::currentDateTime().msecsTo(targetA);

        // 3. 设置时间点B（停止时间）
        QDateTime targetB = ui->dateTimeEdit_2->dateTime();
        int msecToB = QDateTime::currentDateTime().msecsTo(targetB);

        if (msecToA < 0 || msecToA > msecToB){
            QMessageBox::information(this, tr("提示"), tr("时间范围设置不对！\n自动开机时刻必须大于系统当前时间，且关机时间也必须大于开机时间。"));
            return false;
        }

        ui->btn_startMeasure->setEnabled(false);

        // 4. 绑定信号槽：时间A到了启动任务
        connect(startTimer, &QTimer::timeout, this, [=]() {
            startTimer->stop();

            // 启动工作定
            commandHelper->startMeasure(detPara);
            measureTimer->start(detPara.measureTime);
            qInfo() << "系统开机，测量已开始，炮号:" << shotNumber << "时长:" << detPara.measureTime << "ms";
        });

        // 5. 绑定信号槽：时间B到了停止任务
        connect(stopTimer, &QTimer::timeout, this, [&]() {
            isTaskRunning = false;
            stopTimer->stop();
            ui->btn_startMeasure->setEnabled(true);

            // 停止工作定
            waveformPlotTimer->stop();
            commandHelper->stopMeasure();
            printWaveformCollectionSummary();
            printSpectrumSequenceSummary();
            measureTimer->stop();
            qInfo() << "系统自动退出";
        });

        startTimer->start(msecToA);
        stopTimer->start(msecToB);
    }
    else
    {
        commandHelper->startMeasure(detPara);
        measureTimer->start(detPara.measureTime);
        qInfo() << "测量已开始，炮号:" << shotNumber << "时长:" << detPara.measureTime << "ms";
    }

    return true;
}

// 开始测量
void MainWindow::on_btn_startMeasure_clicked()
{
    startMeasureInternal();
}

//打开探测器供电电源，通过继电器进行控制探测器的电源
void MainWindow::on_bt_powerOn_clicked()
{
    startUdpListening();
    commandHelper->PowerOnRelay();
}

//关闭探测器供电电源，通过继电器进行控制探测器的电源
void MainWindow::on_bt_powerOff_clicked()
{
    stopUdpListening();
    commandHelper->PowerOffRelay();
}

//连接探测器网络
void MainWindow::on_bt_connectDet_clicked()
{
    commandHelper->connectARM();
    commandHelper->connectDetector();
}

//断开探测器网络
void MainWindow::on_bt_disconnectDet_clicked()
{
    commandHelper->disconnectARM();
    commandHelper->disconnectDetector();
}


void MainWindow::on_pushButton_clearSysLog_clicked()
{
    ui->plainTextEdit_log->clear();
}


void MainWindow::on_pushButton_clearNetLog_clicked()
{
    ui->tbLog_UDP->clear();
}

using namespace std;

// 随机数生成器（全局复用，避免重复初始化）
static mt19937 gen(chrono::steady_clock::now().time_since_epoch().count());

// 生成带平滑波动的数值
double generateValue(double base, double maxDelta, double& lastValue) {
    // 生成小幅度正态分布波动（波动幅度为最大范围的1/5，模拟连续变化）
    normal_distribution<double> dist(0, maxDelta / 5.0);
    double delta = dist(gen);

    // 限制单次波动幅度不超过最大范围的1/3，避免跳变
    delta = max(-maxDelta/3, min(delta, maxDelta/3));

    // 计算新值并限制在允许范围内
    double newValue = lastValue + delta;
    newValue = max(base - maxDelta, min(newValue, base + maxDelta));

    // 更新上一次值
    lastValue = newValue;
    return newValue;
}


void MainWindow::on_btn_stopMeasure_clicked()
{
    if (isTaskRunning){
        if (startTimer->isActive())
            startTimer->stop();

        if (stopTimer->isActive())
            stopTimer->stop();

        isTaskRunning = false;
        ui->btn_startMeasure->setEnabled(true);
    }

    waveformPlotTimer->stop();
    commandHelper->stopMeasure();
    printWaveformCollectionSummary();
    printSpectrumSequenceSummary();
    measureTimer->stop();
    qInfo() << "手动停止测量";
}


void MainWindow::on_action_hardwareSetting_triggered()
{
    // 硬件参数设置
    DetectorSetting *settDialog = new DetectorSetting();
    settDialog->setAttribute(Qt::WA_DeleteOnClose);
    settDialog->setUdpPortEditable(!m_udpListening);
    settDialog->exec();
}

#include "otaupgradewindow.h"
void MainWindow::on_actionFPGA_triggered()
{
    // FPGA远程更新
    OTAUpgradeWindow *w = new OTAUpgradeWindow(this);
    w->setAttribute(Qt::WA_DeleteOnClose, true);
    w->setWindowFlags(Qt::WindowCloseButtonHint|Qt::Dialog);
    w->setWindowModality(Qt::ApplicationModal);
    w->showNormal();
}


void MainWindow::on_action_relayNetOpen_triggered()
{
    commandHelper->connectRelay();
}


void MainWindow::on_action_relayNetClose_triggered()
{
    commandHelper->disconnectRelay();
}


void MainWindow::on_action_powerOn_triggered()
{
    startUdpListening();
    commandHelper->PowerOnRelay();
}


void MainWindow::on_action_powerOff_triggered()
{
    stopUdpListening();
    commandHelper->PowerOffRelay();
}


void MainWindow::on_action_connectDet_triggered()
{
    commandHelper->connectARM();
    commandHelper->connectDetector();
}



void MainWindow::on_action_disconnectDet_triggered()
{
    commandHelper->disconnectARM();
    commandHelper->disconnectDetector();
}


void MainWindow::on_action_startMeasure_triggered()
{
    startMeasureInternal();
}


void MainWindow::on_action_stopMeasure_triggered()
{
    waveformPlotTimer->stop();
    commandHelper->stopMeasure();
    printWaveformCollectionSummary();
    printSpectrumSequenceSummary();
    measureTimer->stop();
    qInfo() << "手动停止测量";
}

