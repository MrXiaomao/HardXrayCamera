#include "otaupgradewindow.h"
#include "ui_otaupgradewindow.h"
#include <QToolButton>
#include <QFileDialog>
#include <QAction>
#include <QMessageBox>
#include <QDateTime>

// 枚举目录下的 .rpd 文件并存储到 QMap
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QString>

// 枚举目录下符合 "数字_xxx.rpd" 规则的文件并存储到 QMap<int, QString>
QMap<int, QString> enumRpdFilesByNumber(const QString& dirPath) {
    QMap<int, QString> rpdMap;
    QDir dir(dirPath);

    // 筛选 .rpd 文件（忽略大小写）
    QStringList filters;
    filters << "*.rpd" << "*.RPD";
    dir.setNameFilters(filters);

    // 正则表达式：匹配 "数字_xxx.rpd"（数字部分捕获为分组1）
    QRegularExpression regex(R"(^(\d+)_.*\.rpd$)", QRegularExpression::CaseInsensitiveOption);

    // 遍历文件信息列表
    QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileList) {
        QString fileName = fileInfo.fileName();
        QRegularExpressionMatch match = regex.match(fileName);

        // 若匹配成功，提取数字键并存储
        if (match.hasMatch()) {
            int key = match.captured(1).toInt(); // 提取分组1的数字
            rpdMap[key] = fileInfo.absoluteFilePath();
        }
    }

    return rpdMap;
}


OTAUpgradeWindow::OTAUpgradeWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OTAUpgradeWindow)
{
    ui->setupUi(this);

    // 初始化进度条范围
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->pushButton_ok->setEnabled(false);

    QAction *action = ui->lineEdit_filePath->addAction(QIcon(":/open.png"), QLineEdit::TrailingPosition);
    QToolButton* button = qobject_cast<QToolButton*>(action->associatedWidgets().last());
    button->setCursor(QCursor(Qt::PointingHandCursor));
    connect(button, &QToolButton::pressed, this, [=](){
        QString dirPath = QFileDialog::getExistingDirectory(this);
        if (dirPath.isEmpty())
            return;

        ui->lineEdit_filePath->setText(dirPath);
        ui->pushButton_ok->setEnabled(true);
        ui->pushButton_switch->setEnabled(true);

        QMap<int, QString> rpdFiles = enumRpdFilesByNumber(dirPath);
        // 输出结果（按数字键升序排列）
        for (auto it = rpdFiles.begin(); it != rpdFiles.end(); ++it) {
            emit sigWriteLog(QString(tr("探测器[#%1] 更新文件名称\"%2\"")).arg(it.key()).arg(it.value()));
        }
    });

    commHelper = CommandHelper::instance();
    // 探测器
    connect(commHelper, &CommandHelper::sigDetector1Status, this, [=](bool on){
        ui->checkBox_det1->setEnabled(on);
    });
    connect(commHelper, &CommandHelper::sigDetector2Status, this, [=](bool on){
        ui->checkBox_det2->setEnabled(on);
    });

    connect(this, SIGNAL(sigWriteLog(const QString&,QtMsgType)), this, SLOT(slotWriteLog(const QString&,QtMsgType)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigSendProgress(int,const QString&,int,int)), this, SLOT(slotSendProgress(int,const QString&,int,int)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigSendFinished(int,const QString&,bool)), this, SLOT(slotSendFinished(int,const QString&,bool)), Qt::QueuedConnection);
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
    fmtNormal.setForeground(Qt::white);

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

    if (ui->checkBox_det3->isEnabled() && ui->checkBox_det3->isChecked())
        validIndexes.append(3);

    if (ui->checkBox_det4->isEnabled() && ui->checkBox_det4->isChecked())
        validIndexes.append(4);

    if (validIndexes.size() == 0) {
        // 没有选中任何行
        QMessageBox::warning(
            this,
            tr("自定义通道测量——提示"),
            tr("请先选择一个探测器通道，再执行此操作。")
            );
        return;  // 或者给个提示
    }

    QString selectedPartition;
    if (ui->comboBox_partition->currentIndex() == 0){
        selectedPartition = QStringLiteral("主分区程序");
    } else {
        selectedPartition = QStringLiteral("备用分区程序");
    }

    ui->pushButton_ok->setEnabled(false);
    ui->pushButton_switch->setEnabled(false);
    QString dirPath = ui->lineEdit_filePath->text();
    m_rpdFiles = enumRpdFilesByNumber(dirPath);

    m_runningTasks = 0;
    m_taskQueue.clear();
    m_currentTaskIndex = 0;

    for (const auto index : validIndexes) {
        m_runningTasks++;
        // 将任务加入队列（每个设备对应一个任务）
        m_taskQueue.append(qMakePair(index, selectedPartition));
    }

    // 2. 启动第一个任务（若队列非空）
    if (!m_taskQueue.isEmpty()) {
        startNextTask();
    } else {
        ui->pushButton_ok->setEnabled(true);
        ui->pushButton_switch->setEnabled(true);
    }
}

