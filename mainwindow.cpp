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
#include <QGraphicsProxyWidget>

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
    QList<QDoubleSpinBox*> spinBoxs;
    spinBoxs << ui->doubleSpinBox_dev1_temp_1 << ui->doubleSpinBox_dev1_temp_2 << ui->doubleSpinBox_dev1_temp_3;
    spinBoxs << ui->doubleSpinBox_dev1_voltage;
    spinBoxs << ui->doubleSpinBox_dev1_current;
    spinBoxs << ui->doubleSpinBox_dev2_temp_1 << ui->doubleSpinBox_dev2_temp_2 << ui->doubleSpinBox_dev2_temp_3;
    spinBoxs << ui->doubleSpinBox_dev2_temp_4 << ui->doubleSpinBox_dev2_temp_5 << ui->doubleSpinBox_dev2_temp_6;
    spinBoxs << ui->doubleSpinBox_dev2_voltage_1 << ui->doubleSpinBox_dev2_voltage_2 << ui->doubleSpinBox_dev2_voltage_3 << ui->doubleSpinBox_dev2_voltage_4;
    spinBoxs << ui->doubleSpinBox_dev2_current_1 << ui->doubleSpinBox_dev2_current_2 << ui->doubleSpinBox_dev2_current_3 << ui->doubleSpinBox_dev2_current_4;
    for (auto const& spinBox : spinBoxs){
        connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { saveMonitorAlarmSettings(); });
    }

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
    connect(this, SIGNAL(showProfileChart(const int&,const QVector<QVector<ChannelProfileEntry>>&)), this, SLOT(onShowProfileChart(const int&, const QVector<QVector<ChannelProfileEntry>> &)));
    qRegisterMetaType<QtMsgType>("QtMsgType");
    
    ui->plotWave->setTitle("波形");
    ui->plotWave->setXAxisLabel("时间(ns)");
    ui->plotWave->setYAxisLabel("幅值");

    ui->plotSpec->setTitle("分时能谱");
    ui->plotSpec->setXAxisLabel("能量(keV)");
    ui->plotSpec->setYAxisLabel("计数");
    ui->plotSpec->setXRange(1,512);

    ui->plotCps->setTitle("计数率");
    ui->plotCps->setXAxisLabel("时间(ms)");
    ui->plotCps->setYAxisLabel("计数");

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
    // 实时监测左侧栏
    {
        QPushButton* thresholdSettingButton = nullptr;
        thresholdSettingButton = new QPushButton();
        thresholdSettingButton->setText(tr("异常阈值设置"));
        thresholdSettingButton->setFixedSize(150,29);
        thresholdSettingButton->setCheckable(true);
        thresholdSettingButton->setChecked(true);

        QHBoxLayout* sideHboxLayout = new QHBoxLayout();
        sideHboxLayout->setObjectName("sideHboxLayout");
        sideHboxLayout->setContentsMargins(0,0,0,0);
        sideHboxLayout->setSpacing(2);

        QWidget* sideProxyWidget = new QWidget();
        sideProxyWidget->setObjectName("sideProxyWidget");
        sideProxyWidget->setLayout(sideHboxLayout);
        sideHboxLayout->addWidget(thresholdSettingButton);

        QGraphicsScene *scene = new QGraphicsScene(this);
        QGraphicsProxyWidget *w = scene->addWidget(sideProxyWidget);
        w->setPos(0,0);
        w->setRotation(-90);
        ui->graphicsView->setScene(scene);
        ui->graphicsView->setFrameStyle(0);
        ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        ui->graphicsView->setFixedSize(30, 150);
        ui->leftSidewidget->setFixedWidth(30);

        connect(thresholdSettingButton,&QPushButton::clicked,this,[=](){
            if(ui->leftStackedWidget->isHidden()) {
                ui->leftStackedWidget->show();

                thresholdSettingButton->setChecked(true);
            } else {
                if(ui->leftStackedWidget->currentWidget() == ui->detectorStatusWidget) {
                    ui->leftStackedWidget->hide();
                    thresholdSettingButton->setChecked(false);
                } else {
                    ui->leftStackedWidget->setCurrentWidget(ui->detectorStatusWidget);
                    thresholdSettingButton->setChecked(true);
                }
            }
        });

        connect(ui->toolButton_closeDetectorStatusWidget,&QPushButton::clicked,this,[=](){
            ui->leftStackedWidget->hide();
            thresholdSettingButton->setChecked(false);
        });
    }

    // 工作区
    {
        QSplitter *splitterH1 = new QSplitter(Qt::Horizontal,this);
        splitterH1->setHandleWidth(1);
        splitterH1->addWidget(ui->tabWidget_2);
        splitterH1->addWidget(ui->widget_right);
        splitterH1->setStretchFactor(0, 6);
        splitterH1->setStretchFactor(1, 1);
        //splitterH1->setSizes(QList<int>() << 1 << 1);
        ui->centralwidget->layout()->addWidget(splitterH1);
    }
    // 分隔栏
    {
        QSplitter *splitterV1 = new QSplitter(Qt::Vertical,this);
        splitterV1->setHandleWidth(1);
        splitterV1->addWidget(ui->plotWave);
        splitterV1->addWidget(ui->stackedWidget_spectrum);
        splitterV1->addWidget(ui->stackedWidget_3D);
        splitterV1->setStretchFactor(0, 10);
        splitterV1->setStretchFactor(1, 10);
        splitterV1->setStretchFactor(2, 25);
        ui->widget_2->layout()->addWidget(splitterV1);
        ui->widget_2->layout()->addWidget(ui->groupBox_pictureSetting);

        QSplitter *splitterH1 = new QSplitter(Qt::Horizontal,this);
        splitterH1->setHandleWidth(1);
        splitterH1->addWidget(ui->widget_hor);
        splitterH1->addWidget(ui->widget_ver);
        //splitterH1->setSizes(QList<int>() << 1 << 1);
        ui->page_3DSurface->layout()->addWidget(splitterH1);
    }

    connect(ui->plotSpec, &FixedDataPlotWidget::selectRangeChanged, this, [=](const QCPRange& range){
        ui->dsbx_energyLeft->setValue(range.lower);
        ui->dsbx_energyRight->setValue(range.upper);
    });//&MainWindow::onSelectSpectrumRange);
    commandHelper = CommandHelper::instance();
    DetectorSetting::reloadEnergyBoundaries();
    connect(commandHelper, &CommandHelper::sigAppendMsg, this, &MainWindow::slotAppendMsg);
    connect(commandHelper, &CommandHelper::sigRelayStatus, this, &MainWindow::onRelayStatusChanged);
    connect(commandHelper, &CommandHelper::sigRelayConnectError, this, [=](QAbstractSocket::SocketError) {
        // ui->action_relayNetOpen->blockSignals(true);
        // ui->action_relayNetOpen->setEnabled(false);
        // ui->action_relayNetOpen->setText(QStringLiteral("连接远程控制"));
        // ui->action_relayNetOpen->blockSignals(false);

        ui->action_relayNetOpen->setVisible(true);
        ui->action_relayNetClose->setVisible(false);
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
    connect(commandHelper, &CommandHelper::sigHardTriggeredSignalReceived, this,
            &MainWindow::onHardTriggeredSignalReceived, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigMeasureTimerStarted, this, [=]{
        if (m_measureMode == MeasureMode::ManualMode)
            startMeasureDurationTimer();
    }, Qt::QueuedConnection);

    m_spectrumByChannel.resize(kSpectrumChannelCount);
    m_spectrumSequenceNumbersByChannel.resize(kSpectrumChannelCount);
    m_missingSpectrumNumbersByChannel.resize(kSpectrumChannelCount);
    m_lastSpectrumSequenceByChannel.resize(kSpectrumChannelCount);
    m_hasSpectrumSequenceByChannel.resize(kSpectrumChannelCount);
    m_spectrumCountsByChannel.resize(kSpectrumChannelCount);
    m_waveformByChannel.resize(kSpectrumChannelCount);
    m_waveformSequenceNumbersByChannel.resize(kSpectrumChannelCount);
    m_missingWaveformNumbersByChannel.resize(kSpectrumChannelCount);
    m_lastWaveformSequenceByChannel.resize(kSpectrumChannelCount);
    m_hasWaveformSequenceByChannel.resize(kSpectrumChannelCount);

    m_plotRefreshTimer = new QTimer(this);
    m_plotRefreshTimer->setInterval(200); // 每200ms刷新一次波形
    connect(m_plotRefreshTimer, &QTimer::timeout, this, [=]{
        refreshSpectrumPlot();
        refreshWaveformPlot();
    });

    for (int i=0; i <32; ++i){
        if (i==7)
            ui->cbb_channel->addItem(QStringLiteral("水平CH") + QString::number(i%16 + 1) + QStringLiteral("(本底)"));
        else if (i==13)
            ui->cbb_channel->addItem(QStringLiteral("水平CH") + QString::number(i%16 + 1) + QStringLiteral("(Am-241)"));
        else if (i<16)
            ui->cbb_channel->addItem(QStringLiteral("水平CH") + QString::number(i%16 + 1));
        else if (i==23)
            ui->cbb_channel->addItem(QStringLiteral("垂直CH") + QString::number(i%16 + 1) + QStringLiteral("(本底)"));
        else if (i==29)
            ui->cbb_channel->addItem(QStringLiteral("垂直CH") + QString::number(i%16 + 1) + QStringLiteral("(Am-241)"));
        else
            ui->cbb_channel->addItem(QStringLiteral("垂直CH") + QString::number(i%16 + 1));
    }
    ui->cbb_channel->setCurrentIndex(10);
    connect(ui->cbb_channel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onChannelSpinBoxChanged);
    connect(ui->spb_specID, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshSpectrumPlot);
    connect(ui->spb_waveID, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshWaveformPlot);
    connect(ui->spb_waveLen, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshWaveformPlot);
    ui->spb_specID->setMinimum(1);
    ui->spb_waveID->setMinimum(1);
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
    connect(ui->comboBox_measureMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateUnattendedControls);
    connect(ui->spb_hxrDisplayBins, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::refreshSpectrumPlot);

    m_udpShotReceiver = new UdpShotReceiver(this);
    connect(m_udpShotReceiver, &UdpShotReceiver::datagramReceived,
            this, &MainWindow::onUdpDatagramReceived);
    connect(m_udpShotReceiver, &UdpShotReceiver::shotNumberChanged,
            this, &MainWindow::onUdpShotNumberChanged);
    connect(m_udpShotReceiver, &UdpShotReceiver::bindStateChanged,
            this, &MainWindow::onUdpBindStateChanged);

    // ui->action_relayNetOpen->setCheckable(true);
    // ui->action_relayNetOpen->setChecked(false);
    // ui->action_relayNetOpen->setText(QStringLiteral("连接远程控制"));
    ui->action_relayNetOpen->setVisible(true);
    ui->action_relayNetClose->setVisible(false);

    // ui->action_connectDet->setCheckable(true);
    // ui->action_connectDet->setChecked(false);
    // ui->action_connectDet->setText(QStringLiteral("连接采集系统"));
    ui->action_connectDet->setEnabled(false);
    ui->action_disconnectDet->setEnabled(false);

    ui->btn_startMeasure->addAction(ui->action_startMeasure);
    ui->btn_stopMeasure->addAction(ui->action_stopMeasure);
    connect(ui->action_startMeasure, &QAction::changed, this, [=](){
        ui->btn_startMeasure->setEnabled(ui->action_startMeasure->isEnabled());
    });
    connect(ui->action_stopMeasure, &QAction::changed, this, [=](){
        ui->btn_stopMeasure->setEnabled(ui->action_stopMeasure->isEnabled());
    });
    connect(ui->btn_startMeasure, &QPushButton::clicked, ui->action_startMeasure, &QAction::trigger);
    connect(ui->btn_stopMeasure, &QPushButton::clicked, ui->action_stopMeasure, &QAction::trigger);
    ui->action_startMeasure->setEnabled(false);
    ui->action_stopMeasure->setEnabled(false);

    // ui->action_connectMonitor->setCheckable(true);
    // ui->action_connectMonitor->setChecked(false);
    // ui->action_connectMonitor->setText(QStringLiteral("连接实时监测系统"));
    ui->action_connectMonitor->setEnabled(false);
    ui->action_disconnectMonitor->setEnabled(false);

    ui->action_powerOn->setText(QStringLiteral("远程上电"));
    ui->action_powerOff->setText(QStringLiteral("远程断电"));
    setPowerSwitchEnabled(false);

    loadMeasureSettings();
    m_currentShotNumber = ui->lineEdit_shotID->text().trimmed();
    emit ui->comboBox_measureMode->currentIndexChanged(ui->comboBox_measureMode->currentIndex());

    // 初始化状态机
    initStateMachine();

    // 开机自动最大化：一次性延迟调用，保留 lambda
    initStatusbar();

    m_hor3DSurface = init3DSurface(1, ui->widget_hor, QStringLiteral("水平-14通道时序计数率剖面图"));
    m_ver3DSurface = init3DSurface(2, ui->widget_ver, QStringLiteral("垂直-14通道时序计数率剖面图"));

    // 构造函数中添加
    m_logFlushTimer = new QTimer(this);
    m_logFlushTimer->start(10); // 每50ms刷新一次，每秒最多刷新20次
    connect(m_logFlushTimer, &QTimer::timeout, this, [this](){
        // 加锁取出所有缓存日志
        QMutexLocker lock(&m_bufferMutex);
        if (m_logBuffer.isEmpty()) return;

        // 一次性批量插入所有缓存日志
        QTextCursor cursor = ui->plainTextEdit_log->textCursor();
        cursor.beginEditBlock(); // 批量编辑，关闭中途重绘，关键优化！
        cursor.movePosition(QTextCursor::End);

        for (const auto& item : m_logBuffer) {
            QTextCharFormat format;
            format.setForeground(item.color);
            cursor.setCharFormat(format);
            cursor.insertText(item.text + "\n");
        }

        cursor.endEditBlock(); // 完成后一次性刷新UI
        ui->plainTextEdit_log->setTextCursor(cursor);
        // 自动滚动只做一次
        // ui->plainTextEdit_log->verticalScrollBar()->setValue(
        //     ui->plainTextEdit_log->verticalScrollBar()->maximum()
        //     );
        m_logBuffer.clear();
    });

    startUdpListening();
    // 开机自动最大化：一次性延迟调用，保留 lambda
    QTimer::singleShot(0, this, [this] { showMaximized(); });
}

MainWindow::~MainWindow()
{
    saveMonitorAlarmSettings();
    saveMeasureSettings();
    delete ui;
}

void MainWindow::initStatusbar()
{
    // 设置任务栏信息 - 系统时间
    QLabel *label_systemtime = new QLabel(ui->statusbar);
    label_systemtime->setObjectName("label_systemtime");
    label_systemtime->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ui->statusbar->setContentsMargins(5, 0, 5, 0);
    ui->statusbar->addWidget(nullptr, 1);
    ui->statusbar->addPermanentWidget(label_systemtime);

    QTimer* systemClockTimer = new QTimer(this);
    systemClockTimer->setObjectName("systemClockTimer");
    connect(systemClockTimer, &QTimer::timeout, this, &MainWindow::onSystemTimer);
    systemClockTimer->start(900);
}

// 系统刷新定时器，暂定于每秒钟刷新1次，刷新内容包括：测量时间、能谱图像、计数率曲线等
void MainWindow::onSystemTimer()
{
    // 获取当前时间
    QDateTime currentDateTime = QDateTime::currentDateTime();

    // 获取星期几的数字（1代表星期日，7代表星期日）
    int dayOfWeekNumber = currentDateTime.date().dayOfWeek();

    // 星期几的中文名称列表
    QStringList dayNames = {
        tr("星期日"), QObject::tr("星期一"), QObject::tr("星期二"), QObject::tr("星期三"), QObject::tr("星期四"), QObject::tr("星期五"), QObject::tr("星期六"), QObject::tr("星期日")
    };

    // 根据数字获取中文名称
    QString dayOfWeekString = dayNames.at(dayOfWeekNumber);
    this->findChild<QLabel*>("label_systemtime")->setText(QString(QObject::tr("系统时间：")) + currentDateTime.toString("yyyy/MM/dd hh:mm:ss ") + dayOfWeekString);

}

int MainWindow::measureDurationMs() const
{
    return qMax(1, ui->spb_measureTime->value());
}

void MainWindow::startMeasureDurationTimer()
{
    measureTimer->stop();
    const int durationMs = measureDurationMs();
    measureTimer->start(durationMs);
}

void MainWindow::onMeasureTimerTimeout()
{
    if (!commandHelper)
        return;

    if (m_measureMode == MeasureMode::AutoMode && m_autoMeasureState == AutoMeasureState::Measuring) {
        stopAutoMeasureSession();

        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(false);
        qInfo().nospace() << "炮号["<< m_currentShotNumber << "]自动测量时长已到，测量已停止，时长:" << measureDurationMs() << "ms";

        // 自动进入下一次自动测量阶段，直到手动点击停止按钮为止
        startMeasureInternal();
        updateMeasureParamsGroupEnabled();
        return;
    }

    if (m_measureMode == MeasureMode::AutoMatedMode && isTaskRunning && m_enableAutoMated
        && m_autoMeasureState == AutoMeasureState::Measuring) {
        m_plotRefreshTimer->stop();
        commandHelper->stopMeasure();
        printWaveformCollectionSummary();
        printSpectrumSequenceSummary();
        finalizeMeasurementPlots();
        resetMeasurementPlotData();

        qInfo().nospace() << "无人值守炮号[" << m_currentShotNumber
                          << "]测量时长已到，测量已停止，时长:" << measureDurationMs() << "ms";
        enterUnattendedWaitingShot();
        return;
    }

    m_plotRefreshTimer->stop();
    commandHelper->stopMeasure();
    printWaveformCollectionSummary();
    printSpectrumSequenceSummary();
    finalizeMeasurementPlots();

    ui->action_startMeasure->setEnabled(true);
    ui->action_stopMeasure->setEnabled(false);
    ui->comboBox_measureMode->setEnabled(true);
    ui->dateTimeEdit_startup->setEnabled(true);
    ui->dateTimeEdit_shutdown->setEnabled(true);
    updateMeasureParamsGroupEnabled();

    if (m_measureMode == MeasureMode::ManualMode)
        qInfo() << "手动测量时长已到，测量已停止，时长:" << measureDurationMs() << "ms";
    else if (m_measureMode == MeasureMode::AutoMatedMode) {
        emit ui->action_disconnectDet->trigger();
        emit ui->action_disconnectMonitor->trigger();
        emit ui->action_relayNetClose->trigger();
        qInfo() << "无人值守测量时长已到，测量已停止，时长:" << measureDurationMs() << "ms";
    }
}

void MainWindow::onRelayStatusChanged(bool on)
{
    // ui->action_relayNetOpen->blockSignals(true);
    // ui->action_relayNetOpen->setChecked(on);
    // ui->action_relayNetOpen->setEnabled(on);
    // ui->action_relayNetOpen->setText(on ? QStringLiteral("断开远程控制") : QStringLiteral("连接远程控制"));
    // ui->action_relayNetOpen->blockSignals(false);

    replayOnline = on;
    if (on){
        setPowerSwitchEnabled(true);

        qInfo() << "继电器网络状态: 已连接";

        if (m_enableAutoMated) {
            QTimer::singleShot(500, this, [this] {
                // 延迟进入下一步，否则指令会发出去无响应
                emit relayConnected();
            });
        }
    }
    else{
        // 关闭继电器自动重连
        commandHelper->disconnectARM();
        setPowerSwitchEnabled(false);
        syncPowerSwitchFromRelay(false);
        ui->action_connectDet->setEnabled(false);
        ui->action_disconnectDet->setEnabled(false);
        ui->action_connectMonitor->setEnabled(false);
        ui->action_disconnectMonitor->setEnabled(false);
        ui->action_startMeasure->setEnabled(m_measureMode==MeasureMode::AutoMatedMode);
        ui->action_stopMeasure->setEnabled(false);

        qInfo() << "继电器网络状态: 已断开";
    }
}

void MainWindow::onRelayPowerStatusChanged(bool on)
{
    syncPowerSwitchFromRelay(on);
    // ui->action_relayNetOpen->setEnabled(!on);
    // ui->action_relayNetClose->setEnabled(on);
    ui->action_powerOn->setVisible(!on);
    ui->action_powerOff->setVisible(on);

    if (on) {
        ui->action_connectDet->setEnabled(true);
        ui->action_disconnectDet->setEnabled(true);
        ui->action_connectMonitor->setEnabled(true);
        ui->action_disconnectMonitor->setEnabled(false);

        replayPowerOn = true;
        syncDetectorConnectButton();

        qInfo() << "继电器控制的电源状态: 已开启";
    } else {
        ui->action_connectDet->setEnabled(false);
        ui->action_disconnectDet->setEnabled(false);
        ui->action_connectMonitor->setEnabled(false);
        ui->action_disconnectMonitor->setEnabled(false);
        ui->action_startMeasure->setEnabled(m_measureMode==MeasureMode::AutoMatedMode);
        ui->action_stopMeasure->setEnabled(false);

        qInfo() << "继电器控制的电源状态: 已关闭";
        replayPowerOn = false;
        if (m_autoMeasureState == AutoMeasureState::Measuring) {
            measureTimer->stop();
            m_plotRefreshTimer->stop();
            commandHelper->stopMeasure();
            printWaveformCollectionSummary();
            printSpectrumSequenceSummary();
            finalizeMeasurementPlots();
            resetMeasurementPlotData();
            m_autoMeasureState = AutoMeasureState::Idle;
            m_autoMeasureDurationTimerStarted = false;
        } else if (m_autoMeasureState == AutoMeasureState::WaitingShot) {
            measureTimer->stop();
            m_plotRefreshTimer->stop();
            commandHelper->closeMeasurementFiles();
            m_autoMeasureState = AutoMeasureState::Idle;
            m_autoMeasureDurationTimerStarted = false;
        }
        updateMeasureParamsGroupEnabled();
        
        // 断电后清除各设备在线标记
        detectOnline[0] = false;
        detectOnline[1] = false;
        detectOnline[2] = false;
        detectOnline[3] = false;
        syncDetectorConnectButton();

        // 自动打开继电器电源开关
        //emit ui->action_powerOn->trigger();
    }
}

void MainWindow::onDetector1StatusChanged(bool on)
{
    if (on) {
        if (!isMeasureSessionActive())
        {
            ui->action_startMeasure->setEnabled(true);
            ui->action_stopMeasure->setEnabled(false);
        }

        qInfo() << "水平相机主网口(控制/能谱)状态: 已连接";
        detectOnline[0] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->action_startMeasure->setEnabled(m_measureMode==MeasureMode::AutoMatedMode);
            ui->action_stopMeasure->setEnabled(false);
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
            ui->action_startMeasure->setEnabled(true);
            ui->action_stopMeasure->setEnabled(false);
        }
        qInfo() << "垂直相机主网口(控制/能谱)状态: 已连接";
        detectOnline[1] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->action_startMeasure->setEnabled(m_measureMode==MeasureMode::AutoMatedMode);
            ui->action_stopMeasure->setEnabled(false);
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
            ui->action_startMeasure->setEnabled(true);
            ui->action_stopMeasure->setEnabled(false);
        }
        qInfo() << "水平相机副网口(波形接收)状态: 已连接";
        detectOnline[2] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->action_startMeasure->setEnabled(m_measureMode==MeasureMode::AutoMatedMode);
            ui->action_stopMeasure->setEnabled(false);
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
            ui->action_startMeasure->setEnabled(true);
            ui->action_stopMeasure->setEnabled(false);
        }
        qInfo() << "垂直相机副网口(波形接收)状态: 已连接";
        detectOnline[3] = true;
    } else {
        if (!isMeasureSessionActive()) {
            ui->action_startMeasure->setEnabled(m_measureMode==MeasureMode::AutoMatedMode);
            ui->action_stopMeasure->setEnabled(false);
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

    QVector<double> tempLimit;
    QVector<double>  voltageLimit;
    QVector<double>  currentLimit;
    if (armIndex == 1){
        tempLimit << ui->doubleSpinBox_dev1_temp_1->value() << ui->doubleSpinBox_dev1_temp_2->value() << ui->doubleSpinBox_dev1_temp_3->value();
        voltageLimit << ui->doubleSpinBox_dev1_voltage->value();
        currentLimit << ui->doubleSpinBox_dev1_current->value();
    }
    else{
        tempLimit << ui->doubleSpinBox_dev2_temp_1->value() << ui->doubleSpinBox_dev2_temp_2->value() << ui->doubleSpinBox_dev2_temp_3->value();
        tempLimit << ui->doubleSpinBox_dev2_temp_4->value() << ui->doubleSpinBox_dev2_temp_5->value() << ui->doubleSpinBox_dev2_temp_6->value();
        voltageLimit << ui->doubleSpinBox_dev2_voltage_1->value() << ui->doubleSpinBox_dev2_voltage_2->value() << ui->doubleSpinBox_dev2_voltage_3->value() << ui->doubleSpinBox_dev2_voltage_4->value();
        currentLimit << ui->doubleSpinBox_dev2_current_1->value() << ui->doubleSpinBox_dev2_current_2->value() << ui->doubleSpinBox_dev2_current_3->value() << ui->doubleSpinBox_dev2_current_4->value();
    }

    QStringList alarmDetails;
    for (int i = 0; i < temperature.size(); ++i) {
        if (temperature[i] > tempLimit[i]) {
            alarmDetails << tr("温度CH%1=%2°C>阈值%3°C")
                                .arg(i + 1)
                                .arg(temperature[i], 0, 'f', 1)
                                .arg(tempLimit[i], 0, 'f', 1);
        }
    }
    for (int i = 0; i < voltage.size(); ++i) {
        if (voltage[i] > voltageLimit[i]) {
            alarmDetails << tr("电压CH%1=%2V>阈值%3V")
                                .arg(i + 1)
                                .arg(voltage[i], 0, 'f', 3)
                                .arg(voltageLimit[i], 0, 'f', 3);
        }
    }
    for (int i = 0; i < current.size(); ++i) {
        if (current[i] > currentLimit[i]) {
            alarmDetails << tr("电流CH%1=%2A>阈值%3A")
                                .arg(i + 1)
                                .arg(current[i], 0, 'f', 3)
                                .arg(currentLimit[i], 0, 'f', 3);
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
    // if (m_autoMeasureState == AutoMeasureState::Measuring
    //     && ui->comboBox_measureMode->currentIndex() == 1
    //     && !m_autoMeasureDurationTimerStarted) {
    //     m_autoMeasureDurationTimerStarted = true;
    //     startMeasureDurationTimer();
    //     qInfo() << "收到首个能谱数据，开始测量倒计时:" << measureDurationMs() << "ms"; // 改为接收到硬件触发信号才开始计时onMeasureStarted
    // }

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

    const int currentChannel = ui->cbb_channel->currentIndex() + 1;
    if (logicalChannel != currentChannel)
        return;

    // 限流刷新，避免高频能谱拖慢 UI
    // if (spectrumPlotThrottle.isValid() && spectrumPlotThrottle.elapsed() < ui->spinBox_refreshTimeLength->value())
    //     return;

    // spectrumPlotThrottle.restart();
    // refreshSpectrumPlot();
}

void MainWindow::onWaveformDataReceived(int detectorIndex, int channelNumber, quint32 timeUnits,
                                        const QVector<quint16> &samples)
{
    const int logicalChannel = logicalChannelNumber(detectorIndex, channelNumber);
    if (logicalChannel < 1 || logicalChannel > m_waveformByChannel.size())
        return;

    const int channelIndex = logicalChannel - 1;
    appendWaveformData(detectorIndex, channelNumber, timeUnits, samples);

    const quint8 waveTimeIntervalMs = 10;
    const quint32 waveformSequence = timeUnits/waveTimeIntervalMs;
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

    const int currentChannel = ui->cbb_channel->currentIndex() + 1;
    if (logicalChannel != currentChannel)
        return;
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
    //QTextCharFormat format;
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz>>");
    QString logLine;

    if (msgType == QtWarningMsg) {
        //format.setForeground(Qt::blue);
        logLine = QStringLiteral("%1 [WARN] %2").arg(ts).arg(msg);
        appendColoredText(logLine, Qt::blue);
    } else if (msgType == QtCriticalMsg || msgType == QtFatalMsg) {
        //format.setForeground(Qt::red);
        logLine = QStringLiteral("%1 [ERROR] %2").arg(ts).arg(msg);
        appendColoredText(logLine, Qt::red);
    } else {
        // QtDebugMsg、QtInfoMsg、QtSystemMsg 等：不打印级别字样
        logLine = QStringLiteral("%1 %2").arg(ts).arg(msg);
        appendColoredText(logLine, Qt::black);
    }

    // QTextCursor cursor = ui->plainTextEdit_log->textCursor();
    // cursor.movePosition(QTextCursor::End);
    // cursor.insertText(logLine, format);
    // cursor.insertBlock();
    // ui->plainTextEdit_log->setTextCursor(cursor);
}

void MainWindow::appendColoredText(const QString &text, const QColor &color)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "appendColoredText", Qt::QueuedConnection,
                                  Q_ARG(QString, text), Q_ARG(QColor, color));
        return;
    }

    QMutexLocker lock(&m_bufferMutex);
    m_logBuffer.push_back({text, color});
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

        qWarning() << QString("通道%1缺失波形序号: %2")
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

        // 【新增连续序号合并逻辑】
        QList<quint32> lostSeqList;
        for (quint32 missingSequence : missingSequences)
            lostSeqList << missingSequence;

        QStringList missingLabels;
        int i = 0;
        int totalLost = lostSeqList.size();
        while (i < totalLost) {
            quint32 start = lostSeqList[i];
            quint32 end = start;
            // 向后查找连续的序号
            while (i+1 < totalLost && lostSeqList[i+1] == end + 1) {
                end++;
                i++;
            }
            // 单序号不合并，连续多号用~
            if (start == end) {
                missingLabels << QString::number(start);
            } else {
                missingLabels << QStringLiteral("%1~%2").arg(start).arg(end);
            }
            i++;
        }

        // QStringList missingLabels;
        // missingLabels.reserve(missingSequences.size());
        // for (quint32 missingSequence : missingSequences)
        //     missingLabels << QString::number(missingSequence);

        qWarning() << QString("通道%1缺失能谱序号: %2")
            .arg(channel)
            .arg(missingLabels.join(", "));
    }
}

