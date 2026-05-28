/*
 * @Author: MrPan
 * @Date: 2026-03-24 14:36:24
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-27 11:09:50
 * @Description: 请填写简介
 */
#include "detectorsetting.h"
#include "ui_detectorsetting.h"
#include "globalsettings.h"
#include <QHeaderView>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QWheelEvent>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>

namespace {
constexpr int ChannelCount = 32;
constexpr int ThresholdTableRows = ChannelCount / 2;
constexpr int ThresholdMin = 0;
constexpr int ThresholdMax = 65525;
constexpr int DefaultThreshold = 50;

// 16 道能谱能窗 CSV：共 33 行、每行 18 列；第 1 行标题，第 2～33 行为数据（32 行×18 列）
constexpr int kCsvTotalLineCount = 33;
constexpr int kCsvColumnCount = 18;
constexpr int kCsvValueColFirst = 2;   // 1-based，参与校验的首列
constexpr int kCsvValueColLast = 18;   // 1-based，参与校验的末列
constexpr int kCsvValueMin = 0;
constexpr int kCsvValueMax = 65535;

QStringList parseCsvFields(const QString& line)
{
    const QStringList parts = line.split(QLatin1Char(','), Qt::KeepEmptyParts);
    QStringList fields;
    fields.reserve(parts.size());
    for (const QString& part : parts)
        fields.append(part.trimmed());
    return fields;
}

QStringList readCsvLines(const QString& filePath, QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QObject::tr("无法打开文件：%1").arg(filePath);
        return {};
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    QStringList lines;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        lines.append(line);
    }

    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();

    return lines;
}

class NoWheelSpinBox : public QSpinBox
{
public:
    explicit NoWheelSpinBox(QWidget* parent = nullptr)
        : QSpinBox(parent)
    {
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};
}

DetectorSetting::DetectorSetting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DetectorSetting)
{
    ui->setupUi(this);

    ui->pushButton->setText("应用到全部");
    ui->pushButton_2->setText("应用到选中通道");
    ui->spb_waveThreshold1->setRange(ThresholdMin, ThresholdMax);
    initThresholdTable();
    connect(ui->pushButton, &QPushButton::clicked, this, &DetectorSetting::applyThresholdToAll);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &DetectorSetting::applyThresholdToChecked);
    connect(ui->bt_selectCsv, &QPushButton::clicked, this, &DetectorSetting::onSelectCsvFile);
    //给一个默认IP
    ui->widget_detIP1->setIP("0.0.0.0");
    ui->widget_detIP2->setIP("0.0.0.0");
    ui->widget_armIP1->setIP("0.0.0.0");
    ui->widget_armIP2->setIP("0.0.0.0");
    ui->widget_relayIP->setIP("0.0.0.0");

    loadSettings();
}

DetectorSetting::~DetectorSetting()
{
    delete ui;
}

