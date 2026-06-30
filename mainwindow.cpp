/*
 * @Author: MrPan
 * @Date: 2026-03-23 10:31:29
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-28 11:37:33
 * @Description: 请填写简介
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "switchbutton.h"
#include <QFileDialog>
#include <QToolButton>
#include <QAction>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QDir>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <cmath>
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
    measureTimer->setSingleShot(true);
    connect(measureTimer, &QTimer::timeout, this, &MainWindow::onMeasureTimerTimeout);
    ui->setupUi(this);
    loadMonitorAlarmSettings();
    connect(ui->doubleSpinBox_temp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { saveMonitorAlarmSettings(); });
    connect(ui->doubleSpinBox_voltage, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { saveMonitorAlarmSettings(); });
    connect(ui->doubleSpinBox_current, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { saveMonitorAlarmSettings(); });

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

    ui->plotSpec->setTitle("分时能谱");
    ui->plotSpec->setXAxisLabel("道址");
    ui->plotSpec->setYAxisLabel("计数");
    ui->plotSpec->setXRange(1,512);

    ui->plotProfile->setTitle("剖面分布");
    ui->plotProfile->setXAxisLabel("编号");
    ui->plotProfile->setYAxisLabel("计数");
    ui->plotProfile->setXRange(kProfileZMin, kProfileZMax);
    ui->plotProfile->ensureGraphCount(2);
    ui->plotProfile->setGraphColor(0, Qt::blue);
    ui->plotProfile->setGraphColor(1, Qt::red);
    ui->plotProfile->setGraphName(0, QStringLiteral("垂直剖面"));
    ui->plotProfile->setGraphName(1, QStringLiteral("水平剖面"));
    ui->plotProfile->setLegendVisible(true);

    {
        //信号采集机箱
        // ui->plotVoltage->setTitle("电压曲线");
        ui->plotVoltage->ensureGraphCount(1);
        ui->plotVoltage->setXAxisLabel("时间");
        ui->plotVoltage->setYAxisLabel("电压 (V)");
        ui->plotVoltage->setTimeWindow(180);
        ui->plotVoltage->setupNumericLegend();

        // ui->plotCurrent->setTitle("电流曲线");
        ui->plotCurrent->ensureGraphCount(1);
        ui->plotCurrent->setXAxisLabel("时间");
        ui->plotCurrent->setYAxisLabel("电流 (A)");
        ui->plotCurrent->setTimeWindow(180);
        ui->plotCurrent->setupNumericLegend();

        // ui->plotTemp->setTitle("温度曲线");// 共3条
        ui->plotTemp->ensureGraphCount(3);
        ui->plotTemp->setXAxisLabel("时间");
        ui->plotTemp->setYAxisLabel("温度 (℃)");
        ui->plotTemp->setTimeWindow(180);
        ui->plotTemp->setupNumericLegend();
    }
    {
        //电源机箱
        // ui->plotVoltage->setTitle("电压曲线");
        ui->plotVoltage_2->ensureGraphCount(4);
        ui->plotVoltage_2->setXAxisLabel("时间");
        ui->plotVoltage_2->setYAxisLabel("电压 (V)");
        ui->plotVoltage_2->setTimeWindow(180);
        ui->plotVoltage_2->setupNumericLegend();

        // ui->plotCurrent->setTitle("电流曲线");
        ui->plotCurrent_2->ensureGraphCount(4);
        ui->plotCurrent_2->setXAxisLabel("时间");
        ui->plotCurrent_2->setYAxisLabel("电流 (A)");
        ui->plotCurrent_2->setTimeWindow(180);
        ui->plotCurrent_2->setupNumericLegend();

        // ui->plotTemp->setTitle("温度曲线");// 共6条
        ui->plotTemp_2->ensureGraphCount(6);
        ui->plotTemp_2->setXAxisLabel("时间");
        ui->plotTemp_2->setYAxisLabel("温度 (℃)");
        ui->plotTemp_2->setTimeWindow(180);
        ui->plotTemp_2->setupNumericLegend();
    }
    commandHelper = CommandHelper::instance();
    connect(commandHelper, &CommandHelper::sigAppendMsg, this, &MainWindow::slotAppendMsg);
    connect(commandHelper, &CommandHelper::sigRelayStatus, this, &MainWindow::onRelayStatusChanged);
    connect(commandHelper, &CommandHelper::sigRelayConnectError, this, [=](QAbstractSocket::SocketError) {
        ui->btn_relayNetOpen->blockSignals(true);
        ui->btn_relayNetOpen->setChecked(false);
        ui->btn_relayNetOpen->setText(QStringLiteral("连接远程控制"));
        ui->btn_relayNetOpen->blockSignals(false);
    });
    connect(commandHelper, &CommandHelper::sigRelayPowerStatus, this, &MainWindow::onRelayPowerStatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector1Status, this, &MainWindow::onDetector1StatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector2Status, this, &MainWindow::onDetector2StatusChanged);
    connect(commandHelper, &CommandHelper::sigStatus_fpga1_wave, this, &MainWindow::onDetector3StatusChanged);
    connect(commandHelper, &CommandHelper::sigStatus_fpga2_wave, this, &MainWindow::onDetector4StatusChanged);
    connect(commandHelper, &CommandHelper::sigDetector1Fault, this, &MainWindow::onDetector1ConnectFault);
    connect(commandHelper, &CommandHelper::sigDetector2Fault, this, &MainWindow::onDetector2ConnectFault);
    connect(commandHelper, &CommandHelper::sigFault_fpga1_wave, this, &MainWindow::onDetector3ConnectFault);
    connect(commandHelper, &CommandHelper::sigFault_fpga2_wave, this, &MainWindow::onDetector4ConnectFault);
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
    connect(ui->spb_waveID, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshWaveformPlot);
    connect(ui->spb_waveLen, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshWaveformPlot);
    ui->spb_specID->setMinimum(0);
    ui->spb_waveID->setMinimum(0);
    ui->spb_waveLen->setMinimum(static_cast<int>(kWaveformSampleIntervalNs));
    ui->spb_waveLen->setMaximum(kWaveformMaxDisplayNs);
    ui->spb_waveLen->setSingleStep(static_cast<int>(kWaveformSampleIntervalNs));
    ui->spb_waveLen->setValue(10000);
    updateSpecIdSpinBoxRange();
    updateWaveIdSpinBoxRange();

    connect(ui->cmb_transferMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateSpectrumRefreshIntervalRange);
    connect(ui->cmb_transferMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateHxrDisplayBinControls);
    connect(ui->cmb_transferMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateProfileControls);
    connect(ui->comboBox_2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateUnattendedControls);
    connect(ui->spb_hxrDisplayBins, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshSpectrumPlot);
    connect(ui->spb_profileID, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshProfilePlot);
    loadMeasureSettings();

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
    ui->btn_relayNetOpen->setText(QStringLiteral("连接远程控制"));

    ui->bt_connectDet->setCheckable(true);
    ui->bt_connectDet->setChecked(false);
    ui->bt_connectDet->setText(QStringLiteral("连接采集系统"));

    ui->btn_connectMonitor->setCheckable(true);
    ui->btn_connectMonitor->setChecked(false);
    ui->btn_connectMonitor->setText(QStringLiteral("连接实时监测系统"));

    ui->switch_power->setAutoChecked(false);
    ui->switch_power->setText(QStringLiteral("远程上电"), QStringLiteral("远程断电"));
    ui->switch_power->setChecked(false);
    setPowerSwitchEnabled(false);
    connect(ui->switch_power, &SwitchButton::clicked, this, [this]() {
        if (!ui->switch_power->getChecked()) {
            commandHelper->PowerOnRelay();
            showHardwareStartupWaitDialog();
        } else {
            commandHelper->PowerOffRelay();
        }
    });

    ui->bt_connectDet->setEnabled(false);
    ui->btn_startMeasure->setEnabled(false);
    ui->btn_stopMeasure->setEnabled(false);
    emit ui->comboBox_2->currentIndexChanged(ui->comboBox_2->currentIndex());

    // 初始化状态机
    initStateMachine();

    // 开机自动最大化：一次性延迟调用，保留 lambda
    QTimer::singleShot(0, this, [this] { showMaximized(); });
}

MainWindow::~MainWindow()
{
    saveMonitorAlarmSettings();
    saveMeasureSettings();
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

    if (m_autoMeasureState == AutoMeasureState::Measuring) {
        m_autoMeasureState = AutoMeasureState::Idle;
        m_autoMeasureDurationTimerStarted = false;
        ui->btn_startMeasure->setEnabled(true);
        ui->btn_stopMeasure->setEnabled(false);
    }

    ui->btn_startMeasure->setEnabled(true);
    ui->btn_stopMeasure->setEnabled(false);
    ui->comboBox_2->setEnabled(true);
    ui->dateTimeEdit->setEnabled(true);
    ui->dateTimeEdit_2->setEnabled(true);

    if (0 == ui->comboBox_2->currentIndex())
        qInfo() << "手动测量时长已到，测量已停止";
    else if (1 == ui->comboBox_2->currentIndex())
        qInfo() << "自动测量时长已到，测量已停止";
    else
        qInfo() << "无人值守测量时长已到，测量已停止";
}

void MainWindow::onRelayStatusChanged(bool on)
{
    ui->btn_relayNetOpen->blockSignals(true);
    ui->btn_relayNetOpen->setChecked(on);
    ui->btn_relayNetOpen->setText(on ? QStringLiteral("断开远程控制") : QStringLiteral("连接远程控制"));
    ui->btn_relayNetOpen->blockSignals(false);

    if (on){
        setPowerSwitchEnabled(true);

        qInfo() << "继电器网络状态: 已连接";

        QTimer::singleShot(500, this, [=]{
            // 延迟打开电源，否则指令会发出去无响应
            emit step2Finished();// 连接远程控制
        });
    }
    else{
        setPowerSwitchEnabled(false);
        syncPowerSwitchFromRelay(false);
        ui->bt_connectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);
        qInfo() << "继电器网络状态: 已断开";
    }
}

void MainWindow::onRelayPowerStatusChanged(bool on)
{
    syncPowerSwitchFromRelay(on);

    if (on) {
        ui->bt_connectDet->setEnabled(true);
        syncDetectorConnectButton();

        qInfo() << "继电器控制的电源状态: 已开启";
        replayPowerOn = true;
    } else {
        ui->bt_connectDet->setEnabled(false);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(false);

        qInfo() << "继电器控制的电源状态: 已关闭";
        replayPowerOn = false;
        if (m_autoMeasureState == AutoMeasureState::Measuring) {
            measureTimer->stop();
            waveformPlotTimer->stop();
            commandHelper->stopMeasure();
            m_autoMeasureState = AutoMeasureState::Idle;
            m_autoMeasureDurationTimerStarted = false;
        } else if (m_autoMeasureState == AutoMeasureState::WaitingShot) {
            measureTimer->stop();
            waveformPlotTimer->stop();
            commandHelper->closeMeasurementFiles();
            m_autoMeasureState = AutoMeasureState::Idle;
            m_autoMeasureDurationTimerStarted = false;
        }
        
        // 断电后清除各设备在线标记
        detectOnline[0] = false;
        detectOnline[1] = false;
        detectOnline[2] = false;
        detectOnline[3] = false;
        syncDetectorConnectButton();
    }
}

void MainWindow::onDetector1StatusChanged(bool on)
{
    if (on) {
        if (!isMeasureSessionActive())
        {
            ui->btn_startMeasure->setEnabled(true);
            ui->btn_stopMeasure->setEnabled(false);
        }

        qInfo() << "水平相机主网口(控制/能谱)状态: 已连接";
        detectOnline[0] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->btn_startMeasure->setEnabled(false);
            ui->btn_stopMeasure->setEnabled(false);
        }
        qInfo() << "水平相机主网口(控制/能谱)状态: 已断开";
        detectOnline[0] = false;
    }
    syncDetectorConnectButton();
}

void MainWindow::onDetector2StatusChanged(bool on)
{
    if (on) {
        if (!isMeasureSessionActive())
        {
            ui->btn_startMeasure->setEnabled(true);
            ui->btn_stopMeasure->setEnabled(false);
        }
        qInfo() << "垂直相机主网口(控制/能谱)状态: 已连接";
        detectOnline[1] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->btn_startMeasure->setEnabled(false);
            ui->btn_stopMeasure->setEnabled(false);
        }
        qInfo() << "垂直相机主网口(控制/能谱)状态: 已断开";
        detectOnline[1] = false;
    }
    syncDetectorConnectButton();
}

void MainWindow::onDetector3StatusChanged(bool on)
{
    if (on) {
        if (!isMeasureSessionActive())
        {
            ui->btn_startMeasure->setEnabled(true);
            ui->btn_stopMeasure->setEnabled(false);
        }
        qInfo() << "水平相机副网口(波形接收)状态: 已连接";
        detectOnline[2] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->btn_startMeasure->setEnabled(false);
            ui->btn_stopMeasure->setEnabled(false);
        }
        qInfo() << "水平相机副网口(波形接收)状态: 已断开";
        detectOnline[2] = false;
    }
    syncDetectorConnectButton();
}

void MainWindow::onDetector4StatusChanged(bool on)
{
    if (on) {
        if (!isMeasureSessionActive())
        {
            ui->btn_startMeasure->setEnabled(true);
            ui->btn_stopMeasure->setEnabled(false);
        }
        qInfo() << "垂直相机副网口(波形接收)状态: 已连接";
        detectOnline[3] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->btn_startMeasure->setEnabled(false);
            ui->btn_stopMeasure->setEnabled(false);
        }
        qInfo() << "垂直相机副网口(波形接收)状态: 已断开";
        detectOnline[3] = false;
    }
    syncDetectorConnectButton();
}

void MainWindow::onDetector1ConnectFault()
{
    detectOnline[0] = false;
    syncDetectorConnectButton();
}

void MainWindow::onDetector2ConnectFault()
{
    detectOnline[1] = false;
    syncDetectorConnectButton();
}

void MainWindow::onDetector3ConnectFault()
{
    detectOnline[2] = false;
    syncDetectorConnectButton();
}

void MainWindow::onDetector4ConnectFault()
{
    detectOnline[3] = false;
    syncDetectorConnectButton();
}

void MainWindow::onArm1StatusChanged(bool on)
{
    if (on) {
        qInfo() << "状态监测设备1网络：已连接";
        armSensorOnline[0] = true;
    } else {
        qInfo() << "状态监测设备1网络：已断开";
        armSensorOnline[0] = false;
    }
    syncArmMonitorButton();
}

void MainWindow::onArm2StatusChanged(bool on)
{
    if (on) {
        qInfo() << "状态监测设备2网络：已连接";
        armSensorOnline[1] = true;        
    } else {
        qInfo() << "状态监测设备2网络：已断开";
        armSensorOnline[1] = false;
    }
    syncArmMonitorButton();
}

QString MainWindow::armMonitorSaveDir() const
{
    const QString monitorPath = QDir(QCoreApplication::applicationDirPath())
                                    .filePath(QStringLiteral("Monitor"));
    if (!QDir(monitorPath).exists() && !QDir().mkpath(monitorPath)) {
        qWarning() << tr("创建监测数据目录失败:") << monitorPath;
        return {};
    }
    return monitorPath;
}

void MainWindow::saveArmMonitorData(int armIndex, const QVector<double> &temperature,
                                    const QVector<double> &voltage, const QVector<double> &current)
{
    const QString monitorPath = armMonitorSaveDir();
    if (monitorPath.isEmpty())
        return;

    const QString dateTag = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd"));
    const QString filePath = QDir(monitorPath).filePath(
        QStringLiteral("Arm%1Monitor_%2.csv").arg(armIndex).arg(dateTag));

    const bool isNewFile = !QFile::exists(filePath);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif

    if (isNewFile) {
        QStringList header;
        header << QStringLiteral("Time");
        for (int i = 0; i < voltage.size(); ++i)
            header << QStringLiteral("Voltage%1(V)").arg(i + 1);
        for (int i = 0; i < current.size(); ++i)
            header << QStringLiteral("Current%1(A)").arg(i + 1);
        for (int i = 0; i < temperature.size(); ++i)
            header << QStringLiteral("Temp_CH%1(C)").arg(i + 1);
        out << header.join(QStringLiteral(",")) << '\n';
    }

    QStringList values;
    values << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    auto appendValues = [&](const QVector<double> &data) {
        for (double value : data)
            values << QString::number(value, 'f', 3);
    };
    appendValues(voltage);
    appendValues(current);
    appendValues(temperature);
    out << values.join(QStringLiteral(",")) << '\n';
}

void MainWindow::checkArmMonitorAlarm(int armIndex, const QVector<double> &temperature,
                                      const QVector<double> &voltage, const QVector<double> &current)
{
    if (!ui->checkBox_alarm->isChecked())
        return ;

    const double tempLimit = ui->doubleSpinBox_temp->value();
    const double voltageLimit = ui->doubleSpinBox_voltage->value();
    const double currentLimit = ui->doubleSpinBox_current->value();

    QStringList alarmDetails;
    for (int i = 0; i < temperature.size(); ++i) {
        if (temperature[i] > tempLimit) {
            alarmDetails << tr("温度CH%1=%2°C>阈值%3°C")
                                .arg(i + 1)
                                .arg(temperature[i], 0, 'f', 1)
                                .arg(tempLimit, 0, 'f', 1);
        }
    }
    for (int i = 0; i < voltage.size(); ++i) {
        if (voltage[i] > voltageLimit) {
            alarmDetails << tr("电压CH%1=%2V>阈值%3V")
                                .arg(i + 1)
                                .arg(voltage[i], 0, 'f', 3)
                                .arg(voltageLimit, 0, 'f', 3);
        }
    }
    for (int i = 0; i < current.size(); ++i) {
        if (current[i] > currentLimit) {
            alarmDetails << tr("电流CH%1=%2A>阈值%3A")
                                .arg(i + 1)
                                .arg(current[i], 0, 'f', 3)
                                .arg(currentLimit, 0, 'f', 3);
        }
    }

    const int alarmIndex = armIndex - 1;
    if (alarmIndex < 0 || alarmIndex >= 2)
        return;

    const bool isAlarm = !alarmDetails.isEmpty();
    if (isAlarm) {
        if (!m_armMonitorInAlarm[alarmIndex]) {
            m_armMonitorInAlarm[alarmIndex] = true;
            const QString armName = (armIndex == 1)
                                        ? QStringLiteral("信号采集机箱(ARM1)")
                                        : QStringLiteral("电源机箱(ARM2)");
            qWarning().noquote() << QStringLiteral("[%1]监测超限，断开继电器：%2")
                                        .arg(armName, alarmDetails.join(QStringLiteral("；")));
            commandHelper->PowerOffRelay();
        }
    } else {
        m_armMonitorInAlarm[alarmIndex] = false;
    }
}

void MainWindow::handleArmSensorData(int armIndex, const QVector<double> &temperature,
                                     const QVector<double> &voltage, const QVector<double> &current)
{
    if (armIndex == 1) {
        ui->plotTemp->appendPoints(0, temperature);
        ui->plotVoltage->appendPoints(0, voltage);
        ui->plotCurrent->appendPoints(0, current);
        ui->plotTemp->refreshPlot();
        ui->plotVoltage->refreshPlot();
        ui->plotCurrent->refreshPlot();
    } else {
        ui->plotTemp_2->appendPoints(0, temperature);
        ui->plotVoltage_2->appendPoints(0, voltage);
        ui->plotCurrent_2->appendPoints(0, current);
        ui->plotTemp_2->refreshPlot();
        ui->plotVoltage_2->refreshPlot();
        ui->plotCurrent_2->refreshPlot();
    }

    saveArmMonitorData(armIndex, temperature, voltage, current);
    checkArmMonitorAlarm(armIndex, temperature, voltage, current);
}

void MainWindow::onArm1SensorData(const QVector<double>&/*温度*/ temperature, const QVector<double>&/*电压*/ voltage, const QVector<double>&/*电流*/ current)
{
    handleArmSensorData(1, temperature, voltage, current);
}