void MainWindow::clearSpectrumData()
{
    // for (auto &channelSpectra : m_spectrumByChannel)
    //     channelSpectra.clear();
    // for (auto &channelSpectrumCounts : m_spectrumCountsByChannel)
    //     channelSpectrumCounts.clear();
    m_spectrumByChannel.clear();
    m_spectrumCountsByChannel.clear();

    ui->spb_specID->blockSignals(true);
    ui->spb_specID->setValue(1);
    ui->spb_specID->blockSignals(false);
    updateSpecIdSpinBoxRange();
    ui->plotSpec->clearData();
    ui->plotSpec->refreshPlot();
    ui->plotCps->clearData();
    ui->plotCps->refreshPlot();
}

void MainWindow::clearWaveformData()
{
    // for (auto &channelWaveforms : m_waveformByChannel)
    //     channelWaveforms.clear();
    m_waveformByChannel.clear();

    ui->spb_waveID->blockSignals(true);
    ui->spb_waveID->setValue(1);
    ui->spb_waveID->blockSignals(false);
    updateWaveIdSpinBoxRange();
    ui->plotWave->clearData();
    ui->plotWave->refreshPlot();
}

#ifdef QT_DATAVISUALIZATION_LIB
void MainWindow::clear3DSurface(CustomSurface *surface)
{
    if (!surface || surface->seriesList().isEmpty())
        return;

    surface->seriesList().first()->dataProxy()->resetArray(
        new QtDataVisualization::QSurfaceDataArray());
    surface->axisX()->setRange(0.0f, 10000.0f);
    surface->axisY()->setRange(0.0f, 65536.0f);
}
#endif