void OTAUpgradeWindow::startNextTask()
{
    if (m_currentTaskIndex >= m_taskQueue.size()) {
        return;
    }

    // 获取当前任务的设备索引和分区
    auto currentTask = m_taskQueue[m_currentTaskIndex];
    int index = currentTask.first;
    QString partition = currentTask.second;
    QString filePath = m_rpdFiles[index];

    qDebug() << (tr("=== 开始执行任务%1：探测器[#%2]（分区：%3） ===")
               .arg(m_currentTaskIndex + 1).arg(index).arg(partition));
    sigWriteLog(tr("=== 开始执行任务%1：探测器[#%2]（分区：%3） ===")
                    .arg(m_currentTaskIndex + 1).arg(index).arg(partition));

    // 1.1.1 发送更新数据包和更新地址（原业务逻辑保留）
    QByteArray sectorData = QByteArray::fromHex("12 34 00 0F D0 00 00 00 00 00 AB CD");
    commHelper->sendOTAUpgradeData(index, sectorData);

    // 1.1.2 计数块数
    qint64 fileSize = QFileInfo(filePath).size();
    quint8 sectorCount = qCeil((double)fileSize / (64*1024)); // 计数块数

    // 2. 按照特定地址块进行Flash擦除指令
    //第4-5字节代表特定地址，6-7字节代表根据文件大小要进行擦除的块区
    sectorData = QByteArray::fromHex("55 04 01 00 30 00 00 f0");
    sectorData[5] = (sectorCount >> 8) & 0xFF;
    sectorData[6] = sectorCount & 0xFF;
    commHelper->sendOTAUpgradeData(index, sectorData);

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
    QFuture<void> future = QtConcurrent::run(this, &OTAUpgradeWindow::asyncSendOTAData,
                                             index, filePath, partition);

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
    ui->pushButton_switch->setEnabled(true);
    this->close();
}


void OTAUpgradeWindow::asyncSendOTAData(int index, const QString& filePath, const QString& selectedPartition)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit sigSendFinished(index, selectedPartition, false);
        return;
    }

    qint64 fileSize = QFileInfo(filePath).size();
    int sendCount = qCeil((double)fileSize / 256);
    bool sendSuccess = true;
    m_currentFileSize[index] = fileSize;
    //m_returnDataSize[index] = 0;

    commHelper->startOTAUpgrade(index);
    for (int i = 0; i < sendCount && sendSuccess; ++i) {
        // 发送进度信号（主线程更新UI）
        emit sigSendProgress(index, selectedPartition, i + 1, sendCount);

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
    if (!sendSuccess)
    {
        emit sigSendFinished(index, selectedPartition, sendSuccess);
    }
    else{
        std::thread([=](){
            emit sigSendFinished(index, selectedPartition, sendSuccess);
        }).join();
    }

    commHelper->endOTAUpgrade(index); // 发送结束指令
}

void OTAUpgradeWindow::slotSendProgress(int index, const QString& partition, int current, int total)
{
    // 更新进度条（主线程安全）
    int progress = (current * 100) / total;
    ui->progressBar->setValue(progress);
}

void OTAUpgradeWindow::slotSendFinished(int index, const QString& partition, bool success)
{
    m_runningTasks--;

    if (success) {
        // 发送完成指令（原业务逻辑保留）
        QByteArray finishData = QByteArray::fromHex("55 06 01 00 00 00 00 f0");
        if (commHelper->sendOTAUpgradeData(index, finishData)) {            
            qDebug() << (tr("探测器[#%1][%2]数据发送完毕！").arg(index).arg(partition));
            emit sigWriteLog(tr("探测器[#%1][%2]数据发送完毕！").arg(index).arg(partition));
        } else {
            qCritical() << (tr("探测器[#%1][%2]发送完成指令失败！").arg(index).arg(partition));
            sigWriteLog(tr("探测器[#%1][%2]发送完成指令失败！").arg(index).arg(partition), QtCriticalMsg);
        }
    } else {
        qCritical() << (tr("探测器[#%1][%2]更新失败！").arg(index).arg(partition));
        sigWriteLog(tr("探测器[#%1][%2]更新失败！").arg(index).arg(partition), QtCriticalMsg);
    }

    // 按钮状态
    if (m_runningTasks == 0) {
        emit sigUpgradeFinished();
    }
}

void OTAUpgradeWindow::slotUpgradeFinished()
{
    // 所有任务完成
    qDebug() << ("=== 所有OTA任务执行完毕 ===", QtInfoMsg);
    sigWriteLog("=== 所有OTA任务执行完毕 ===", QtInfoMsg);
    ui->pushButton_ok->setEnabled(true);
    ui->pushButton_switch->setEnabled(true);
    ui->progressBar->setValue(0);
}
