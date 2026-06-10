#include "otaupgradewindow.h"
#include "ui_otaupgradewindow.h"
#include <QToolButton>
#include <QFileDialog>
#include <QAction>
#include <QMessageBox>
#include <QButtonGroup>
#include <QDateTime>

#include <QFileInfo>

OTAUpgradeWindow::OTAUpgradeWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OTAUpgradeWindow)
{
    ui->setupUi(this);

    // 初始化进度条范围
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->pushButton_ok->setEnabled(true);

    QAction *action = ui->lineEdit_filePath->addAction(QIcon(":/resource/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button = qobject_cast<QToolButton*>(action->associatedWidgets().last());
    button->setCursor(QCursor(Qt::PointingHandCursor));
    connect(button, &QToolButton::pressed, this, [=](){
        QString filePath = QFileDialog::getOpenFileName(
            this,
            tr("选择更新文件"),
            QString(),
            tr("BIN 文件 (*.bin)"));
        if (filePath.isEmpty())
            return;

        ui->lineEdit_filePath->setText(filePath);
        emit sigWriteLog(QString(tr("更新文件：\"%1\"")).arg(filePath));
    });

    commHelper = CommandHelper::instance();
    // 探测器
    QButtonGroup *detectorGroup = new QButtonGroup(this);
    detectorGroup->addButton(ui->checkBox_det1, 1);
    detectorGroup->addButton(ui->checkBox_det2, 2);
    detectorGroup->setExclusive(true);

    connect(commHelper, &CommandHelper::sigDetector1Status, this, [=](bool on){
        ui->checkBox_det1->setEnabled(on);
    });
    connect(commHelper, &CommandHelper::sigDetector2Status, this, [=](bool on){
        ui->checkBox_det2->setEnabled(on);
    });
    connect(commHelper, &CommandHelper::sigOTAUpgradeData, this, [=](quint8 index, const QByteArray& data){
        if (data.at(0) == 0x86)
         {
            emit sigWriteLog(tr("旧程序擦除成功！"));
            m_waitLoop.quit();
        }
    });

    connect(this, SIGNAL(sigWriteLog(const QString&,QtMsgType)), this, SLOT(slotWriteLog(const QString&,QtMsgType)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigSendProgress(int,int,int)), this, SLOT(slotSendProgress(int,int,int)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigSendFinished(int,bool)), this, SLOT(slotSendFinished(int,bool)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigUpgradeFinished()), this, SLOT(slotUpgradeFinished()), Qt::QueuedConnection);
}

OTAUpgradeWindow::~OTAUpgradeWindow()
{
    // 清理任务监控器
    for (auto watcher : m_watchers) {
        if (watcher->isRunning()) {
            watcher->cancel();
            watcher->waitForFinished();
        }
        delete watcher;
    }

    delete ui;
}

#include <QScrollBar>
void OTAUpgradeWindow::slotWriteLog(const QString &msg, QtMsgType msgType)
{
    QPlainTextEdit* edit = ui->plainTextEdit_log;

    // 可选：减少刷新（高频日志建议保留）
    edit->setUpdatesEnabled(false);

    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::End);

    // ===== 定义颜色 =====
    QTextCharFormat fmtNormal;
    fmtNormal.setForeground(Qt::black);

    QTextCharFormat fmtError;
    fmtError.setForeground(Qt::red);

    QTextCharFormat fmtWarning;
    fmtWarning.setForeground(Qt::green);

    // ===== 时间 =====
    cursor.setCharFormat(fmtNormal);
    cursor.insertText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz >> "));

    // ===== 内容 =====
    if (msgType == QtCriticalMsg || msgType == QtFatalMsg) {
        cursor.setCharFormat(fmtError);
    }
    else if (msgType == QtWarningMsg) {
        cursor.setCharFormat(fmtWarning);
    }
    else {
        cursor.setCharFormat(fmtNormal);
    }

    cursor.insertText(msg);
    cursor.insertText("\n");

    edit->setTextCursor(cursor);

    // 自动滚动
    edit->verticalScrollBar()->setValue(edit->verticalScrollBar()->maximum());
    // 限制最大行数
    ui->plainTextEdit_log->document()->setMaximumBlockCount(2000);
    edit->setUpdatesEnabled(true);
}

void OTAUpgradeWindow::on_pushButton_ok_clicked()
{
    QVector<int> validIndexes;
    if (ui->checkBox_det1->isEnabled() && ui->checkBox_det1->isChecked())
        validIndexes.append(1);

    if (ui->checkBox_det2->isEnabled() && ui->checkBox_det2->isChecked())
        validIndexes.append(2);

    if (validIndexes.size() == 0) {
        // 没有选中任何行
        QMessageBox::warning(
            this,
            tr("OTA程序升级提示"),
            tr("请先选择一个探测器通道，再执行此操作。")
            );
        return;  // 或者给个提示
    }

    // 验证文件是否存在，未输入、不存在则提示
    m_binFileName = ui->lineEdit_filePath->text();
    if (m_binFileName.isEmpty()) {
        QMessageBox::warning(this, tr("OTA程序升级提示"), tr("请先选择更新文件！"));
        return;
    }
    if (!QFileInfo(m_binFileName).exists()) {
        QMessageBox::warning(this, tr("OTA程序升级提示"), tr("更新文件不存在！请重新选择文件。"));
        return;
    }

    // 检测所选 FPGA 主网口是否已连接（det1=FPGA1, det2=FPGA2）
    for (const auto index : validIndexes) {
        if (!commHelper->isDetectorConnected(index)) {
            const QString fpgaName = (index == 1) ? tr("FPGA1主网口") : tr("FPGA2主网口");
            QMessageBox::warning(this, tr("OTA程序升级提示"),
                                 tr("%1未连接！请先连接网络后再执行更新。").arg(fpgaName));
            return;
        }
    }

    ui->pushButton_ok->setEnabled(false);

    m_runningTasks = 0;
    m_taskQueue.clear();
    m_currentTaskIndex = 0;

    for (const auto index : validIndexes) {
        m_runningTasks++;
        // 将任务加入队列（每个设备对应一个任务）
        m_taskQueue.append(index);
    }

    // 2. 启动第一个任务（若队列非空）
    if (!m_taskQueue.isEmpty()) {
        startNextTask();
    } else {
        ui->pushButton_ok->setEnabled(true);
    }
}

void OTAUpgradeWindow::startNextTask()
{
    if (m_currentTaskIndex >= m_taskQueue.size()) {
        return;
    }

    const int index = m_taskQueue[m_currentTaskIndex];

    qDebug() << tr("=== 开始执行任务%1：探测器[#%2]（主分区程序） ===")
                .arg(m_currentTaskIndex + 1).arg(index);
    sigWriteLog(tr("=== 开始执行任务%1：探测器[#%2]（主分区程序） ===")
                    .arg(m_currentTaskIndex + 1).arg(index));

    // 1.1.1 发送更新数据包和更新地址（原业务逻辑保留）
    QByteArray sectorData = QByteArray::fromHex("12 34 00 0F D0 00 00 00 00 00 AB CD");
    commHelper->sendOTAUpgradeData(index, sectorData);

    // 1.1.2 计数块数
    qint64 fileSize = QFileInfo(m_binFileName).size();
    quint8 sectorCount = qCeil((double)fileSize / (64*1024)); // 计数块数

    // 2. 按照特定地址块进行Flash擦除指令
    //第4-5字节代表特定地址，6-7字节代表根据文件大小要进行擦除的块区
    sectorData = QByteArray::fromHex("55 04 01 00 00 00 00 f0");
    sectorData[5] = (sectorCount >> 8) & 0xFF;
    sectorData[6] = sectorCount & 0xFF;
    commHelper->sendOTAUpgradeData(index, sectorData);

    // 堵塞等待86字节
    m_waitLoop.exec();

    // 3. 按照特定地址进行Flash写入指令
    sectorData[1] = 0x05;
    sectorData[5] = 0x00;
    sectorData[6] = 0x00;
    if (!commHelper->sendOTAUpgradeData(index, sectorData)) {
        qCritical() << tr("探测器[#%1]发送更新地址失败，跳过该任务").arg(index);
        sigWriteLog(tr("探测器[#%1]发送更新地址失败，跳过该任务").arg(index), QtCriticalMsg);
        m_currentTaskIndex++;
        startNextTask();
        return;
    }
    QThread::msleep(1000); // 子线程休眠，不阻塞主线程

    // 4. 启动当前任务的异步发送（子线程执行）
    QFuture<void> future = QtConcurrent::run(this, &OTAUpgradeWindow::asyncSendOTAData, index);

    // 监控当前任务完成，自动启动下一个任务
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, [this, watcher]() {
        m_currentTaskIndex++;
        startNextTask(); // 任务完成后启动下一个
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void OTAUpgradeWindow::on_pushButton_exit_clicked()
{
    for (int index = 0; index <=3; ++index)
        commHelper->endOTAUpgrade(index);

    ui->pushButton_ok->setEnabled(true);
    this->close();
}


void OTAUpgradeWindow::asyncSendOTAData(int index)
{
    QFile file(m_binFileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit sigSendFinished(index, false);
        return;
    }

    qint64 fileSize = QFileInfo(m_binFileName).size();
    int sendCount = qCeil((double)fileSize / 256);
    bool sendSuccess = true;
    commHelper->startOTAUpgrade(index);
    for (int i = 0; i < sendCount && sendSuccess; ++i) {
        emit sigSendProgress(index, i + 1, sendCount);

        QByteArray buf = file.read(256);
        if (buf.size() < 256 && i != sendCount - 1) {
            sendSuccess = false;
            break;
        }

        if (!commHelper->sendOTAUpgradeData(index, buf)) {
            sendSuccess = false;
            break;
        }

        QThread::msleep(1); // 子线程休眠，不阻塞主线程
    }

    file.close();
    if (!sendSuccess) {
        emit sigSendFinished(index, sendSuccess);
    } else {
        std::thread([=](){
            emit sigSendFinished(index, sendSuccess);
        }).join();
    }

    commHelper->endOTAUpgrade(index); // 发送结束指令
}

void OTAUpgradeWindow::slotSendProgress(int /*index*/, int current, int total)
{
    int progress = (current * 100) / total;
    ui->progressBar->setValue(progress);
}

void OTAUpgradeWindow::slotSendFinished(int index, bool success)
{
    m_runningTasks--;

    if (success) {
        QByteArray finishData = QByteArray::fromHex("55 06 01 00 00 00 00 f0");
        if (commHelper->sendOTAUpgradeData(index, finishData)) {
            qDebug() << tr("探测器[#%1][主分区程序]数据发送完毕！").arg(index);
            emit sigWriteLog(tr("探测器[#%1][主分区程序]数据发送完毕！").arg(index));
        } else {
            qCritical() << tr("探测器[#%1][主分区程序]发送完成指令失败！").arg(index);
            sigWriteLog(tr("探测器[#%1][主分区程序]发送完成指令失败！").arg(index), QtCriticalMsg);
        }
    } else {
        qCritical() << tr("探测器[#%1][主分区程序]更新失败！").arg(index);
        sigWriteLog(tr("探测器[#%1][主分区程序]更新失败！").arg(index), QtCriticalMsg);
    }

    // 按钮状态
    if (m_runningTasks == 0) {
        emit sigUpgradeFinished();
    }
}

void OTAUpgradeWindow::slotUpgradeFinished()
{
    // 所有任务完成
    qDebug() << "=== 所有OTA任务执行完毕 ===";
    sigWriteLog("=== 所有OTA任务执行完毕 ===", QtInfoMsg);
    ui->pushButton_ok->setEnabled(true);
    ui->progressBar->setValue(0);
}