// 加载界面的参数
void DetectorSetting::loadSettings()
{
    JsonSettings* settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    // 网络配置读取
    QString ip_det1 = settings->getValueByPath("network/ip1").toString();
    QString ip_det2 = settings->getValueByPath("network/ip2").toString();
    QString ip_arm1 = settings->getValueByPath("network/ip_arm1").toString();
    QString ip_arm2 = settings->getValueByPath("network/ip_arm2").toString();
    QString ip_relay = settings->getValueByPath("network/ip_relay").toString();
    ui->widget_detIP1->setIP(ip_det1);
    ui->widget_detIP2->setIP(ip_det2);
    ui->widget_armIP1->setIP(ip_arm1);
    ui->widget_armIP2->setIP(ip_arm2);
    ui->widget_relayIP->setIP(ip_relay);
    ui->spinBox_portDet1->setValue(settings->getValueByPath("network/port_det1").toInt());
    ui->spinBox_portDet2->setValue(settings->getValueByPath("network/port_det2").toInt());
    ui->spinBox_armPort1->setValue(settings->getValueByPath("network/port_arm1").toInt());
    ui->spinBox_armPort2->setValue(settings->getValueByPath("network/port_arm2").toInt());
    ui->spinBox_portRelay->setValue(settings->getValueByPath("network/port_relay").toInt());

    JsonSettings *runSettings = GlobalSettings::instance()->mRunSettings;
    ScopedFileLock runLock(runSettings);
    ui->spb_udpPort->setValue(runSettings->getValueByPath("Network/udpBroadcastPort", 6000).toInt());

    // 硬件参数读取
    ui->spb_specRefashTime->setValue(settings->getValueByPath("FPGA/spec_refash_time").toInt());
    ui->spb_threshold->setValue(settings->getValueByPath("FPGA/threshold").toInt());
    ui->spb_deadTime->setValue(settings->getValueByPath("FPGA/deadTime").toInt());
    ui->spb_waveThreshold1->setValue(settings->getValueByPath("FPGA/wave/threshold1", DefaultThreshold).toInt());
    for (int channel = 1; channel <= ChannelCount; ++channel) {
        const int value = settings->getValueByPath(QString("FPGA/wave/threshold%1").arg(channel),
                                                   DefaultThreshold).toInt();
        setThresholdValue(channel, value);
    }

    const QString csvPath = settings->getValueByPath("FPGA/16SpecEnWindow_csv_path").toString();
    if (!csvPath.isEmpty() && QFile::exists(csvPath)) {
        m_csvFilePath = csvPath;
        ui->lineEdit_csvPath->setText(csvPath);
    } else {
        m_csvFilePath.clear();
        ui->lineEdit_csvPath->clear();
    }
}

//保存界面参数
void DetectorSetting::on_btn_ok_accepted()
{
    JsonSettings* settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    // 网络配置读取
    QString ip_det1 = ui->widget_detIP1->getIP();
    QString ip_det2 = ui->widget_detIP2->getIP();
    QString ip_arm1 = ui->widget_armIP1->getIP();
    QString ip_arm2 = ui->widget_armIP2->getIP();
    QString ip_relay = ui->widget_relayIP->getIP();
    
    int port_det1 = ui->spinBox_portDet1->value();
    int port_det2 = ui->spinBox_portDet2->value();
    int port_arm1 = ui->spinBox_armPort1->value();
    int port_arm2 = ui->spinBox_armPort2->value();
    int port_relay = ui->spinBox_portRelay->value();

    settings->setValueByPath("network/ip1", ip_det1);
    settings->setValueByPath("network/ip2", ip_det2);
    settings->setValueByPath("network/ip_arm1", ip_arm1);
    settings->setValueByPath("network/ip_arm2", ip_arm2);
    settings->setValueByPath("network/ip_relay", ip_relay);
    settings->setValueByPath("network/port_det1", port_det1);
    settings->setValueByPath("network/port_det2", port_det2);
    settings->setValueByPath("network/port_arm1", port_arm1);
    settings->setValueByPath("network/port_arm2", port_arm2);
    settings->setValueByPath("network/port_relay", port_relay);

    //硬件参数读取
    settings->setValueByPath("FPGA/spec_refash_time", ui->spb_specRefashTime->value());
    settings->setValueByPath("FPGA/threshold", ui->spb_threshold->text());
    settings->setValueByPath("FPGA/deadTime", ui->spb_deadTime->text());

    for (int channel = 1; channel <= ChannelCount; ++channel) {
        settings->setValueByPath(QString("FPGA/wave/threshold%1").arg(channel),
                                 thresholdValue(channel));
    }

    settings->setValueByPath("FPGA/16SpecEnWindow_csv_path", m_csvFilePath);

    settings->save();

    if (ui->spb_udpPort->isEnabled()) {
        JsonSettings *runSettings = GlobalSettings::instance()->mRunSettings;
        ScopedFileLock runLock(runSettings);
        runSettings->setValueByPath("Network/udpBroadcastPort", ui->spb_udpPort->value());
        runSettings->save();
    }
}

void DetectorSetting::setUdpPortEditable(bool editable)
{
    ui->spb_udpPort->setEnabled(editable);
}

