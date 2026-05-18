/*
 * @Author: MrPan
 * @Date: 2026-03-24 14:36:24
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-05-18 16:12:52
 * @Description: 请填写简介
 */
#include "detectorsetting.h"
#include "ui_detectorsetting.h"
#include "globalsettings.h"
#include <QHeaderView>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QWheelEvent>

namespace {
constexpr int ChannelCount = 32;
constexpr int ThresholdTableRows = ChannelCount / 2;
constexpr int ThresholdMin = 0;
constexpr int ThresholdMax = 65525;
constexpr int DefaultThreshold = 50;

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

    // 保存文件
    settings->save();
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