void MainWindow::onArm2SensorData(const QVector<double>&/*温度*/ temperature, const QVector<double>&/*电压*/ voltage, const QVector<double>&/*电流*/ current)
{
    handleArmSensorData(2, temperature, voltage, current);
}

void MainWindow::onSpectrumDataReceived(int detectorIndex, int channelNumber, quint32 timeMs,
                                        const QVector<quint32> &counts)
{
    const int logicalChannel = logicalChannelNumber(detectorIndex, channelNumber);
    if (logicalChannel < 1 || logicalChannel > m_spectrumByChannel.size())
        return;

    if (m_autoMeasureState == AutoMeasureState::Measuring
        && ui->comboBox_2->currentIndex() == 1
        && !m_autoMeasureDurationTimerStarted) {
        m_autoMeasureDurationTimerStarted = true;
        measureTimer->start(mdetPara.measureTime);
        qInfo() << "收到首个能谱数据，开始测量倒计时:" << mdetPara.measureTime << "ms";
    }

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
    appendWaveformData(detectorIndex, channelNumber, timeUnits, samples);

    const quint32 waveformSequence = timeUnits;
    m_waveformSequenceNumbersByChannel[channelIndex].append(waveformSequence);
    if (!m_hasWaveformSequenceByChannel[channelIndex]) {
        m_hasWaveformSequenceByChannel[channelIndex] = true;
        m_lastWaveformSequenceByChannel[channelIndex] = waveformSequence;
    } else if (waveformSequence > m_lastWaveformSequenceByChannel[channelIndex]) {
        for (quint32 missingSequence = m_lastWaveformSequenceByChannel[channelIndex] + 1;
             missingSequence < waveformSequence;
             ++missingSequence) {
            m_missingWaveformNumbersByChannel[channelIndex].append(missingSequence);
        }
        m_lastWaveformSequenceByChannel[channelIndex] = waveformSequence;
    }

    const int currentChannel = ui->spb_channel->value();
    if (logicalChannel != currentChannel)
        return;

    const int waveCount = m_waveformByChannel.at(currentChannel - 1).size();
    ui->spb_waveID->blockSignals(true);
    ui->spb_waveID->setValue(waveCount - 1);
    ui->spb_waveID->blockSignals(false);
    updateWaveIdSpinBoxRange();
}