void MainWindow::resetMeasurementPlotData()
{
    clearSpectrumData();
    clearWaveformData();
    resetWaveformCounters();
    resetSpectrumSequenceTracking();
#ifdef QT_DATAVISUALIZATION_LIB
    clear3DSurface(m_hor3DSurface);
    clear3DSurface(m_ver3DSurface);
#endif
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
    //entry.counts = counts;
    std::copy(counts.cbegin(), counts.cend(), entry.counts);
    m_spectrumByChannel.append(storageChannel - 1, entry);

    SpectrumCountsEntry countsEntry;
    countsEntry.detectorIndex = detectorIndex;
    countsEntry.timeMs = timeMs;
    countsEntry.count = 0;
    for (const auto& count: counts)
        countsEntry.count += count;
    m_spectrumCountsByChannel.append(storageChannel - 1, countsEntry);
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
    //entry.samples = samples;
    std::copy(samples.cbegin(), samples.cend(), entry.samples);
    m_waveformByChannel.append(storageChannel - 1, entry);
}

void MainWindow::ensureSpectrumBinAddresses(int binCount)
{
    if (m_spectrumBinAddresses.size() == binCount)
        return;

    m_spectrumBinAddresses.resize(binCount);
    for (int i = 0; i < binCount; ++i){
        m_spectrumBinAddresses[i] = i + 1;                       
    }
}

void MainWindow::updateSpecIdSpinBoxRange()
{
    const int channel = ui->cbb_channel->currentIndex() + 1;
    const int specCount = (channel >= 1 && channel <= m_spectrumByChannel.size())
                              ? m_spectrumByChannel.size(channel - 1)
                              : 0;
    // 编号从 1 起：有 N 条数据时范围为 1..N，无数据时保持 1
    const int maxSpecId = qMax(1, specCount);

    ui->spb_specID->blockSignals(true);
    ui->spb_specID->setMaximum(maxSpecId);
    if (ui->spb_specID->value() > maxSpecId)
        ui->spb_specID->setValue(maxSpecId);
    ui->spb_specID->blockSignals(false);
}

void MainWindow::updateWaveIdSpinBoxRange()
{
    const int channel = ui->cbb_channel->currentIndex() + 1;
    const int waveCount = (channel >= 1 && channel <= m_waveformByChannel.size())
                              ? m_waveformByChannel.size(channel - 1)
                              : 0;
    // 编号从 1 起：有 N 条数据时范围为 1..N，无数据时保持 1
    const int maxWaveId = qMax(1, waveCount);

    ui->spb_waveID->blockSignals(true);
    ui->spb_waveID->setMaximum(maxWaveId);
    if (ui->spb_waveID->value() > maxWaveId)
        ui->spb_waveID->setValue(maxWaveId);
    ui->spb_waveID->blockSignals(false);
}

