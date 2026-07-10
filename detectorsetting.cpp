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
constexpr int ThresholdChannelsPerRow = 4;
constexpr int ThresholdTableRows = ChannelCount / ThresholdChannelsPerRow;
constexpr int ThresholdTableColumns = ThresholdChannelsPerRow * 2;
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

    connect(ui->bt_selectCsv, &QPushButton::clicked, this, &DetectorSetting::onSelectCsvFile);
    //给一个默认IP
    ui->widget_fpga1MainIP->setIP("0.0.0.0");
    ui->widget_fpga2MainIP->setIP("0.0.0.0");
    ui->widget_fpga1WaveIP->setIP("0.0.0.0");
    ui->widget_fpga2WaveIP->setIP("0.0.0.0");
    ui->widget_armIP1->setIP("192.168.0.90");
    ui->widget_armIP2->setIP("192.168.0.91");
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
    QString ip_fpga1_main = settings->getValueByPath("network/ip_fpga1_main").toString();
    QString ip_fpga2_main = settings->getValueByPath("network/ip_fpga2_main").toString();
    QString ip_fpga1_wave = settings->getValueByPath("network/ip_fpga1_wave").toString();
    QString ip_fpga2_wave = settings->getValueByPath("network/ip_fpga2_wave").toString();
    QString ip_arm1 = settings->getValueByPath("network/ip_arm1").toString();
    QString ip_arm2 = settings->getValueByPath("network/ip_arm2").toString();
    QString ip_relay = settings->getValueByPath("network/ip_relay").toString();
    ui->widget_fpga1MainIP->setIP(ip_fpga1_main);
    ui->widget_fpga2MainIP->setIP(ip_fpga2_main);
    ui->widget_fpga1WaveIP->setIP(ip_fpga1_wave);
    ui->widget_fpga2WaveIP->setIP(ip_fpga2_wave);
    ui->widget_armIP1->setIP(ip_arm1);
    ui->widget_armIP2->setIP(ip_arm2);
    ui->widget_relayIP->setIP(ip_relay);
    ui->spinBox_portFpga1Main->setValue(settings->getValueByPath("network/port_fpga1_main").toInt());
    ui->spinBox_portFpga2Main->setValue(settings->getValueByPath("network/port_fpga2_main").toInt());
    ui->spinBox_portFpga1Wave->setValue(settings->getValueByPath("network/port_fpga1_wave").toInt());
    ui->spinBox_portFpga2Wave->setValue(settings->getValueByPath("network/port_fpga2_wave").toInt());
    ui->spinBox_armPort1->setValue(settings->getValueByPath("network/port_arm1").toInt());
    ui->spinBox_armPort2->setValue(settings->getValueByPath("network/port_arm2").toInt());
    ui->spinBox_portRelay->setValue(settings->getValueByPath("network/port_relay").toInt());

    JsonSettings *runSettings = GlobalSettings::instance()->mRunSettings;
    ScopedFileLock runLock(runSettings);
    ui->spb_udpPort->setValue(runSettings->getValueByPath("Network/udpBroadcastPort", 6000).toInt());

    // 硬件参数读取
    ui->spb_threshold->setValue(settings->getValueByPath("FPGA/threshold").toInt());
    ui->spb_deadTime->setValue(settings->getValueByPath("FPGA/deadTime").toInt());

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

    // 网络配置保存
    QString ip_fpga1_main = ui->widget_fpga1MainIP->getIP();
    QString ip_fpga2_main = ui->widget_fpga2MainIP->getIP();
    QString ip_fpga1_wave = ui->widget_fpga1WaveIP->getIP();
    QString ip_fpga2_wave = ui->widget_fpga2WaveIP->getIP();
    QString ip_arm1 = ui->widget_armIP1->getIP();
    QString ip_arm2 = ui->widget_armIP2->getIP();
    QString ip_relay = ui->widget_relayIP->getIP();

    int port_fpga1_main = ui->spinBox_portFpga1Main->value();
    int port_fpga2_main = ui->spinBox_portFpga2Main->value();
    int port_fpga1_wave = ui->spinBox_portFpga1Wave->value();
    int port_fpga2_wave = ui->spinBox_portFpga2Wave->value();
    int port_arm1 = ui->spinBox_armPort1->value();
    int port_arm2 = ui->spinBox_armPort2->value();
    int port_relay = ui->spinBox_portRelay->value();

    settings->setValueByPath("network/ip_fpga1_main", ip_fpga1_main);
    settings->setValueByPath("network/ip_fpga2_main", ip_fpga2_main);
    settings->setValueByPath("network/ip_fpga1_wave", ip_fpga1_wave);
    settings->setValueByPath("network/ip_fpga2_wave", ip_fpga2_wave);
    settings->setValueByPath("network/ip_arm1", ip_arm1);
    settings->setValueByPath("network/ip_arm2", ip_arm2);
    settings->setValueByPath("network/ip_relay", ip_relay);
    settings->setValueByPath("network/port_fpga1_main", port_fpga1_main);
    settings->setValueByPath("network/port_fpga2_main", port_fpga2_main);
    settings->setValueByPath("network/port_fpga1_wave", port_fpga1_wave);
    settings->setValueByPath("network/port_fpga2_wave", port_fpga2_wave);
    settings->setValueByPath("network/port_arm1", port_arm1);
    settings->setValueByPath("network/port_arm2", port_arm2);
    settings->setValueByPath("network/port_relay", port_relay);

    //硬件参数读取
    settings->setValueByPath("FPGA/threshold", ui->spb_threshold->text());
    settings->setValueByPath("FPGA/deadTime", ui->spb_deadTime->text());
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