void MainWindow::onChannelSpinBoxChanged(int /*value*/)
{
    updateSpecIdSpinBoxRange();
    updateWaveIdSpinBoxRange();
    refreshSpectrumPlot();
    refreshWaveformPlot();
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
    std::fill(m_lastWaveformSequenceByChannel.begin(), m_lastWaveformSequenceByChannel.end(), 0);
    std::fill(m_hasWaveformSequenceByChannel.begin(), m_hasWaveformSequenceByChannel.end(), false);
    for (auto &waveforms : m_waveformByChannel)
        waveforms.clear();
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
    clearProfileData();
}

void MainWindow::clearWaveformData()
{
    for (auto &channelWaveforms : m_waveformByChannel)
        channelWaveforms.clear();

    ui->spb_waveID->blockSignals(true);
    ui->spb_waveID->setValue(0);
    ui->spb_waveID->blockSignals(false);
    updateWaveIdSpinBoxRange();
    ui->plotWave->clearData();
    ui->plotWave->refreshPlot();
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

void MainWindow::appendWaveformData(int detectorIndex, int channelNumber, quint32 timeUnits,
                                    const QVector<quint16> &samples)
{
    const int storageChannel = logicalChannelNumber(detectorIndex, channelNumber);
    if (storageChannel < 1 || storageChannel > m_waveformByChannel.size())
        return;

    WaveformEntry entry;
    entry.detectorIndex = detectorIndex;
    entry.timeUnits = timeUnits;
    entry.samples = samples;
    m_waveformByChannel[storageChannel - 1].append(entry);
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

void MainWindow::updateWaveIdSpinBoxRange()
{
    const int channel = ui->spb_channel->value();
    const int waveCount = (channel >= 1 && channel <= m_waveformByChannel.size())
                              ? m_waveformByChannel.at(channel - 1).size()
                              : 0;
    const int maxWaveId = qMax(0, waveCount - 1);

    ui->spb_waveID->blockSignals(true);
    ui->spb_waveID->setMaximum(maxWaveId);
    if (ui->spb_waveID->value() > maxWaveId)
        ui->spb_waveID->setValue(maxWaveId);
    ui->spb_waveID->blockSignals(false);
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

void MainWindow::updateHxrDisplayBinControls()
{
    const bool hxrMode = ui->cmb_transferMode->currentIndex() == 1;
    ui->label_hxrDisplayBins->setVisible(hxrMode);
    ui->spb_hxrDisplayBins->setVisible(hxrMode);

    if (hxrMode)
        ui->plotSpec->setXRange(1, hxrDisplayBinCount());
    else
        ui->plotSpec->setXRange(1, 512);

    refreshSpectrumPlot();
}

int MainWindow::hxrDisplayBinCount() const
{
    return ui->spb_hxrDisplayBins->value();
}

void MainWindow::updateProfileControls()
{
    const bool spectrum512Mode = ui->cmb_transferMode->currentIndex() == 0;
    ui->label_energyLeft->setVisible(spectrum512Mode);
    ui->dsbx_energyLeft->setVisible(spectrum512Mode);
    ui->label_energyRight->setVisible(spectrum512Mode);
    ui->dsbx_energyRight->setVisible(spectrum512Mode);
    ui->btn_generateProfile->setVisible(spectrum512Mode);
    ui->label_profileID->setVisible(spectrum512Mode);
    ui->spb_profileID->setVisible(spectrum512Mode);
}

void MainWindow::updateUnattendedControls()
{
    const bool unattendedMode = ui->comboBox_2->currentIndex() == 2;
    ui->label_9->setVisible(unattendedMode);
    ui->dateTimeEdit->setVisible(unattendedMode);
    ui->label_10->setVisible(unattendedMode);
    ui->dateTimeEdit_2->setVisible(unattendedMode);
}

QString MainWindow::energyCalibrationFilePath() const
{
    const QString fileName = QStringLiteral("能量刻度.csv");
    const QString appDirPath = QCoreApplication::applicationDirPath();
    const QString appDirFile = QDir(appDirPath).filePath(fileName);
    if (QFile::exists(appDirFile))
        return appDirFile;

    const QString currentDirFile = QDir::currentPath() + QDir::separator() + fileName;
    if (QFile::exists(currentDirFile))
        return currentDirFile;

    return appDirFile;
}

bool MainWindow::loadEnergyCalibration(QVector<EnergyCalibration> &calibration,
                                        QString *errorMessage) const
{
    calibration.clear();
    calibration.resize(kProfileChannelCount);

    const QString filePath = energyCalibrationFilePath();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = tr("无法打开能量刻度文件：%1").arg(filePath);
        return false;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    if (in.atEnd()) {
        if (errorMessage)
            *errorMessage = tr("能量刻度文件为空：%1").arg(filePath);
        return false;
    }

    const QString header = in.readLine().trimmed();
    if (!header.startsWith(QStringLiteral("channel"), Qt::CaseInsensitive)) {
        if (errorMessage)
            *errorMessage = tr("能量刻度文件首行应为标题 channel,k,b");
        return false;
    }

    int loadedRows = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        const QStringList fields = line.split(QLatin1Char(','));
        if (fields.size() < 3) {
            if (errorMessage)
                *errorMessage = tr("能量刻度文件格式错误：%1").arg(line);
            return false;
        }

        bool okChannel = false;
        bool okK = false;
        bool okB = false;
        const int channel = fields.at(0).trimmed().toInt(&okChannel);
        const double k = fields.at(1).trimmed().toDouble(&okK);
        const double b = fields.at(2).trimmed().toDouble(&okB);
        if (!okChannel || !okK || !okB || channel < 1 || channel > kProfileChannelCount) {
            if (errorMessage)
                *errorMessage = tr("能量刻度文件数据无效：%1").arg(line);
            return false;
        }
        if (qFuzzyIsNull(k)) {
            if (errorMessage)
                *errorMessage = tr("通道 %1 的 k 系数不能为 0").arg(channel);
            return false;
        }

        calibration[channel - 1].k = k;
        calibration[channel - 1].b = b;
        ++loadedRows;
    }

    if (loadedRows != kProfileChannelCount) {
        if (errorMessage) {
            *errorMessage = tr("能量刻度文件应包含 %1 个通道，当前 %2 个")
                                .arg(kProfileChannelCount)
                                .arg(loadedRows);
        }
        return false;
    }

    return true;
}

void MainWindow::energyToBinRange(double energyLeft, double energyRight,
                                  const EnergyCalibration &cal,
                                  int &binStart, int &binEnd) const
{
    const double eMin = qMin(energyLeft, energyRight);
    const double eMax = qMax(energyLeft, energyRight);

    double chLow = (eMin - cal.b) / cal.k;
    double chHigh = (eMax - cal.b) / cal.k;
    if (chLow > chHigh)
        std::swap(chLow, chHigh);

    binStart = qBound(1, static_cast<int>(std::floor(chLow)), kSpectrum512BinCount);
    binEnd = qBound(1, static_cast<int>(std::ceil(chHigh)), kSpectrum512BinCount);
    if (binStart > binEnd)
        std::swap(binStart, binEnd);
}

quint64 MainWindow::sumCountsInBinRange(const QVector<quint32> &counts,
                                        int binStart, int binEnd) const
{
    if (counts.isEmpty() || binStart < 1 || binEnd < binStart)
        return 0;

    const int startIndex = binStart - 1;
    const int endIndex = qMin(binEnd, counts.size()) - 1;
    if (startIndex >= counts.size())
        return 0;

    quint64 total = 0;
    for (int i = startIndex; i <= endIndex; ++i)
        total += counts.at(i);
    return total;
}

double MainWindow::profilePointPosition(int pointIndex) const
{
    if (pointIndex < 0 || pointIndex >= kVerticalCameraChannels)
        return 0.0;

    return kProfileZMin
           + pointIndex * (kProfileZMax - kProfileZMin)
                 / (kVerticalCameraChannels - 1);
}

void MainWindow::generateProfileSnapshots()
{
    if (ui->cmb_transferMode->currentIndex() != 0) {
        QMessageBox::information(this, tr("提示"), tr("剖面分布仅适用于512道能谱模式"));
        return;
    }

    const double energyLeft = ui->dsbx_energyLeft->value();
    const double energyRight = ui->dsbx_energyRight->value();
    if (energyLeft > energyRight) {
        QMessageBox::information(this, tr("提示"), tr("能量区间左端不能大于右端"));
        return;
    }

    QString calError;
    if (!loadEnergyCalibration(m_energyCalibration, &calError)) {
        QMessageBox::warning(this, tr("提示"), calError);
        return;
    }

    int profileCount = 0;
    for (const QVector<SpectrumEntry> &channelSpectra : m_spectrumByChannel) {
        profileCount = qMax(profileCount, channelSpectra.size());
    }
    if (profileCount <= 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有512道能谱数据，无法生成剖面分布"));
        return;
    }

    QVector<int> binStarts(kProfileChannelCount);
    QVector<int> binEnds(kProfileChannelCount);
    for (int ch = 0; ch < kProfileChannelCount; ++ch) {
        energyToBinRange(energyLeft, energyRight, m_energyCalibration.at(ch),
                         binStarts[ch], binEnds[ch]);
    }

    m_profileSnapshots.clear();
    m_profileSnapshots.reserve(profileCount);

    for (int profileIndex = 0; profileIndex < profileCount; ++profileIndex) {
        ProfileSnapshot snapshot;
        snapshot.counts.resize(kProfileChannelCount);
        snapshot.timeMs = 0;

        for (int ch = 0; ch < kProfileChannelCount; ++ch) {
            const QVector<SpectrumEntry> &spectra = m_spectrumByChannel.at(ch);
            if (profileIndex >= spectra.size()) {
                snapshot.counts[ch] = 0;
                continue;
            }

            const SpectrumEntry &entry = spectra.at(profileIndex);
            if (snapshot.timeMs == 0)
                snapshot.timeMs = entry.timeMs;
            snapshot.counts[ch] = sumCountsInBinRange(entry.counts, binStarts.at(ch), binEnds.at(ch));
        }

        m_profileSnapshots.append(snapshot);
    }

    ui->spb_profileID->blockSignals(true);
    ui->spb_profileID->setValue(profileCount - 1);
    ui->spb_profileID->blockSignals(false);
    updateProfileIdSpinBoxRange();
    refreshProfilePlot();

    qInfo() << QString("剖面分布已生成 %1 帧，能量区间 [%2, %3] keV")
                   .arg(profileCount)
                   .arg(energyLeft, 0, 'f', 1)
                   .arg(energyRight, 0, 'f', 1);
}

void MainWindow::updateProfileIdSpinBoxRange()
{
    const int maxProfileId = qMax(0, m_profileSnapshots.size() - 1);

    ui->spb_profileID->blockSignals(true);
    ui->spb_profileID->setMaximum(maxProfileId);
    if (ui->spb_profileID->value() > maxProfileId)
        ui->spb_profileID->setValue(maxProfileId);
    ui->spb_profileID->blockSignals(false);
}

void MainWindow::clearProfileData()
{
    m_profileSnapshots.clear();
    ui->spb_profileID->blockSignals(true);
    ui->spb_profileID->setValue(0);
    ui->spb_profileID->blockSignals(false);
    updateProfileIdSpinBoxRange();
    ui->plotProfile->clearAllGraphData();
    ui->plotProfile->refreshPlot(false, true);
}

void MainWindow::refreshProfilePlot()
{
    if (m_profileSnapshots.isEmpty()) {
        ui->plotProfile->clearAllGraphData();
        ui->plotProfile->setTitle(QStringLiteral("剖面分布 (无数据)"));
        ui->plotProfile->refreshPlot(false, true);
        return;
    }

    const int profileId = ui->spb_profileID->value();
    if (profileId < 0 || profileId >= m_profileSnapshots.size()) {
        ui->plotProfile->clearAllGraphData();
        ui->plotProfile->setTitle(QStringLiteral("剖面分布 #%1 (无数据)").arg(profileId));
        ui->plotProfile->refreshPlot(false, true);
        return;
    }

    const ProfileSnapshot &snapshot = m_profileSnapshots.at(profileId);
    QVector<double> verticalX(kVerticalCameraChannels);
    QVector<double> verticalY(kVerticalCameraChannels);
    QVector<double> horizontalX(kVerticalCameraChannels);
    QVector<double> horizontalY(kVerticalCameraChannels);

    for (int i = 0; i < kVerticalCameraChannels; ++i) {
        const double x = profilePointPosition(i);
        verticalX[i] = x;
        verticalY[i] = static_cast<double>(snapshot.counts.at(i));

        horizontalX[i] = x;
        horizontalY[i] = static_cast<double>(snapshot.counts.at(i + kVerticalCameraChannels));
    }

    ui->plotProfile->setGraphData(0, verticalX, verticalY);
    ui->plotProfile->setGraphData(1, horizontalX, horizontalY);
    ui->plotProfile->setTitle(QStringLiteral("剖面分布 #%1 t=%2ms")
                                  .arg(profileId)
                                  .arg(snapshot.timeMs));
    ui->plotProfile->setXRange(kProfileZMin, kProfileZMax);
    ui->plotProfile->refreshPlot(false, true);
}

void MainWindow::on_btn_generateProfile_clicked()
{
    generateProfileSnapshots();
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

    const bool hxrMode = ui->cmb_transferMode->currentIndex() == 1;
    const int displayBinCount = hxrMode
                                    ? qMin(hxrDisplayBinCount(), entry.counts.size())
                                    : entry.counts.size();
    if (displayBinCount <= 0) {
        ui->plotSpec->clearData();
        ui->plotSpec->refreshPlot();
        return;
    }

    ensureSpectrumBinAddresses(displayBinCount);
    if (hxrMode) {
        ui->plotSpec->setXRange(1, displayBinCount);
        ui->plotSpec->setData(m_spectrumBinAddresses,
                              entry.counts.mid(0, displayBinCount));
    } else {
        ui->plotSpec->setData(m_spectrumBinAddresses, entry.counts);
    }
    ui->plotSpec->refreshPlot();
}

void MainWindow::refreshWaveformPlot()
{
    const int channel = ui->spb_channel->value();
    const int waveId = ui->spb_waveID->value();
    if (channel < 1 || channel > m_waveformByChannel.size()) {
        ui->plotWave->clearData();
        ui->plotWave->refreshPlot();
        return;
    }

    const QVector<WaveformEntry> &waveforms = m_waveformByChannel.at(channel - 1);
    if (waveId < 0 || waveId >= waveforms.size()) {
        ui->plotWave->clearData();
        ui->plotWave->setTitle(QString("波形 CH%1 #%2 (无数据)").arg(channel).arg(waveId));
        ui->plotWave->refreshPlot();
        return;
    }

    const WaveformEntry &entry = waveforms.at(waveId);
    const QVector<quint16> &samples = entry.samples;
    const int n = samples.size();
    const int waveLenNs = ui->spb_waveLen->value();
    const int displayCount =
        qMin(n, static_cast<int>(waveLenNs / kWaveformSampleIntervalNs) + 1);
    QVector<double> xs(displayCount), ys(displayCount);
    for (int i = 0; i < displayCount; ++i) {
        xs[i] = static_cast<double>(i) * kWaveformSampleIntervalNs;
        ys[i] = static_cast<double>(samples.at(i));
    }

    ui->plotWave->setData(xs, ys);
    ui->plotWave->setXRange(0, waveLenNs);
    ui->plotWave->setTitle(QString("波形 Det%1 CH%2 #%3 t=%4ms")
                               .arg(entry.detectorIndex)
                               .arg(channel)
                               .arg(waveId)
                               .arg(entry.timeUnits));
    ui->plotWave->refreshPlot(false, true);
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

    if (ui->comboBox_2->currentIndex() == 1
        && m_autoMeasureState == AutoMeasureState::WaitingShot) {
        triggerAutoMeasureFromShot(shotNumber);
    }
}

bool MainWindow::buildDetParameter(DetParameter &detPara, Order::TriggerMode trigMode) const
{
    detPara = {};
    detPara.trigMode = trigMode;

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

    return true;
}

void MainWindow::triggerAutoMeasureFromShot(const QString &shotNumber)
{
    commandHelper->setShotNumber(shotNumber);
    m_autoMeasureDurationTimerStarted = false;

    clearWaveformData();
    clearSpectrumData();
    resetWaveformCounters();
    resetSpectrumSequenceTracking();
    spectrumPlotThrottle.invalidate();

    commandHelper->beginRecording(mdetPara);
    commandHelper->sendSpectrumControl(Order::HardwareTrigger);

    if (mdetPara.transferMode == Order::TransferMode::Spectrum16) {
        ui->plotSpec->setXRange(1, hxrDisplayBinCount());
    } else {
        ui->plotSpec->setXRange(1, 512);
    }

    waveformPlotTimer->start();

    m_autoMeasureState = AutoMeasureState::Measuring;
    ui->btn_stopMeasure->setEnabled(true);
    qInfo() << "炮号" << shotNumber << "触发自动测量，收到首个能谱后开始计时，时长:"
            << mdetPara.measureTime << "ms";
}

bool MainWindow::isMeasureSessionActive() const
{
    return measureTimer->isActive()
        || m_autoMeasureState != AutoMeasureState::Idle
        || isTaskRunning;
}

void MainWindow::stopAutoMeasureSession()
{
    const AutoMeasureState stateBeforeStop = m_autoMeasureState;
    if (stateBeforeStop == AutoMeasureState::Idle)
        return;

    // 先退出等待/测量状态，防止已排队的炮号消息在停止过程中再次启动测量。
    m_autoMeasureState = AutoMeasureState::Idle;

    if (isTaskRunning) {
        if (startTimer && startTimer->isActive())
            startTimer->stop();
        if (stopTimer && stopTimer->isActive())
            stopTimer->stop();
        isTaskRunning = false;
    }

    measureTimer->stop();
    waveformPlotTimer->stop();

    if (stateBeforeStop == AutoMeasureState::Measuring) {
        // 炮号已到，开始测量指令已经发出，此时必须向硬件发送停止指令。
        commandHelper->stopMeasure();
        printWaveformCollectionSummary();
        printSpectrumSequenceSummary();
    } else {
        // 炮号未到，硬件尚未开始测量，只取消等待，不发送停止指令。
        commandHelper->closeMeasurementFiles();
    }

    if (m_autoMeasureState != AutoMeasureState::Idle) {
        m_autoMeasureState = AutoMeasureState::Idle;
        m_autoMeasureDurationTimerStarted = false;
        ui->btn_startMeasure->setEnabled(true);
        ui->btn_stopMeasure->setEnabled(false);
        ui->comboBox_2->setEnabled(true);
        ui->dateTimeEdit->setEnabled(true);
        ui->dateTimeEdit_2->setEnabled(true);
    }
}

#include <QtConcurrent>
bool MainWindow::startMeasureInternal()
{
    const int measureMode = ui->comboBox_2->currentIndex();
    if (measureMode == 1 && m_autoMeasureState != AutoMeasureState::Idle)
        return true;

    clearWaveformData();
    clearSpectrumData();
    resetWaveformCounters();
    resetSpectrumSequenceTracking();
    spectrumPlotThrottle.invalidate();

    DetParameter detPara = {};
    const Order::TriggerMode trigMode = (measureMode == 1)
                                            ? Order::TriggerMode::HardwareTrigger
                                            : Order::TriggerMode::SoftwareTrigger;
    buildDetParameter(detPara, trigMode);

    const QString savePath = ui->le_savePath->text();
    if (savePath.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择数据保存路径"));
        return false;
    }

    QDir dir;
    if (!dir.exists(savePath) && !dir.mkpath(savePath)) {
        QMessageBox::information(this, tr("提示"), tr("请先选择数据保存路径"));
        return false;
    }

    mdetPara = detPara;

    const QString shotNumber = ui->lineEdit_shotID->text().trimmed();
    m_currentShotNumber = shotNumber;

    commandHelper->setSaveFileFormat(ui->cmb_saveFormat->currentIndex() == 0 ? Binary : Text);
    commandHelper->setSavePath(QDir::toNativeSeparators(QFileInfo(savePath).absoluteFilePath()));
    commandHelper->setShotNumber(shotNumber);

    // 自动测量
    if (measureMode == 1) {
        if (!commandHelper->configureMeasure(detPara))
            return false;

        if (detPara.transferMode == Order::TransferMode::Spectrum16) {
            ui->plotSpec->setXRange(1, hxrDisplayBinCount());
        } else {
            ui->plotSpec->setXRange(1, 512);
        }

        m_autoMeasureState = AutoMeasureState::WaitingShot;
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(true);
        ui->comboBox_2->setEnabled(false);
        ui->dateTimeEdit->setEnabled(false);
        ui->dateTimeEdit_2->setEnabled(false);
        qInfo() << "自动测量已就绪，等待炮号...";
        return true;
    }

    m_enableAutoMated = (measureMode == 2);
    if (m_enableAutoMated){
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

        ui->spb_measureTime->setValue(detPara.measureTime);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(true);
        ui->comboBox_2->setEnabled(false);
        ui->dateTimeEdit->setEnabled(false);
        ui->dateTimeEdit_2->setEnabled(false);

        // 4. 绑定信号槽：时间A到了启动任务
        connect(startTimer, &QTimer::timeout, this, [=]() {
            //qDebug() << "1111111111";
            ui->btn_startMeasure->setEnabled(false);
            ui->btn_stopMeasure->setEnabled(true);
            startTimer->stop();
            //qDebug() << "22222";
            machine->start();
            qInfo() << "测量已开始，炮号:" << shotNumber << "时长:" << detPara.measureTime << "ms";
        });

        // 5. 绑定信号槽：时间B到了停止任务
        connect(stopTimer, &QTimer::timeout, this, [&]() {
            isTaskRunning = false;
            m_enableAutoMated = false;
            machine->stop();

            if (startTimer->isActive())
                startTimer->stop();

            if (stopTimer->isActive())
                stopTimer->stop();

            if (measureTimer->isActive())
                measureTimer->stop();

            // 停止工作定
            waveformPlotTimer->stop();
            commandHelper->stopMeasure();
            printWaveformCollectionSummary();
            printSpectrumSequenceSummary();            

            m_autoMeasureState = AutoMeasureState::Idle;
            ui->btn_startMeasure->setEnabled(true);
            ui->btn_stopMeasure->setEnabled(false);
            ui->comboBox_2->setEnabled(true);
            ui->dateTimeEdit->setEnabled(true);
            ui->dateTimeEdit_2->setEnabled(true);

            qInfo() << "无人值守时间到，系统将自动退出";
            QTimer::singleShot(3000, this, [=]{
                this->close();
            });
        });

        startTimer->start(msecToA);
        stopTimer->start(msecToB);
        qInfo() << "程序已进入无人值守模式，超出用户自定义的监测参数范围，软件将自动切断前端硬件供电。";
    }
    else
    {
        // 手动测量
        waveformPlotTimer->start();

        if (detPara.transferMode == Order::TransferMode::Spectrum16) {
            ui->plotSpec->setXRange(1, hxrDisplayBinCount());
        } else {
            ui->plotSpec->setXRange(1, 512);
        }

        commandHelper->startMeasure(detPara);
        measureTimer->start(detPara.measureTime);
        ui->btn_startMeasure->setEnabled(false);
        ui->btn_stopMeasure->setEnabled(true);
        ui->comboBox_2->setEnabled(false);
        ui->dateTimeEdit->setEnabled(false);
        ui->dateTimeEdit_2->setEnabled(false);
        qInfo() << "测量已开始，炮号:" << shotNumber << "时长:" << detPara.measureTime << "ms";
    }

    return true;
}

// 开始测量
void MainWindow::on_btn_startMeasure_clicked()
{
    startMeasureInternal();
}

//连接采集系统（FPGA1/FPGA2）
void MainWindow::on_bt_connectDet_clicked()
{
    if (ui->bt_connectDet->isChecked()) {
        startUdpListening();
        commandHelper->connectDetector();
    } else {
        stopUdpListening();
        commandHelper->disconnectDetector();
    }
}

//断开采集系统功能已合并至 bt_connectDet 切换按钮
void MainWindow::on_bt_disconnectDet_clicked()
{
}

void MainWindow::on_btn_connectMonitor_clicked()
{
    if (ui->btn_connectMonitor->isChecked())
        commandHelper->connectARM();
    else
        commandHelper->disconnectARM();
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
    if (m_autoMeasureState == AutoMeasureState::WaitingShot) {
        stopAutoMeasureSession();
        qInfo() << "自动测量已取消，未收到炮号";
        return;
    }

    if (m_autoMeasureState == AutoMeasureState::Measuring) {
        stopAutoMeasureSession();
        qInfo() << "自动测量已手动停止，已发送硬件停止指令";
        return;
    }

    if (isTaskRunning){
        if (startTimer->isActive())
            startTimer->stop();

        if (stopTimer->isActive())
            stopTimer->stop();

        isTaskRunning = false;
    }

    m_enableAutoMated = false;
    machine->stop();
    ui->btn_startMeasure->setEnabled(true);
    ui->btn_stopMeasure->setEnabled(false);
    ui->comboBox_2->setEnabled(true);
    ui->dateTimeEdit->setEnabled(true);
    ui->dateTimeEdit_2->setEnabled(true);

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
    commandHelper->PowerOnRelay();
    showHardwareStartupWaitDialog();
}


void MainWindow::on_action_powerOff_triggered()
{
    commandHelper->PowerOffRelay();
}

void MainWindow::syncPowerSwitchFromRelay(bool powerOn)
{
    ui->switch_power->blockSignals(true);
    ui->switch_power->setChecked(powerOn);
    ui->switch_power->blockSignals(false);

    if (powerOn){
        QTimer::singleShot(500, this, [=]{
            // 延迟连接采集系统，否则连接可能会失败
            emit step3Finished();//
        });
    }
}

void MainWindow::setPowerSwitchEnabled(bool enabled)
{
    ui->switch_power->setEnabled(enabled);
}

void MainWindow::syncDetectorConnectButton()
{
    const bool anyConnected = detectOnline[0] || detectOnline[1] || detectOnline[2] || detectOnline[3];
    ui->bt_connectDet->blockSignals(true);
    ui->bt_connectDet->setChecked(anyConnected);
    ui->bt_connectDet->setText(anyConnected ? QStringLiteral("断开采集系统")
                                            : QStringLiteral("连接采集系统"));
    ui->bt_connectDet->blockSignals(false);

    if (anyConnected){
        QTimer::singleShot(500, this, [=]{
            // 延迟连接采集系统，否则连接可能会失败
            emit step4Finished();
        });
    }
}

void MainWindow::syncArmMonitorButton()
{
    const bool anyConnected = armSensorOnline[0] || armSensorOnline[1];
    ui->btn_connectMonitor->blockSignals(true);
    ui->btn_connectMonitor->setChecked(anyConnected);
    ui->btn_connectMonitor->setText(anyConnected ? QStringLiteral("断开实时监测系统")
                                                 : QStringLiteral("连接实时监测系统"));
    ui->btn_connectMonitor->blockSignals(false);

    if (anyConnected)
        emit step1Finished();
}

void MainWindow::loadMonitorAlarmSettings()
{
    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    ui->doubleSpinBox_temp->blockSignals(true);
    ui->doubleSpinBox_voltage->blockSignals(true);
    ui->doubleSpinBox_current->blockSignals(true);

    ui->doubleSpinBox_temp->setValue(
        settings->getValueByPath(QStringLiteral("Monitor/tempAlarmThreshold"), 65.0).toDouble());
    ui->doubleSpinBox_voltage->setValue(
        settings->getValueByPath(QStringLiteral("Monitor/voltageAlarmThreshold"), 5.02).toDouble());
    ui->doubleSpinBox_current->setValue(
        settings->getValueByPath(QStringLiteral("Monitor/currentAlarmThreshold"), 1.25).toDouble());

    ui->doubleSpinBox_temp->blockSignals(false);
    ui->doubleSpinBox_voltage->blockSignals(false);
    ui->doubleSpinBox_current->blockSignals(false);
}

void MainWindow::saveMonitorAlarmSettings()
{
    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    settings->setValueByPath(QStringLiteral("Monitor/tempAlarmThreshold"),
                             ui->doubleSpinBox_temp->value());
    settings->setValueByPath(QStringLiteral("Monitor/voltageAlarmThreshold"),
                             ui->doubleSpinBox_voltage->value());
    settings->setValueByPath(QStringLiteral("Monitor/currentAlarmThreshold"),
                             ui->doubleSpinBox_current->value());
    settings->save();
}

void MainWindow::loadMeasureSettings()
{
    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    ui->cmb_transferMode->blockSignals(true);
    ui->comboBox_2->blockSignals(true);
    ui->cmb_saveFormat->blockSignals(true);

    ui->cmb_transferMode->setCurrentIndex(
        settings->getValueByPath(QStringLiteral("Measure/transferMode"), 0).toInt());
    ui->spb_measureTime->setValue(
        settings->getValueByPath(QStringLiteral("Measure/measureTime"), 3000).toInt());
    ui->le_savePath->setText(
        settings->getValueByPath(QStringLiteral("Measure/savePath"), QStringLiteral("./波形测量"))
            .toString());
    ui->spb_specRefashTime->setValue(
        settings->getValueByPath(QStringLiteral("Measure/specRefreshTime"), 10).toInt());
    ui->cmb_saveFormat->setCurrentIndex(
        settings->getValueByPath(QStringLiteral("Measure/saveFormat"), 0).toInt());
    ui->comboBox_2->setCurrentIndex(
        settings->getValueByPath(QStringLiteral("Measure/measureMode"), 0).toInt());

    const QDateTime defaultAutoStart(QDate(2026, 3, 23), QTime(10, 0, 0));
    const QDateTime defaultAutoStop(QDate(2026, 3, 23), QTime(12, 0, 0));
    QDateTime autoStartTime = QDateTime::fromString(
        settings->getValueByPath(QStringLiteral("Measure/autoStartTime"), defaultAutoStart.toString(Qt::ISODate))
            .toString(),
        Qt::ISODate);
    QDateTime autoStopTime = QDateTime::fromString(
        settings->getValueByPath(QStringLiteral("Measure/autoStopTime"), defaultAutoStop.toString(Qt::ISODate))
            .toString(),
        Qt::ISODate);
    if (!autoStartTime.isValid())
        autoStartTime = defaultAutoStart;
    if (!autoStopTime.isValid())
        autoStopTime = defaultAutoStop;
    ui->dateTimeEdit->setDateTime(autoStartTime);
    ui->dateTimeEdit_2->setDateTime(autoStopTime);

    ui->cmb_transferMode->blockSignals(false);
    ui->comboBox_2->blockSignals(false);
    ui->cmb_saveFormat->blockSignals(false);

    updateSpectrumRefreshIntervalRange();
    updateHxrDisplayBinControls();
    updateProfileControls();
    updateUnattendedControls();
}

void MainWindow::saveMeasureSettings()
{
    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    settings->setValueByPath(QStringLiteral("Measure/transferMode"), ui->cmb_transferMode->currentIndex());
    settings->setValueByPath(QStringLiteral("Measure/measureTime"), ui->spb_measureTime->value());
    settings->setValueByPath(QStringLiteral("Measure/savePath"), ui->le_savePath->text().trimmed());
    settings->setValueByPath(QStringLiteral("Measure/specRefreshTime"), ui->spb_specRefashTime->value());
    settings->setValueByPath(QStringLiteral("Measure/saveFormat"), ui->cmb_saveFormat->currentIndex());
    settings->setValueByPath(QStringLiteral("Measure/measureMode"), ui->comboBox_2->currentIndex());
    settings->setValueByPath(QStringLiteral("Measure/autoStartTime"),
                             ui->dateTimeEdit->dateTime().toString(Qt::ISODate));
    settings->setValueByPath(QStringLiteral("Measure/autoStopTime"),
                             ui->dateTimeEdit_2->dateTime().toString(Qt::ISODate));
    settings->save();
}

void MainWindow::showHardwareStartupWaitDialog()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("提示"));
    box.setText(QStringLiteral("硬件启动中，请稍等。"));
    box.setStandardButtons(QMessageBox::NoButton);
    QTimer::singleShot(5000, &box, &QMessageBox::accept);
    box.exec();
}