void MainWindow::syncSpectrumSpinBoxToLatest()
{
    const int channel = ui->cbb_channel->currentIndex() + 1;
    if (channel < 1 || channel > m_spectrumByChannel.size())
        return;

    const int maxSpecId = qMax(1, m_spectrumByChannel.size(channel - 1));
    ui->spb_specID->blockSignals(true);
    ui->spb_specID->setMaximum(maxSpecId);
    ui->spb_specID->setValue(maxSpecId);
    ui->spb_specID->blockSignals(false);
}

void MainWindow::syncWaveformSpinBoxToLatest()
{
    const int channel = ui->cbb_channel->currentIndex() + 1;
    if (channel < 1 || channel > m_waveformByChannel.size())
        return;

    const int maxWaveId = qMax(1, m_waveformByChannel.size(channel - 1));
    ui->spb_waveID->blockSignals(true);
    ui->spb_waveID->setMaximum(maxWaveId);
    ui->spb_waveID->setValue(maxWaveId);
    ui->spb_waveID->blockSignals(false);
}

void MainWindow::finalizeMeasurementPlots()
{
    syncSpectrumSpinBoxToLatest();
    syncWaveformSpinBoxToLatest();
    refreshSpectrumPlot();
    refreshWaveformPlot();
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
    return;
    const bool spectrum512Mode = ui->cmb_transferMode->currentIndex() == 0;
    ui->label_energyLeft->setVisible(spectrum512Mode);
    ui->dsbx_energyLeft->setVisible(spectrum512Mode);
    ui->label_energyRight->setVisible(spectrum512Mode);
    ui->dsbx_energyRight->setVisible(spectrum512Mode);
    ui->btn_generateProfile->setVisible(spectrum512Mode);
}

void MainWindow::updateUnattendedControls()
{
    m_measureMode = (MeasureMode)ui->comboBox_measureMode->currentIndex();
    const bool unattendedMode = m_measureMode == MeasureMode::AutoMatedMode;
    ui->label_9->setVisible(unattendedMode);
    ui->dateTimeEdit_startup->setVisible(unattendedMode);
    ui->label_10->setVisible(unattendedMode);
    ui->dateTimeEdit_shutdown->setVisible(unattendedMode);

    if (m_measureMode == MeasureMode::AutoMatedMode){
        // 无人值守        
        ui->action_startMeasure->setEnabled(true);
        // ui->checkBox_alarm->setEnabled(false);
        ui->checkBox_alarm->setChecked(true);
        ui->dateTimeEdit_startup->setEnabled(true);
        ui->dateTimeEdit_shutdown->setEnabled(true);
    }
    else{
        ui->lineEdit_shotID->setEnabled(true);
        ui->action_startMeasure->setEnabled(replayPowerOn ? !ui->action_connectDet->isEnabled() : false);
        ui->checkBox_alarm->setEnabled(true);
        ui->checkBox_alarm->setChecked(false);
        ui->dateTimeEdit_startup->setEnabled(false);
        ui->dateTimeEdit_shutdown->setEnabled(false);
    }
}

void MainWindow::updateMeasureParamsGroupEnabled()
{
    ui->groupBox_3->setEnabled(!isMeasureSessionActive());
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

void MainWindow::energyToBinRange512(double energyLeft, double energyRight,
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

void MainWindow::energyToBinRange16(double energyLeft, double energyRight,
                                     const EnergyCalibration &cal,
                                     int &binStart, int &binEnd) const
{
    const double eMin = qMin(energyLeft, energyRight);
    const double eMax = qMax(energyLeft, energyRight);

    double chLow = (eMin - cal.b) / cal.k;
    double chHigh = (eMax - cal.b) / cal.k;
    if (chLow > chHigh)
        std::swap(chLow, chHigh);

    binStart = qBound(1, static_cast<int>(std::floor(chLow)), kSpectrum16BinCount);
    binEnd = qBound(1, static_cast<int>(std::ceil(chHigh)), kSpectrum16BinCount);
    if (binStart > binEnd)
        std::swap(binStart, binEnd);
}

quint64 MainWindow::sumCountsInBinRange(const quint32* counts, qint32 size,//const QVector<quint32> &counts,
                                        int binStart, int binEnd) const
{
    if (size==0 || binStart < 1 || binEnd < binStart)
        return 0;

    const int startIndex = binStart - 1;
    const int endIndex = qMin(binEnd, size) - 1;
    if (startIndex >= size)
        return 0;

    quint64 total = 0;
    for (int i = startIndex; i <= endIndex; ++i)
        total += counts[i];
    return total;
}

void MainWindow::generateProfileSnapshots()
{
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
    for (int channelIdx=0; channelIdx<kSpectrumChannelCount; ++channelIdx){
        profileCount = qMax(profileCount, m_spectrumByChannel.size(channelIdx));
    }
    if (profileCount <= 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有能谱数据，无法生成剖面分布"));
        return;
    }

    //X轴采用能量重新进行刻度,
    // 对于512道，X轴应该采用能量刻度文件对道址进行换算
    // 对于16道，X轴应该采用HXR能量道文件输入能量点，采用右边界
    // QVector<quint32> spectrumEneryAddresses;
    // if (hxrMode) {
    //     const QVector<QVector<quint16>>& energyBoundaries = DetectorSetting::energyBoundaries();

    //     // 取出 energyBoundaries 中对应通道 specId 的右边界作为能量地址
    //     if (specId < energyBoundaries.size()) {
    //         const QVector<quint16> &boundaries = energyBoundaries.at(specId);
    //         for (const auto& addr : boundaries){
    //             spectrumEneryAddresses.push_back(addr);
    //         }
    //     }
    //     ui->plotSpec->setXRange(1, displayBinCount);
    //     ui->plotSpec->setData(spectrumEneryAddresses.mid(0, displayBinCount),
    //                           entry.counts.mid(0, displayBinCount));
    // } else {
    //     for (const auto& addr : m_spectrumBinAddresses){
    //         spectrumEneryAddresses.push_back(addr*commandHelper->m_channelEnergyCalib[channel-1].k_calib + commandHelper->m_channelEnergyCalib[channel-1].b_calib);
    //     }
    //     ui->plotSpec->setData(spectrumEneryAddresses, entry.counts);
    // }
    /////////////////////////////////////////////////////////////////////////////////////////
    QVector<QVector<ChannelProfileEntry>> dataHor;
    QVector<QVector<ChannelProfileEntry>> dataVer;
    dataHor.resize(16);
    dataVer.resize(16);

    const bool spectrum512Mode = mdetPara.transferMode == Order::TransferMode::Spectrum512;// ui->cmb_transferMode->currentIndex() == 0;
    if (spectrum512Mode){
        for (int ch = 0; ch < kProfileChannelCount; ++ch) {
            QVector<int> binStarts(kProfileChannelCount);
            QVector<int> binEnds(kProfileChannelCount);
            for (int ch = 0; ch < kProfileChannelCount; ++ch) {
                energyToBinRange512(energyLeft, energyRight, m_energyCalibration.at(ch),
                                    binStarts[ch], binEnds[ch]);
            }

            QVector<ChannelProfileEntry> entrys;
            entrys.resize(profileCount);
            for (int profileIndex = 0; profileIndex < profileCount; ++profileIndex) {
                const QVector<SpectrumEntry> &spectra = m_spectrumByChannel.at(ch);
                if (profileIndex >= m_spectrumByChannel.size(ch)) {
                    if (ch<16)
                        dataHor[ch%16] = entrys;
                    else
                        dataVer[ch%16] = entrys;
                    continue;
                }

                const SpectrumEntry &entry = spectra.at(profileIndex);
                entrys[profileIndex].timeMs = entry.timeMs;
                entrys[profileIndex].energy = sumCountsInBinRange(entry.counts, 512, binStarts.at(ch), binEnds.at(ch));
            }

            if (ch<16)
                dataHor[ch%16] = entrys;
            else
                dataVer[ch%16] = entrys;
        }
    }
    else{
        for (int ch = 0; ch < kProfileChannelCount; ++ch) {
            QVector<int> binStarts(kProfileChannelCount);
            QVector<int> binEnds(kProfileChannelCount);
            for (int ch = 0; ch < kProfileChannelCount; ++ch) {
                energyToBinRange16(energyLeft, energyRight, m_energyCalibration.at(ch),
                                    binStarts[ch], binEnds[ch]);
            }

            QVector<ChannelProfileEntry> entrys;
            entrys.resize(profileCount);
            for (int profileIndex = 0; profileIndex < profileCount; ++profileIndex) {
                const QVector<SpectrumEntry> &spectra = m_spectrumByChannel.at(ch);
                if (profileIndex >= m_spectrumByChannel.size(ch)) {
                    if (ch<16)
                        dataHor[ch%16] = entrys;
                    else
                        dataVer[ch%16] = entrys;
                    continue;
                }

                const SpectrumEntry &entry = spectra.at(profileIndex);
                entrys[profileIndex].timeMs = entry.timeMs;
                entrys[profileIndex].energy = sumCountsInBinRange(entry.counts, 16, binStarts.at(ch), binEnds.at(ch));
            }

            if (ch<16)
                dataHor[ch%16] = entrys;
            else
                dataVer[ch%16] = entrys;
        }
    }
    emit showProfileChart(1, dataHor);
    emit showProfileChart(2, dataVer);


    /////////////////////////////////////////////////////////////////////////////////////////
    // QVector<int> binStarts(kProfileChannelCount);
    // QVector<int> binEnds(kProfileChannelCount);
    // for (int ch = 0; ch < kProfileChannelCount; ++ch) {
    //     energyToBinRange512(energyLeft, energyRight, m_energyCalibration.at(ch),
    //                      binStarts[ch], binEnds[ch]);
    // }

    // for (int profileIndex = 0; profileIndex < profileCount; ++profileIndex) {
    //     ProfileSnapshot snapshot;
    //     snapshot.counts.resize(kProfileChannelCount);
    //     snapshot.timeMs = 0;

    //     for (int ch = 0; ch < kProfileChannelCount; ++ch) {
    //         const QVector<SpectrumEntry> &spectra = m_spectrumByChannel.at(ch);
    //         if (profileIndex >= spectra.size()) {
    //             snapshot.counts[ch] = 0;
    //             continue;
    //         }

    //         const SpectrumEntry &entry = spectra.at(profileIndex);
    //         if (snapshot.timeMs == 0)
    //             snapshot.timeMs = entry.timeMs;
    //         snapshot.counts[ch] = sumCountsInBinRange(entry.counts, binStarts.at(ch), binEnds.at(ch));
    //     }
    // }
}

void MainWindow::on_btn_generateProfile_clicked()
{
    generateProfileSnapshots();
}

void MainWindow::refreshSpectrumPlot()
{
    if (ui->radioButton_cps->isChecked()){
        refreshSpectrumCountsPlot();
        return;
    }

    if (isMeasureSessionActive())
        syncSpectrumSpinBoxToLatest();    

    const int channel = ui->cbb_channel->currentIndex() + 1;
    const int specId = ui->spb_specID->value(); // 1-based
    const int specIndex = specId - 1;
    if (channel < 1 || channel > m_spectrumByChannel.size()) {
        ui->plotSpec->clearData();
        ui->plotSpec->refreshPlot();
        return;
    }

    const QVector<SpectrumEntry> &spectra = m_spectrumByChannel.at(channel - 1);
    if (specIndex < 0 || specIndex >= m_spectrumByChannel.size(channel - 1)) {
        ui->plotSpec->clearData();
        ui->plotSpec->setTitle(QString("能谱 CH%1 #%2 (无数据)").arg(channel).arg(specId));
        ui->plotSpec->refreshPlot();
        return;
    }

    const SpectrumEntry &entry = spectra.at(specIndex);
    ui->plotSpec->setTitle(QString("能谱 %1 CH%2 #%3 t=%4ms")
                               .arg(entry.detectorIndex==1  ? QStringLiteral("水平") : QStringLiteral("垂直"))
                               .arg((channel-1) % 16 + 1)
                               .arg(specId)
                               .arg(entry.timeMs)
                               );

    const bool hxrMode = ui->cmb_transferMode->currentIndex() == 1;
    // const int displayBinCount = hxrMode
    //                                 ? qMin(hxrDisplayBinCount(), 16)
    //                                 : 512;
    const int displayBinCount = mdetPara.transferMode == Order::TransferMode::Spectrum16 ? qMin(hxrDisplayBinCount(), 16) : 512;
    if (displayBinCount <= 0) {
        ui->plotSpec->clearData();
        ui->plotSpec->refreshPlot();
        return;
    }

    ensureSpectrumBinAddresses(displayBinCount);

    //X轴采用能量重新进行刻度,
    // 对于512道，X轴应该采用能量刻度文件对道址进行换算
    // 对于16道，X轴应该采用HXR能量道文件输入能量点，采用右边界
    QVector<quint32> spectrumEneryAddresses;
    if (hxrMode) {
        const QVector<QVector<quint16>>& energyBoundaries = DetectorSetting::energyBoundaries();

        // 取出 energyBoundaries 中对应通道 specId 的右边界作为能量地址
        if (channel <= energyBoundaries.size()) {
            const QVector<quint16> &boundaries = energyBoundaries.at(channel-1);
            for (const auto& addr : boundaries){
                spectrumEneryAddresses.push_back(addr);
            }
        }
        ui->plotSpec->setXRange(1, displayBinCount);

        QVector<quint32> counts;
        counts.resize(displayBinCount);
        memcpy(counts.data(), entry.counts, sizeof(quint32) * displayBinCount);
        ui->plotSpec->setData(spectrumEneryAddresses.mid(1, displayBinCount),
                              counts);
    } else {
        for (const auto& addr : m_spectrumBinAddresses){
            spectrumEneryAddresses.push_back(addr*commandHelper->m_channelEnergyCalib[channel-1].k_calib + commandHelper->m_channelEnergyCalib[channel-1].b_calib);
        }

        QVector<quint32> counts;
        counts.resize(displayBinCount);
        memcpy(counts.data(), entry.counts, sizeof(quint32) * displayBinCount);
        ui->plotSpec->setData(spectrumEneryAddresses, counts);
    }
    ui->plotSpec->refreshPlot();
}

void MainWindow::refreshWaveformPlot()
{
    if (isMeasureSessionActive())
        syncWaveformSpinBoxToLatest();

    const int channel = ui->cbb_channel->currentIndex() + 1;
    const int waveId = ui->spb_waveID->value(); // 1-based
    const int waveIndex = waveId - 1;
    if (channel < 1 || channel > m_waveformByChannel.size()) {
        ui->plotWave->clearData();
        ui->plotWave->refreshPlot();
        return;
    }

    const QVector<WaveformEntry> &waveforms = m_waveformByChannel.at(channel - 1);
    if (waveIndex < 0 || waveIndex >= m_waveformByChannel.size(channel - 1)) {
        ui->plotWave->clearData();
        ui->plotWave->setTitle(QString("波形 CH%1 #%2 (无数据)").arg(channel).arg(waveId));
        ui->plotWave->refreshPlot();
        return;
    }

    const WaveformEntry &entry = waveforms.at(waveIndex);
    QVector<quint16> samples;
    samples.resize(sizeof(entry.samples)/sizeof(quint16));
    memcpy(samples.data(), entry.samples, sizeof(entry.samples));
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
    ui->plotWave->setTitle(QString("波形 %1 CH%2 #%3 t=%4ms")
                               .arg(entry.detectorIndex==1 ? QStringLiteral("水平") : QStringLiteral("垂直"))
                               .arg((channel-1) % 16 + 1)
                               .arg(waveId)
                               .arg(entry.timeUnits));
    ui->plotWave->refreshPlot(false, true);
}

void MainWindow::refreshSpectrumCountsPlot()
{
    const int channel = ui->cbb_channel->currentIndex() + 1;
    if (channel < 1 || channel > m_spectrumByChannel.size()) {
        ui->plotCps->clearData();
        ui->plotCps->refreshPlot();
        return;
    }

    const QVector<SpectrumCountsEntry> &entry = m_spectrumCountsByChannel.at(channel - 1);
    if (entry.size() <= 0)
        return;

    ui->plotCps->setTitle(QString("计数率 %1 CH%2")
                               .arg(entry[0].detectorIndex==1  ? QStringLiteral("水平") : QStringLiteral("垂直"))
                               .arg(channel)
                           );

    QVector<double> xs(entry.size()), ys(entry.size());
    for (int i = 0; i < m_spectrumCountsByChannel.size(channel - 1); ++i) {
        xs[i] = entry[i].timeMs;
        ys[i] = entry[i].count;
    }
    ui->plotCps->setData(xs, ys);
    ui->plotCps->refreshPlot(true, true);
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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return;

    const QString line = QStringLiteral("%1 %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-hh:mm:ss")),
                                  shotNumber);
    QByteArray payload = line.toUtf8();
    payload.append('\n');
    file.write(payload);
}