bool DetectorSetting::validate16SpecEnWindowCsv(const QString& filePath, QString* errorMessage)
{
    /*
     * 16 道能谱能窗 CSV 格式规则：
     * 1. 文件必须有 33 行（第 1 行标题 + 第 2～33 行数据，整体为 33 行×18 列表格）；
     * 2. 第 1 行为标题行，不检查其内容；
     * 3. 第 2～33 行每行必须有 18 列；
     * 4. 每行第 2～18 列（共 17 个数值）须为 [0, 65535] 内的整数；
     * 5. 每行第 2～18 列须严格递增（后一列 > 前一列，不允许相等）。
     */
    const QStringList lines = readCsvLines(filePath, errorMessage);
    if (lines.isEmpty() && errorMessage && !errorMessage->isEmpty())
        return false;

    if (lines.size() != kCsvTotalLineCount) {
        if (errorMessage) {
            *errorMessage = tr("CSV 行数必须为 %1 行（当前 %2 行）")
                                .arg(kCsvTotalLineCount)
                                .arg(lines.size());
        }
        return false;
    }

    for (int lineIndex = 1; lineIndex < kCsvTotalLineCount; ++lineIndex) {
        const int lineNumber = lineIndex + 1;
        const QStringList fields = parseCsvFields(lines.at(lineIndex));

        if (fields.size() < kCsvColumnCount) {
            if (errorMessage) {
                *errorMessage = tr("第 %1 行列数不足，需要 %2 列（当前 %3 列）")
                                    .arg(lineNumber)
                                    .arg(kCsvColumnCount)
                                    .arg(fields.size());
            }
            return false;
        }

        int previousValue = -1;
        for (int col = kCsvValueColFirst; col <= kCsvValueColLast; ++col) {
            const QString& text = fields.at(col - 1);
            bool ok = false;
            const int value = text.toInt(&ok);
            if (!ok || text.contains(QLatin1Char('.'))) {
                if (errorMessage) {
                    *errorMessage = tr("第 %1 行第 %2 列不是合法整数：%3")
                                        .arg(lineNumber)
                                        .arg(col)
                                        .arg(text);
                }
                return false;
            }
            if (value < kCsvValueMin || value > kCsvValueMax) {
                if (errorMessage) {
                    *errorMessage = tr("第 %1 行第 %2 列数值超出范围 [%3, %4]：%5")
                                        .arg(lineNumber)
                                        .arg(col)
                                        .arg(kCsvValueMin)
                                        .arg(kCsvValueMax)
                                        .arg(value);
                }
                return false;
            }
            if (previousValue >= 0 && value <= previousValue) {
                if (errorMessage) {
                    *errorMessage = tr("第 %1 行第 %2～%3 列须严格递增，第 %4 列 (%5) 不大于前一列 (%6)")
                                        .arg(lineNumber)
                                        .arg(kCsvValueColFirst)
                                        .arg(kCsvValueColLast)
                                        .arg(col)
                                        .arg(value)
                                        .arg(previousValue);
                }
                return false;
            }
            previousValue = value;
        }
    }

    return true;
}

bool DetectorSetting::load16SpecEnWindowCsv(const QString& filePath,
                                            QVector<QVector<quint16>>& channelBoundaries,
                                            QString* errorMessage)
{
    channelBoundaries.clear();
    if (!validate16SpecEnWindowCsv(filePath, errorMessage))
        return false;

    const QStringList lines = readCsvLines(filePath, errorMessage);
    if (lines.size() != kCsvTotalLineCount) {
        if (errorMessage) {
            *errorMessage = tr("CSV 行数必须为 %1 行（当前 %2 行）")
                                .arg(kCsvTotalLineCount)
                                .arg(lines.size());
        }
        return false;
    }

    channelBoundaries.reserve(kCsvTotalLineCount - 1);
    for (int lineIndex = 1; lineIndex < kCsvTotalLineCount; ++lineIndex) {
        const QStringList fields = parseCsvFields(lines.at(lineIndex));
        QVector<quint16> boundaries;
        boundaries.reserve(kCsvValueColLast - kCsvValueColFirst + 1);
        for (int col = kCsvValueColFirst; col <= kCsvValueColLast; ++col)
            boundaries.append(static_cast<quint16>(fields.at(col - 1).toInt()));
        channelBoundaries.append(boundaries);
    }

    if (channelBoundaries.size() != 32) {
        if (errorMessage)
            *errorMessage = tr("CSV 数据通道数必须为 32（当前 %1）").arg(channelBoundaries.size());
        channelBoundaries.clear();
        return false;
    }

    return true;
}