void MainWindow::on_action_connectDet_triggered()
{
    startUdpListening();
    commandHelper->connectDetector();
}



void MainWindow::on_action_disconnectDet_triggered()
{
    stopUdpListening();
    commandHelper->disconnectDetector();
}


void MainWindow::on_action_startMeasure_triggered()
{
    startMeasureInternal();
}


void MainWindow::on_action_stopMeasure_triggered()
{
    on_btn_stopMeasure_clicked();
}


void MainWindow::on_comboBox_2_currentIndexChanged(int index)
{
    if (index == 2){
        // 无人值守
        ui->btn_startMeasure->setEnabled(true);
        ui->checkBox_alarm->setEnabled(false);
        ui->checkBox_alarm->setChecked(true);
        ui->dateTimeEdit->setEnabled(true);
        ui->dateTimeEdit_2->setEnabled(true);
    }
    else{
        ui->btn_startMeasure->setEnabled(ui->bt_connectDet->isChecked());
        ui->checkBox_alarm->setEnabled(true);
        ui->checkBox_alarm->setChecked(false);
        ui->dateTimeEdit->setEnabled(false);
        ui->dateTimeEdit_2->setEnabled(false);
    }
}

#include <QSignalTransition>
void MainWindow::initStateMachine()
{
    machine = new QStateMachine(this);

    // 初始化各个状态
    stIdle = new QState(machine);
    stStep1 = new QState(machine);
    stStep2 = new QState(machine);
    stStep3 = new QState(machine);
    stStep4 = new QState(machine);
    stFinish = new QState(machine);
    machine->setInitialState(stIdle);

    // 从空闲到Step1，启动后执行任务
    stIdle->addTransition(stIdle, &QState::entered, stStep1);

    stStep1->addTransition(this, &MainWindow::step1Finished, stStep2);
    connect(stStep1, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 开启电源
        if (!ui->btn_connectMonitor->isChecked()){
            ui->btn_connectMonitor->setChecked(true);
            emit ui->btn_connectMonitor->clicked(true);
        }
        else{
            emit step1Finished();
        }
    });

    // 后面两个步骤以此类推
    stStep2->addTransition(this, &MainWindow::step2Finished, stStep3);
    connect(stStep2, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 连接远程控制
        if (!ui->btn_relayNetOpen->isChecked()){
            ui->btn_relayNetOpen->setChecked(true);
            emit ui->btn_relayNetOpen->clicked(true);
        }
        else{
            emit step2Finished();
        }
    });

    stStep3->addTransition(this, &MainWindow::step3Finished, stStep4);
    connect(stStep3, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 开启电源
        if (!ui->switch_power->getChecked()){
            emit ui->switch_power->clicked(true);
        }
        else{
            emit step3Finished();
        }
    });

    stStep4->addTransition(this, &MainWindow::step4Finished, stFinish);
    connect(stStep4, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 连接采集系统
        if (!ui->bt_connectDet->isChecked()){
            ui->bt_connectDet->setChecked(true);
            emit ui->bt_connectDet->clicked(true);
        }
        else{
            emit step4Finished();
        }
    });

    connect(stFinish, &QState::entered, this, [=](){
        if (!m_enableAutoMated)
            return;

        // 启动工作定
        ui->btn_startMeasure->setEnabled(false);
        commandHelper->startMeasure(mdetPara);
        measureTimer->start(mdetPara.measureTime);
        waveformPlotTimer->start();
        qInfo() << "系统开机，测量已开始，炮号:" << m_currentShotNumber << "时长:" << mdetPara.measureTime << "ms";
    });
}