void MainWindow::onUdpShotNumberChanged(const QString &shotNumber)
{
    if (shotNumber == m_currentShotNumber)
        return;

    const QString info = tr("炮号已刷新：%1").arg(shotNumber);
    appendUdpLog(info);

    if (m_autoMeasureState != AutoMeasureState::WaitingShot)
        return;

    if (m_measureMode == MeasureMode::AutoMode) {
        qInfo() << info;

        m_currentShotNumber = shotNumber;
        ui->lineEdit_shotID->setText(shotNumber);
        saveShotNumberFile(shotNumber);
        triggerAutoMeasureFromShot(shotNumber);
    } else if (m_measureMode == MeasureMode::AutoMatedMode && m_enableAutoMated) {
        qInfo() << info;

        m_currentShotNumber = shotNumber;
        ui->lineEdit_shotID->setText(shotNumber);
        saveShotNumberFile(shotNumber);
        triggerUnattendedMeasureFromShot(shotNumber);
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

    // 接收炮号不清空图像，等到接收到硬触发信号之后再清空图像
    resetMeasurementPlotData();
    //spectrumPlotThrottle.invalidate();

    //commandHelper->beginRecording(mdetPara);// configureMeasure 中提前创建文件，避免漏掉反馈指令
    commandHelper->sendSpectrumControl(Order::HardwareTrigger);

    // if (mdetPara.transferMode == Order::TransferMode::Spectrum16) {
    //     ui->plotSpec->setXRange(1, hxrDisplayBinCount());
    // } else {
    //     ui->plotSpec->setXRange(1, 512);
    // }

    m_plotRefreshTimer->start(ui->spinBox_refreshTimeLength->value());

    m_autoMeasureState = AutoMeasureState::Measuring;
    ui->action_stopMeasure->setEnabled(true);
    updateMeasureParamsGroupEnabled();
    qInfo() << "炮号" << shotNumber << "触发自动测量，收到硬触发信号后开始计时，时长:"
            << measureDurationMs() << "ms";
}

void MainWindow::enterUnattendedWaitingShot()
{
    startUdpListening();

    mdetPara.trigMode = Order::TriggerMode::HardwareTrigger;
    m_autoMeasureDurationTimerStarted = false;

    if (!commandHelper->configureMeasure(mdetPara))
        return;

    if (mdetPara.transferMode == Order::TransferMode::Spectrum16) {
        ui->plotSpec->setXRange(1, hxrDisplayBinCount());
    } else {
        ui->plotSpec->setXRange(1, 512);
    }

    m_autoMeasureState = AutoMeasureState::WaitingShot;
    ui->action_startMeasure->setEnabled(false);
    ui->action_stopMeasure->setEnabled(true);
    updateMeasureParamsGroupEnabled();
    qInfo() << "无人值守已就绪，等待炮号...";
}

void MainWindow::triggerUnattendedMeasureFromShot(const QString &shotNumber)
{
    commandHelper->setShotNumber(shotNumber);
    m_autoMeasureDurationTimerStarted = false;

    resetMeasurementPlotData();
    //spectrumPlotThrottle.invalidate();

    mdetPara.trigMode = Order::TriggerMode::HardwareTrigger;
    commandHelper->startMeasure(mdetPara);

    if (mdetPara.transferMode == Order::TransferMode::Spectrum16) {
        ui->plotSpec->setXRange(1, hxrDisplayBinCount());
    } else {
        ui->plotSpec->setXRange(1, 512);
    }

    m_plotRefreshTimer->start(ui->spinBox_refreshTimeLength->value());

    m_autoMeasureState = AutoMeasureState::Measuring;
    ui->action_stopMeasure->setEnabled(true);
    updateMeasureParamsGroupEnabled();
    qInfo() << "炮号" << shotNumber << "触发无人值守测量，等待硬件触发，时长:"
            << measureDurationMs() << "ms";
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
    m_plotRefreshTimer->stop();

    if (stateBeforeStop == AutoMeasureState::Measuring) {
        // 炮号已到，开始测量指令已经发出，此时必须向硬件发送停止指令。
        commandHelper->stopMeasure();
        printWaveformCollectionSummary();
        printSpectrumSequenceSummary();
        finalizeMeasurementPlots();
        //resetMeasurementPlotData();
    } else {
        // 炮号未到，硬件尚未开始测量，只取消等待，不发送停止指令。
        commandHelper->closeMeasurementFiles();
    }

    if (m_autoMeasureState != AutoMeasureState::Idle) {
        m_autoMeasureState = AutoMeasureState::Idle;
        m_autoMeasureDurationTimerStarted = false;
        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(false);
        ui->comboBox_measureMode->setEnabled(true);
        ui->dateTimeEdit_startup->setEnabled(true);
        ui->dateTimeEdit_shutdown->setEnabled(true);
    }
}

#include <QtConcurrent>
bool MainWindow::startMeasureInternal()
{
    if ((m_measureMode == MeasureMode::AutoMode || m_measureMode == MeasureMode::AutoMatedMode) && m_autoMeasureState != AutoMeasureState::Idle)
        return true;

    DetParameter detPara = {};
    const Order::TriggerMode trigMode = (m_measureMode == MeasureMode::AutoMode || m_measureMode == MeasureMode::AutoMatedMode)
                                            ? Order::TriggerMode::HardwareTrigger
                                            : Order::TriggerMode::SoftwareTrigger;
    buildDetParameter(detPara, trigMode);

    if (Order::TriggerMode::SoftwareTrigger == trigMode){
        resetMeasurementPlotData();
        //spectrumPlotThrottle.invalidate();
    }

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

    // 无人值守：先按开机/关机时刻启动定时器，到点后再走状态机开机并等待炮号
    m_enableAutoMated = (m_measureMode == MeasureMode::AutoMatedMode);
    if (m_enableAutoMated) {
        if (startTimer == nullptr) {
            startTimer = new QTimer(this);
            startTimer->setSingleShot(true);
        }
        if (stopTimer == nullptr) {
            stopTimer = new QTimer(this);
            stopTimer->setSingleShot(true);
        }

        const QDateTime targetA = ui->dateTimeEdit_startup->dateTime();
        const int msecToA = QDateTime::currentDateTime().msecsTo(targetA);
        const QDateTime targetB = ui->dateTimeEdit_shutdown->dateTime();
        const int msecToB = QDateTime::currentDateTime().msecsTo(targetB);

        if (msecToA < 0 || msecToA > msecToB) {
            m_enableAutoMated = false;
            QMessageBox::information(this, tr("提示"), tr("时间范围设置不对！\n自动开机时刻必须大于系统当前时间，且关机时间也必须大于开机时间。"));
            return false;
        }

        isTaskRunning = true;
        ui->spb_measureTime->setValue(detPara.measureTime);
        ui->action_startMeasure->setEnabled(false);
        ui->action_stopMeasure->setEnabled(true);
        ui->comboBox_measureMode->setEnabled(false);
        ui->dateTimeEdit_startup->setEnabled(false);
        ui->dateTimeEdit_shutdown->setEnabled(false);

        disconnect(startTimer, nullptr, this, nullptr);
        connect(startTimer, &QTimer::timeout, this, [=]() {
            ui->action_startMeasure->setEnabled(false);
            ui->action_stopMeasure->setEnabled(true);
            startTimer->stop();
            machine->start();
            qInfo() << "无人值守开机时刻已到，自动开机流程已启动，炮号:" << shotNumber
                    << "计划测量时长:" << detPara.measureTime << "ms";
        });

        disconnect(stopTimer, nullptr, this, nullptr);
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

            // 关机时刻到达：无论正在测量还是等待炮号，都断开硬件连接
            emit ui->action_disconnectDet->trigger();
            emit ui->action_disconnectMonitor->trigger();
            emit ui->action_relayNetClose->trigger();

            m_plotRefreshTimer->stop();
            commandHelper->stopMeasure();
            printWaveformCollectionSummary();
            printSpectrumSequenceSummary();
            finalizeMeasurementPlots();

            m_autoMeasureState = AutoMeasureState::Idle;
            ui->action_startMeasure->setEnabled(true);
            ui->action_stopMeasure->setEnabled(false);
            ui->comboBox_measureMode->setEnabled(true);
            ui->dateTimeEdit_startup->setEnabled(true);
            ui->dateTimeEdit_shutdown->setEnabled(true);
            updateMeasureParamsGroupEnabled();

            qInfo() << "无人值守时间到，系统将自动退出";
            QTimer::singleShot(3000, this, [=]{
                this->close();
            });
        });

        startTimer->start(msecToA);
        stopTimer->start(msecToB);
        updateMeasureParamsGroupEnabled();
        qInfo() << "程序已进入无人值守模式，温度、电压、电流超出用户自定义的监测参数范围，软件将自动切断前端硬件供电。";
        return true;
    }

    // 自动测量：配置硬件后等待炮号
    if (m_measureMode == MeasureMode::AutoMode) {
        m_autoMeasureDurationTimerStarted = false;
        if (!commandHelper->configureMeasure(detPara))
            return false;

        if (detPara.transferMode == Order::TransferMode::Spectrum16) {
            ui->plotSpec->setXRange(1, hxrDisplayBinCount());
        } else {
            ui->plotSpec->setXRange(1, 512);
        }

        m_autoMeasureState = AutoMeasureState::WaitingShot;
        ui->action_startMeasure->setEnabled(false);
        ui->action_stopMeasure->setEnabled(true);
        ui->comboBox_measureMode->setEnabled(false);
        ui->dateTimeEdit_startup->setEnabled(false);
        ui->dateTimeEdit_shutdown->setEnabled(false);
        updateMeasureParamsGroupEnabled();
        qInfo() << "自动测量已就绪，等待炮号...";
        return true;
    }

    // 手动测量
    m_plotRefreshTimer->start(ui->spinBox_refreshTimeLength->value());

    if (detPara.transferMode == Order::TransferMode::Spectrum16) {
        ui->plotSpec->setXRange(1, hxrDisplayBinCount());
    } else {
        ui->plotSpec->setXRange(1, 512);
    }

    commandHelper->startMeasure(detPara);
    //startMeasureDurationTimer(); // 收到测试开始之后再计时
    ui->action_startMeasure->setEnabled(false);
    ui->action_stopMeasure->setEnabled(true);
    ui->comboBox_measureMode->setEnabled(false);
    ui->dateTimeEdit_startup->setEnabled(false);
    ui->dateTimeEdit_shutdown->setEnabled(false);
    updateMeasureParamsGroupEnabled();
    qInfo() << "测量已开始，炮号:" << shotNumber << "时长:" << measureDurationMs() << "ms";
    return true;
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
    if (replayPowerOn)
        emit ui->action_powerOff->triggered();
    commandHelper->disconnectRelay();
}