void DetectorSetting::onSelectCsvFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择 CSV 文件"),
        QString(),
        tr("CSV 文件 (*.csv);;所有文件 (*.*)"));

    if (filePath.isEmpty())
        return;

    QString errorMessage;
    if (!DetectorSetting::validate16SpecEnWindowCsv(filePath, &errorMessage)) {
        QMessageBox::warning(this, tr("CSV 文件格式错误"), errorMessage);
        return;
    }

    m_csvFilePath = filePath;
    ui->lineEdit_csvPath->setText(filePath);
}

void DetectorSetting::initThresholdTable()
{
    ui->tableThreshold->clear();
    ui->tableThreshold->setColumnCount(4);
    ui->tableThreshold->setRowCount(ThresholdTableRows);
    ui->tableThreshold->setHorizontalHeaderLabels(QStringList() << "通道号" << "波形阈值" << "通道号" << "波形阈值");
    ui->tableThreshold->verticalHeader()->setVisible(false);
    ui->tableThreshold->horizontalHeader()->setStretchLastSection(true);
    ui->tableThreshold->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableThreshold->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableThreshold->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableThreshold->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    ui->tableThreshold->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableThreshold->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int row = 0; row < ThresholdTableRows; ++row) {
        for (int side = 0; side < 2; ++side) {
            const int channel = row * 2 + side + 1;
            const int channelColumn = side * 2;
            const int thresholdColumn = channelColumn + 1;

            QTableWidgetItem* channelItem = new QTableWidgetItem(QString("CH%1").arg(channel));
            channelItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            channelItem->setCheckState(Qt::Unchecked);
            ui->tableThreshold->setItem(row, channelColumn, channelItem);

            QSpinBox* thresholdSpinBox = new NoWheelSpinBox(ui->tableThreshold);
            thresholdSpinBox->setRange(ThresholdMin, ThresholdMax);
            thresholdSpinBox->setValue(DefaultThreshold);
            thresholdSpinBox->setAlignment(Qt::AlignCenter);
            ui->tableThreshold->setCellWidget(row, thresholdColumn, thresholdSpinBox);
        }
    }
}

void DetectorSetting::applyThresholdToAll()
{
    const int value = ui->spb_waveThreshold1->value();
    for (int channel = 1; channel <= ChannelCount; ++channel)
        setThresholdValue(channel, value);
}

void DetectorSetting::applyThresholdToChecked()
{
    const int value = ui->spb_waveThreshold1->value();
    for (int channel = 1; channel <= ChannelCount; ++channel) {
        const int row = (channel - 1) / 2;
        const int channelColumn = ((channel - 1) % 2) * 2;
        QTableWidgetItem* item = ui->tableThreshold->item(row, channelColumn);
        if (item && item->checkState() == Qt::Checked)
            setThresholdValue(channel, value);
    }
}

int DetectorSetting::thresholdValue(int channel) const
{
    const int row = (channel - 1) / 2;
    const int thresholdColumn = ((channel - 1) % 2) * 2 + 1;
    QSpinBox* spinBox = qobject_cast<QSpinBox*>(ui->tableThreshold->cellWidget(row, thresholdColumn));
    return spinBox ? spinBox->value() : DefaultThreshold;
}

void DetectorSetting::setThresholdValue(int channel, int value)
{
    const int row = (channel - 1) / 2;
    const int thresholdColumn = ((channel - 1) % 2) * 2 + 1;
    QSpinBox* spinBox = qobject_cast<QSpinBox*>(ui->tableThreshold->cellWidget(row, thresholdColumn));
    if (spinBox)
        spinBox->setValue(qBound(ThresholdMin, value, ThresholdMax));
}