void MainWindow::on_action_powerOn_triggered()
{
    commandHelper->PowerOnRelay();
    showHardwareStartupWaitDialog();
    ui->action_powerOn->setVisible(false);
    ui->action_powerOff->setVisible(true);
}


void MainWindow::on_action_powerOff_triggered()
{
    commandHelper->PowerOffRelay();
    ui->action_powerOn->setVisible(true);
    ui->action_powerOff->setVisible(false);
}

void MainWindow::syncPowerSwitchFromRelay(bool powerOn)
{
    // ui->action_powerOn->blockSignals(true);
    // ui->action_powerOn->setEnabled(powerOn);
    // ui->action_powerOff->setEnabled(powerOn);
    // ui->action_powerOn->blockSignals(false);
    // ui->action_powerOn->setEnabled(!powerOn);
    // ui->action_powerOff->setEnabled(powerOn);

    if (powerOn && m_enableAutoMated) {
        QTimer::singleShot(500, this, [this] {
            emit relayPowerOpened();
        });
    }
}

void MainWindow::setPowerSwitchEnabled(bool enabled)
{
    ui->action_powerOn->setVisible(replayOnline ? !enabled : false);
    ui->action_powerOff->setVisible(enabled);

    ui->action_relayNetOpen->setVisible(!enabled);
    ui->action_relayNetClose->setVisible(enabled);
}

void MainWindow::syncDetectorConnectButton()
{
    const bool anyConnected = detectOnline[0] || detectOnline[1] || detectOnline[2] || detectOnline[3];
    const bool allConnected = detectOnline[0] && detectOnline[1] && detectOnline[2] && detectOnline[3];
    // ui->action_connectDet->blockSignals(true);
    // ui->action_connectDet->setChecked(anyConnected);
    // ui->action_connectDet->setText(anyConnected ? QStringLiteral("断开采集系统")
    //                                         : QStringLiteral("连接采集系统"));
    // ui->action_connectDet->blockSignals(false);
    ui->action_connectDet->setEnabled(!allConnected && replayPowerOn);
    ui->action_disconnectDet->setEnabled(anyConnected);

    ui->action_startMeasure->setEnabled(anyConnected || m_measureMode==MeasureMode::AutoMatedMode);
    ui->action_stopMeasure->setEnabled(anyConnected);

    if (allConnected && m_enableAutoMated) {
        QTimer::singleShot(500, this, [this] {
            emit detectorConnected();
        });
    }
}

void MainWindow::syncArmMonitorButton()
{
    const bool anyConnected = armSensorOnline[0] || armSensorOnline[1];
    const bool allConnected = armSensorOnline[0] && armSensorOnline[1];
    // ui->action_connectMonitor->blockSignals(true);
    // ui->action_connectMonitor->setChecked(anyConnected);
    // ui->action_connectMonitor->setText(anyConnected ? QStringLiteral("断开实时监测系统")
    //                                              : QStringLiteral("连接实时监测系统"));
    // ui->action_connectMonitor->blockSignals(false);
    ui->action_connectMonitor->setEnabled(!allConnected);
    ui->action_disconnectMonitor->setEnabled(anyConnected);

    if (!replayOnline) {
        ui->action_connectMonitor->setEnabled(false);
        ui->action_disconnectMonitor->setEnabled(false);
    }

    if (allConnected && m_enableAutoMated)
        emit monitorConnected();
}

void MainWindow::loadMonitorAlarmSettings()
{
    QList<QDoubleSpinBox*> spinBoxs;
    spinBoxs << ui->doubleSpinBox_dev1_temp_1 << ui->doubleSpinBox_dev1_temp_2 << ui->doubleSpinBox_dev1_temp_3;
    spinBoxs << ui->doubleSpinBox_dev1_voltage;
    spinBoxs << ui->doubleSpinBox_dev1_current;
    spinBoxs << ui->doubleSpinBox_dev2_temp_1 << ui->doubleSpinBox_dev2_temp_2 << ui->doubleSpinBox_dev2_temp_3;
    spinBoxs << ui->doubleSpinBox_dev2_temp_4 << ui->doubleSpinBox_dev2_temp_5 << ui->doubleSpinBox_dev2_temp_6;
    spinBoxs << ui->doubleSpinBox_dev2_voltage_1 << ui->doubleSpinBox_dev2_voltage_2 << ui->doubleSpinBox_dev2_voltage_3 << ui->doubleSpinBox_dev2_voltage_4;
    spinBoxs << ui->doubleSpinBox_dev2_current_1 << ui->doubleSpinBox_dev2_current_2 << ui->doubleSpinBox_dev2_current_3 << ui->doubleSpinBox_dev2_current_4;

    QList<QString> sectionNames;
    sectionNames << "dev1_tempAlarmThreshold1" << "dev1_tempAlarmThreshold2" << "dev1_tempAlarmThreshold3";
    sectionNames << "dev1_voltageAlarmThreshold";
    sectionNames << "dev1_currentAlarmThreshold";
    sectionNames << "dev2_tempAlarmThreshold1" << "dev2_tempAlarmThreshold2" << "dev2_tempAlarmThreshold3";
    sectionNames << "dev2_tempAlarmThreshold4" << "dev2_tempAlarmThreshold5" << "dev2_tempAlarmThreshold6";
    sectionNames << "dev2_voltageAlarmThreshold1" << "dev2_voltageAlarmThreshold2" << "dev2_voltageAlarmThreshold3" << "dev2_voltageAlarmThreshold4";
    sectionNames << "dev2_currentAlarmThreshold1" << "dev2_currentAlarmThreshold2" << "dev2_currentAlarmThreshold3" << "dev2_currentAlarmThreshold4";

    QVector<double> defaultValues;
    defaultValues << 32.50 << 32.50 << 32.50;
    defaultValues << 24.0;
    defaultValues << 1.0;
    defaultValues << 32.50 << 32.50 << 32.50;
    defaultValues << 32.50 << 32.50 << 32.50;
    defaultValues << 62.0 << 13.0 << 62.0 << 13.0;
    defaultValues << 0.01 << 0.05 << 0.01 << 0.05;

    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    int i = 0;
    for (auto const& section : sectionNames){
        spinBoxs[i]->blockSignals(true);
        double v = settings->getValueByPath(QStringLiteral("Monitor/") + section, defaultValues[i]).toDouble();
        spinBoxs[i]->setValue(v);
        spinBoxs[i]->blockSignals(false);
        ++i;
    }
}

void MainWindow::saveMonitorAlarmSettings()
{
    QList<QDoubleSpinBox*> spinBoxs;
    spinBoxs << ui->doubleSpinBox_dev1_temp_1 << ui->doubleSpinBox_dev1_temp_2 << ui->doubleSpinBox_dev1_temp_3;
    spinBoxs << ui->doubleSpinBox_dev1_voltage;
    spinBoxs << ui->doubleSpinBox_dev1_current;
    spinBoxs << ui->doubleSpinBox_dev2_temp_1 << ui->doubleSpinBox_dev2_temp_2 << ui->doubleSpinBox_dev2_temp_3;
    spinBoxs << ui->doubleSpinBox_dev2_temp_4 << ui->doubleSpinBox_dev2_temp_5 << ui->doubleSpinBox_dev2_temp_6;
    spinBoxs << ui->doubleSpinBox_dev2_voltage_1 << ui->doubleSpinBox_dev2_voltage_2 << ui->doubleSpinBox_dev2_voltage_3 << ui->doubleSpinBox_dev2_voltage_4;
    spinBoxs << ui->doubleSpinBox_dev2_current_1 << ui->doubleSpinBox_dev2_current_2 << ui->doubleSpinBox_dev2_current_3 << ui->doubleSpinBox_dev2_current_4;

    QList<QString> sectionNames;
    sectionNames << "dev1_tempAlarmThreshold1" << "dev1_tempAlarmThreshold2" << "dev1_tempAlarmThreshold3";
    sectionNames << "dev1_voltageAlarmThreshold";
    sectionNames << "dev1_currentAlarmThreshold";
    sectionNames << "dev2_tempAlarmThreshold1" << "dev2_tempAlarmThreshold2" << "dev2_tempAlarmThreshold3";
    sectionNames << "dev2_tempAlarmThreshold4" << "dev2_tempAlarmThreshold5" << "dev2_tempAlarmThreshold6";
    sectionNames << "dev2_voltageAlarmThreshold1" << "dev2_voltageAlarmThreshold2" << "dev2_voltageAlarmThreshold3" << "dev2_voltageAlarmThreshold4";
    sectionNames << "dev2_currentAlarmThreshold1" << "dev2_currentAlarmThreshold2" << "dev2_currentAlarmThreshold3" << "dev2_currentAlarmThreshold4";

    QVector<double> defaultValues;
    defaultValues << 32.50 << 32.50 << 32.50;
    defaultValues << 24.0;
    defaultValues << 1.0;
    defaultValues << 32.50 << 32.50 << 32.50;
    defaultValues << 32.50 << 32.50 << 32.50;
    defaultValues << 62.0 << 13.0 << 62.0 << 13.0;
    defaultValues << 0.01 << 0.05 << 0.01 << 0.05;

    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    int i = 0;
    for (auto const& section : sectionNames){
        spinBoxs[i]->blockSignals(true);
        settings->setValueByPath(QStringLiteral("Monitor/") + section, spinBoxs[i]->value());
        spinBoxs[i]->blockSignals(false);
        ++i;
    }
    settings->save();
}

void MainWindow::loadMeasureSettings()
{
    JsonSettings *settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    ui->cmb_transferMode->blockSignals(true);
    ui->comboBox_measureMode->blockSignals(true);
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
    ui->comboBox_measureMode->setCurrentIndex(
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
    ui->dateTimeEdit_startup->setDateTime(autoStartTime);
    ui->dateTimeEdit_shutdown->setDateTime(autoStopTime);

    ui->cmb_transferMode->blockSignals(false);
    ui->comboBox_measureMode->blockSignals(false);
    ui->cmb_saveFormat->blockSignals(false);

    updateSpectrumRefreshIntervalRange();
    updateHxrDisplayBinControls();
    updateProfileControls();
    updateUnattendedControls();
    refreshWaveformPlot();
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
    settings->setValueByPath(QStringLiteral("Measure/measureMode"), ui->comboBox_measureMode->currentIndex());
    settings->setValueByPath(QStringLiteral("Measure/autoStartTime"),
                             ui->dateTimeEdit_startup->dateTime().toString(Qt::ISODate));
    settings->setValueByPath(QStringLiteral("Measure/autoStopTime"),
                             ui->dateTimeEdit_shutdown->dateTime().toString(Qt::ISODate));
    settings->save();
}

void MainWindow::showHardwareStartupWaitDialog()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("提示"));
    box.setText(QStringLiteral("硬件启动中，请稍等..."));
    box.setStandardButtons(QMessageBox::NoButton);

    // 设置非模态对话框，避免界面堵塞
    box.setWindowFlags(box.windowFlags() | Qt::WindowStaysOnTopHint | Qt::Tool);
    box.setModal(true);

    // 窗口延迟关闭
    QTimer::singleShot(5000, &box, [&box](){
        if(!box.isHidden()) box.accept();
    });

    box.exec();
}


void MainWindow::on_action_connectDet_triggered()
{
    //startUdpListening();// 改为系统启动自动开启
    commandHelper->connectDetector();
}



void MainWindow::on_action_disconnectDet_triggered()
{
    //stopUdpListening();
    commandHelper->disconnectDetector();
}


void MainWindow::on_action_startMeasure_triggered()
{
    startMeasureInternal();
}

void MainWindow::on_action_stopMeasure_triggered()
{
    if (isTaskRunning){
        if (startTimer->isActive())
            startTimer->stop();

        if (stopTimer->isActive())
            stopTimer->stop();

        isTaskRunning = false;
    }

    m_enableAutoMated = false;
    machine->stop();

    if (m_autoMeasureState == AutoMeasureState::WaitingShot) {
        stopAutoMeasureSession();
        ui->comboBox_measureMode->setEnabled(true);
        ui->dateTimeEdit_startup->setEnabled(true);
        ui->dateTimeEdit_shutdown->setEnabled(true);
        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(false);
        updateMeasureParamsGroupEnabled();
        qInfo() << "自动测量已取消，未收到炮号";
        return;
    }

    if (m_autoMeasureState == AutoMeasureState::Measuring) {
        stopAutoMeasureSession();
        ui->comboBox_measureMode->setEnabled(true);
        ui->dateTimeEdit_startup->setEnabled(true);
        ui->dateTimeEdit_shutdown->setEnabled(true);
        ui->action_startMeasure->setEnabled(true);
        ui->action_stopMeasure->setEnabled(false);
        updateMeasureParamsGroupEnabled();
        qInfo() << "自动测量已手动停止，已发送硬件停止指令";
        return;
    }

    ui->action_startMeasure->setEnabled(true);
    ui->action_stopMeasure->setEnabled(false);
    ui->comboBox_measureMode->setEnabled(true);
    ui->dateTimeEdit_startup->setEnabled(true);
    ui->dateTimeEdit_shutdown->setEnabled(true);

    m_plotRefreshTimer->stop();
    commandHelper->stopMeasure();
    printWaveformCollectionSummary();
    printSpectrumSequenceSummary();
    measureTimer->stop();
    finalizeMeasurementPlots();
    updateMeasureParamsGroupEnabled();
    qInfo() << "手动停止测量";
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

    stStep1->addTransition(this, &MainWindow::relayConnected, stStep2);
    connect(stStep1, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 连接远程控制
        if (!replayOnline){
            emit ui->action_relayNetOpen->triggered(true);
        }
        else{
            emit relayConnected();
        }
    });

    // 后面两个步骤以此类推
    stStep2->addTransition(this, &MainWindow::relayPowerOpened, stStep3);
    connect(stStep2, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 开启电源
        if (!replayPowerOn){
            emit ui->action_powerOn->triggered(true);
        }
        else{
            emit relayPowerOpened();
        }

    });

    stStep3->addTransition(this, &MainWindow::detectorConnected, stStep4);
    connect(stStep3, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 连接采集系统
        const bool allConnected = detectOnline[0] && detectOnline[1] && detectOnline[2] && detectOnline[3];
        if (!allConnected){
            emit ui->action_connectDet->triggered(true);
        }
        else{
            emit detectorConnected();
        }
    });

    stStep4->addTransition(this, &MainWindow::monitorConnected, stFinish);
    connect(stStep4, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 连接实时监测系统
        if (ui->action_connectMonitor->isEnabled()){
            emit ui->action_connectMonitor->triggered(true);
        }
        else{
            emit monitorConnected();
        }
    });

    connect(stFinish, &QState::entered, this, [this](){
        if (!m_enableAutoMated)
            return;

        // 启动工作：先监听炮号，收到新炮号后再硬触发测量
        qInfo() << "系统开机完成，进入等待炮号阶段";
        enterUnattendedWaitingShot();
    });
}

void MainWindow::on_action_connectMonitor_triggered()
{
    m_armMonitorInAlarm[0] = false;
    m_armMonitorInAlarm[1] = false;
    commandHelper->connectARM();
}


void MainWindow::on_action_disconnectMonitor_triggered()
{
    commandHelper->disconnectARM();
}


void MainWindow::on_action_exit_triggered()
{
    this->close();
}


void MainWindow::on_action_about_triggered()
{
    QString filename = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    QMessageBox::about(this, tr("关于"),
                       QString("<p>") +
                           tr("版本") +
                           QString("</p><span style='color:blue;'>%1</span><p>").arg(filename, APP_VERSION) +
                           tr("提交") +
                           QString("</p><span style='color:blue;'>%1: %2</span><p>").arg(GIT_BRANCH, GIT_HASH) +
                           tr("日期") +
                           QString("</p><span style='color:blue;'>%1</span><p>").arg(GIT_DATE) +
                           tr("开发者") +
                           QString("</p><span style='color:blue;'>MaoXiaoqing</span><p>") +
                           "</p><p>版权所有 (C) 2026</p>"
                       );
}


void MainWindow::on_action_analyze_triggered()
{
    // 数据分析
}

void MainWindow::closeEvent(QCloseEvent *event) {

    //确认是否退出
    int ret = QMessageBox::question(this, tr("系统退出提示"), tr("确定要退出软件系统吗？"),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret == QMessageBox::No) {
        event->ignore();
        return;
    }

    // 关闭继电器，否则下次启动第一次容易连接不上
    commandHelper->PowerOffRelay();
    this->hide();
    event->accept();
    qApp->quit();
}

#include "signalwidthsetting.h"
void MainWindow::on_action_signalWidth_triggered()
{
    SignalWidthSetting dlg;
    dlg.exec();
}


void MainWindow::on_radioButton_spec_clicked()
{
    ui->stackedWidget_spectrum->setCurrentWidget(ui->page_spectrum);
    refreshSpectrumPlot();
}


void MainWindow::on_radioButton_cps_clicked()
{
    ui->stackedWidget_spectrum->setCurrentWidget(ui->page_cps);
    refreshSpectrumPlot();
}

void MainWindow::onHardTriggeredSignalReceived()
{
    // 硬触发模式：收到硬件触发后开始计时
    // qDebug() << "MainWindow 收到硬触发指令！";
    if (m_autoMeasureState == AutoMeasureState::Measuring
        && (m_measureMode == MeasureMode::AutoMode || m_measureMode == MeasureMode::AutoMatedMode)
        && !m_autoMeasureDurationTimerStarted) {
        m_autoMeasureDurationTimerStarted = true;
        //resetMeasurementPlotData();//炮号来的时候就清空，节省时间
        //spectrumPlotThrottle.invalidate();
        if (mdetPara.transferMode == Order::TransferMode::Spectrum16) {
            ui->plotSpec->setXRange(1, hxrDisplayBinCount());
        } else {
            ui->plotSpec->setXRange(1, 512);
        }
        startMeasureDurationTimer();
        qInfo() << "收到硬件触发指令，开始测量倒计时:" << measureDurationMs() << "ms";
    }
}

#include <QRandomGenerator>
CustomSurface* MainWindow::init3DSurface(const int& detectorIndex, QWidget* wigetContainer, const QString& title)
{
    const int channelCount = 14;
    const int timePoints = 101;
    const float timeMin = 0.0f, timeMax = 10000.0f;
    const float valMin = 0.0f, valMax = 65536.0f;

    // 在创建Q3DSurface实例前，先配置全局Surface格式
    QSurfaceFormat format;
    format.setSamples(8); // 开启8x MSAA多重采样抗锯齿
    format.setDepthBufferSize(24); // 提升深度缓冲位宽
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    // 1. 创建3D曲面视图
    CustomSurface *surface = new CustomSurface();
    surface->setShadowQuality(QtDataVisualization::QAbstract3DGraph::ShadowQualityNone);
    surface->setSelectionMode(QtDataVisualization::QAbstract3DGraph::SelectionNone);
    surface->setHorizontalAspectRatio(1.0f);// 把X/Z平面的纵深比例
    surface->setAspectRatio(1.5f);//全局Y轴高度 / X-Z平面纵深的整体比例
    // 禁用自动降采样优化，强制高质量渲染
    surface->setOptimizationHints(QAbstract3DGraph::OptimizationDefault);
    // 不要开启RenderToTexture模式，该模式会把整个图表渲染到低分辨率纹理，导致所有文本模糊
    // surface->setRenderingMode(QAbstract3DGraph::RenderToTexture); // 这行代码一定要删掉

    // 嵌入到传入的容器控件
    QWidget *container = QWidget::createWindowContainer(surface);
    //container->setMinimumSize(500, 500);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    container->setContentsMargins(0,0,0,0);

    // 创建全局标题Label
    auto *globalTitle = new QLabel(title, container);
    globalTitle->setAlignment(Qt::AlignCenter);
    globalTitle->setStyleSheet(R"(
        QLabel {
            color: black;
            font-size: 18px;
            font-weight: bold;
            background-color: white;
            padding: 4px 12px;
        }
    )");

    QVBoxLayout* vLayout = new QVBoxLayout(wigetContainer);
    vLayout->setContentsMargins(0,0,0,0);
    vLayout->setSpacing(0);
    vLayout->addWidget(container);
    vLayout->addWidget(globalTitle);
    wigetContainer->setLayout(vLayout);

    // 全局主题配置
    surface->activeTheme()->setGridEnabled(true);
    surface->activeTheme()->setLabelBorderEnabled(false);
    surface->activeTheme()->setLightStrength(1.0);
    surface->activeTheme()->setAmbientLightStrength(1.0);
    surface->activeTheme()->setHighlightLightStrength(1.0);
    surface->activeTheme()->setGridLineColor(QColor(60,60,60));

    // 2. 配置坐标轴 完全保留原有语义
    QtDataVisualization::QValue3DAxis *xAxis = new QtDataVisualization::QValue3DAxis();
    xAxis->setTitle("时间 (ms)");
    xAxis->setTitleVisible(true);
    xAxis->setRange(timeMin, timeMax);

    QtDataVisualization::QValue3DAxis *yAxis = new QtDataVisualization::QValue3DAxis();
    yAxis->setTitle("计数率");
    yAxis->setTitleVisible(true);
    yAxis->setRange(valMin, valMax);

    QtDataVisualization::QValue3DAxis *zAxis = new QtDataVisualization::QValue3DAxis();
    zAxis->setTitle("通道号");
    zAxis->setTitleVisible(true);
    zAxis->setRange(0, (channelCount+1)*1.0);
    zAxis->setLabelFormat("CH%.0f");
    zAxis->setSegmentCount(channelCount);
    zAxis->setFormatter(new QValue3DAxisFormatterX());

    surface->setAxisX(xAxis);
    surface->setAxisY(yAxis);
    surface->setAxisZ(zAxis);

    // 3. 生成标准连续剖面网格数据（核心方案3）
    QtDataVisualization::QSurfaceDataProxy *profileProxy = new QtDataVisualization::QSurfaceDataProxy();
    QtDataVisualization::QSurface3DSeries *profileSeries = new QtDataVisualization::QSurface3DSeries(profileProxy);
    profileSeries->setDrawMode(QtDataVisualization::QSurface3DSeries::DrawSurfaceAndWireframe);
    profileSeries->setMeshSmooth(true);
    profileSeries->setFlatShadingEnabled(false);

    // 复用你原有自定义蓝-绿-黄-橙-红全段渐变
    QLinearGradient gradient;
    gradient.setColorAt(0.0, Qt::darkBlue);
    gradient.setColorAt(0.1, QColor(4, 49, 255));
    gradient.setColorAt(0.2, QColor(1, 100, 253));
    gradient.setColorAt(0.3, QColor(4, 155, 252));
    gradient.setColorAt(0.4, QColor(4, 203, 255));
    gradient.setColorAt(0.5, QColor(3, 255, 2));
    gradient.setColorAt(0.6, QColor(254, 255, 2));
    gradient.setColorAt(0.7, QColor(254, 151, 4));
    gradient.setColorAt(0.8, QColor(255, 100, 2));
    gradient.setColorAt(0.9, QColor(255, 70, 2));
    gradient.setColorAt(1.0, QColor(255, 0, 0));

    profileSeries->setBaseGradient(gradient);
    profileSeries->setColorStyle(QtDataVisualization::Q3DTheme::ColorStyleRangeGradient);

    // 生成101行 × 16列的标准二维剖面网格
    QtDataVisualization::QSurfaceDataArray *fullProfileData = new QtDataVisualization::QSurfaceDataArray();
    fullProfileData->reserve(timePoints);

    // for (int chIdx = 0; chIdx < channelCount; ++chIdx)
    // {
    //     float fixedZ = chIdx * 1.0f; // Z轴直接用0~15的索引，完全落在Z轴range内，不会被裁剪
    //     QtDataVisualization::QSurfaceDataRow* timeRow = new QtDataVisualization::QSurfaceDataRow(timePoints);
    //     for (int tIdx = 0; tIdx < timePoints; ++tIdx)
    //     {
    //         float currentX = timeMin + tIdx * (timeMax - timeMin) / (timePoints - 1);
    //         float randomY = QRandomGenerator::global()->bounded((quint32)valMin, (quint32)valMax);
    //         (*timeRow)[tIdx] = QVector3D(currentX, randomY, fixedZ);
    //     }

    //     fullProfileData->append(timeRow);
    // }

    profileSeries->dataProxy()->resetArray(fullProfileData);
    surface->addSeries(profileSeries);

    // 自定义场景主题，提升纹理采样质量
    Q3DTheme *theme = surface->activeTheme();
    theme->setType(Q3DTheme::ThemeQt);
    // 关闭远距离自动淡化效果，避免画面整体发灰发虚
    theme->setBackgroundEnabled(true);

    // 4. 优化视角 适合剖面观测
    surface->scene()->activeCamera()->setCameraPosition(
        -60.0f,   // 方位角 调整为更易观察通道分布
        30.0f,    // 低仰角 突出剖面高度起伏
        125.0f    // 视距缩放范围 完整显示全部16通道范围
        );

    surface->show();

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]{

        const int channelCount = 16;
        const int timePoints = 101;
        const float timeMin = 0.0f, timeMax = 10000.0f;
        const float valMin = 0.0f, valMax = 65536.0f;

        QVector<QVector<ChannelProfileEntry>> data;
        data.resize(16);
        for (int chIdx = 0; chIdx < channelCount; ++chIdx)
        {
            QVector<ChannelProfileEntry> entrys;
            entrys.resize(timePoints);
            for (int tIdx = 0; tIdx < timePoints; ++tIdx)
            {
                float currentX = timeMin + tIdx * (timeMax - timeMin) / (timePoints - 1);
                float randomY = QRandomGenerator::global()->bounded((quint32)valMin, (quint32)valMax);

                entrys[tIdx].timeMs = currentX;
                entrys[tIdx].energy = randomY;
            }

            data[chIdx] = entrys;
        }

        emit showProfileChart(detectorIndex, data);
    });
    //timer->start(5000);

    return surface;
}

// void MainWindow::onSelectSpectrumRange(const QCPRange& range)
// {
//     ui->dsbx_energyLeft->setValue(range.minRange);
//     ui->dsbx_energyRight->setValue(range.maxRange);
// }

void MainWindow::onShowProfileChart(const int& detectorIndex, const QVector<QVector<ChannelProfileEntry>>& data)
{
    const int channelCount = 16;
    const int timePoints = data[0].size();
    float timeMin = 0.0f, timeMax = 0.0f;
    float valMin = 0.0f, valMax = 0.0f;

    QtDataVisualization::QSurfaceDataArray *fullProfileData = new QtDataVisualization::QSurfaceDataArray();
    fullProfileData->reserve(timePoints);

    int i = 0;
    for (int chIdx = 0; chIdx < channelCount; ++chIdx)
    {
        if (chIdx == 7 || chIdx == 13) // 通道8本地 通道14Am-241
            continue;

        auto &entrys = data[chIdx];//QVector<ChannelProfileEntry>
        float fixedZ = i * 1.0f; // Z轴直接用0~15的索引，完全落在Z轴range内，不会被裁剪
        QtDataVisualization::QSurfaceDataRow* timeRow = new QtDataVisualization::QSurfaceDataRow(timePoints);
        for (int tIdx = 0; tIdx < entrys.size(); ++tIdx)
        {
            float timeMs = entrys[tIdx].timeMs;
            float energy = entrys[tIdx].energy;
            timeMax = qMax(timeMax, timeMs);
            valMax = qMax(valMax, energy);
            (*timeRow)[tIdx] = QVector3D(timeMs, energy, fixedZ);
        }

        i++;
        fullProfileData->append(timeRow);
    }

    CustomSurface *surface = m_ver3DSurface;
    if (1== detectorIndex)
        surface = m_hor3DSurface;

    surface->axisX()->setRange(timeMin, timeMax);
    surface->axisY()->setRange(valMin, valMax*1.2);
    surface->seriesList().first()->dataProxy()->resetArray(fullProfileData);

    m_currentProfileData[detectorIndex-1] = data;
}

void MainWindow::on_btn_exportProfile_clicked()
{
    for (int i=0; i<=1; ++i){
        QString fileName = QString("%1/%2_%3_%4_%5_剖面数据.csv")
            .arg(commandHelper->mSavePath)
            .arg(i == 0 ? QStringLiteral("水平") : QStringLiteral("垂直"))
            .arg(commandHelper->mShotTag)
            .arg(commandHelper->mShotTimestamp)
            .arg(commandHelper->m_detPara.measureTime);

        const int channelCount = 16;
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            continue;

        QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        out.setCodec("UTF-8");
#endif

        // 取第一个通道的能谱时间戳
        const int timePoints = m_currentProfileData[i][0].size();
        QStringList lines;
        lines.append("channel");
        for (int k=1; k<=timePoints; ++k){
            auto &entrys = m_currentProfileData[i][0];
            float timeMs = entrys[k-1].timeMs;
            lines.append(QString::number(timeMs) + QStringLiteral("keV"));
        }
        out << lines.join(',') << Qt::endl;

        for (int chIdx = 0; chIdx < channelCount; ++chIdx)
        {
            lines.clear();
            lines.append(QString::number(chIdx+1));

            auto &entrys = m_currentProfileData[i][chIdx];
            for (int tIdx = 0; tIdx < entrys.size(); ++tIdx)
            {
                float energy = entrys[tIdx].energy;
                lines.append(QString::number(energy));
            }
            out << lines.join(',') << Qt::endl;
        }

        file.close();
    }

    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("数据导出完成！\n文件存储路径；") + commandHelper->mSavePath);
}


#include "hdadataupload.h"
void MainWindow::on_action_dataUpload_triggered()
{
    int ret = QMessageBox::question(this, tr("数据上传"), tr("确定要将数据上传到HDA服务器吗？"),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret == QMessageBox::No) {
        return;
    }

    HDADataUpload hdaClient;
    if (!hdaClient.connect()){
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("HDA服务器连接失败，数据无法上传！"));
        return;
    }

    int recordCount = 0;
    std::string shotTime = commandHelper->mShotTimestamp.toStdString();
    for (int channelIdx=0; channelIdx<kSpectrumChannelCount; ++channelIdx){
        const QVector<SpectrumCountsEntry> &entry = m_spectrumCountsByChannel.at(channelIdx);
        if (entry.size() > 0){
            std::vector<double> time/*时间ms*/;
            std::vector<double> values/*能谱计数率*/;
            for (int i = 0; i < m_spectrumCountsByChannel.size(channelIdx); ++i) {
                time.push_back(entry[i].timeMs);
                values.push_back(entry[i].count);
                recordCount++;
            }

            hdaClient.startUploadSpectrumCpsData(m_currentShotNumber.toInt(), shotTime, channelIdx+1, time, values);
        }
    }

    hdaClient.disconnect();
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("数据上传完毕，本次上传记录数共%1条！").arg(recordCount));
}


void MainWindow::on_action_parameterQuery_triggered()
{
    if (nullptr == parameterQueryDialog){
        parameterQueryDialog = new ParameterQueryDialog();
        parameterQueryDialog->setWindowFlags(parameterQueryDialog->windowFlags() | Qt::WindowStaysOnTopHint);
    }

    parameterQueryDialog->show();
}

