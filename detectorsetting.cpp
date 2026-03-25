/*
 * @Author: MrPan
 * @Date: 2026-03-24 14:36:24
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-03-24 21:50:03
 * @Description: 请填写简介
 */
#include "detectorsetting.h"
#include "ui_detectorsetting.h"
#include <QIntValidator>
#include "globalsettings.h"

DetectorSetting::DetectorSetting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DetectorSetting)
{
    ui->setupUi(this);
    ui->le_portDet1->setValidator(new QIntValidator(0, 65535, ui->le_portDet1));
    ui->le_portDet2->setValidator(new QIntValidator(0, 65535, ui->le_portDet2));
    ui->le_portRelay->setValidator(new QIntValidator(0, 65535, ui->le_portRelay));

    //给一个默认IP
    ui->widget_detIP1->setIP("0.0.0.0");
    ui->widget_detIP2->setIP("0.0.0.0");
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
    QString ip_relay = settings->getValueByPath("network/ip_relay").toString();
    ui->widget_detIP1->setIP(ip_det1);
    ui->widget_detIP2->setIP(ip_det2);
    ui->widget_relayIP->setIP(ip_relay);
    ui->le_portDet1->setText(settings->getValueByPath("network/port_det1").toString());
    ui->le_portDet2->setText(settings->getValueByPath("network/port_det2").toString());
    ui->le_portRelay->setText(settings->getValueByPath("network/port_relay").toString());

    // 硬件参数读取
    ui->spb_specRefashTime->setValue(settings->getValueByPath("FPGA/spec_refash_time").toInt());
    ui->spb_threshold->setValue(settings->getValueByPath("FPGA/threshold").toInt());
    ui->spb_deadTime->setValue(settings->getValueByPath("FPGA/deadTime").toInt());
    ui->spb_waveThreshold1->setValue(settings->getValueByPath("FPGA/wave/threshold1").toInt());
    ui->spb_waveThreshold2->setValue(settings->getValueByPath("FPGA/wave/threshold2").toInt());
    ui->spb_waveThreshold3->setValue(settings->getValueByPath("FPGA/wave/threshold3").toInt());
    ui->spb_waveThreshold4->setValue(settings->getValueByPath("FPGA/wave/threshold4").toInt());
    ui->spb_waveThreshold5->setValue(settings->getValueByPath("FPGA/wave/threshold5").toInt());
    ui->spb_waveThreshold6->setValue(settings->getValueByPath("FPGA/wave/threshold6").toInt());
    ui->spb_waveThreshold7->setValue(settings->getValueByPath("FPGA/wave/threshold7").toInt());
    ui->spb_waveThreshold8->setValue(settings->getValueByPath("FPGA/wave/threshold8").toInt());
    ui->spb_waveThreshold9->setValue(settings->getValueByPath("FPGA/wave/threshold9").toInt());
    ui->spb_waveThreshold10->setValue(settings->getValueByPath("FPGA/wave/threshold10").toInt());
    ui->spb_waveThreshold11->setValue(settings->getValueByPath("FPGA/wave/threshold11").toInt());
    ui->spb_waveThreshold12->setValue(settings->getValueByPath("FPGA/wave/threshold12").toInt());
    ui->spb_waveThreshold13->setValue(settings->getValueByPath("FPGA/wave/threshold13").toInt());
    ui->spb_waveThreshold14->setValue(settings->getValueByPath("FPGA/wave/threshold14").toInt());
    ui->spb_waveThreshold15->setValue(settings->getValueByPath("FPGA/wave/threshold15").toInt());
    ui->spb_waveThreshold16->setValue(settings->getValueByPath("FPGA/wave/threshold16").toInt());
}

//保存界面参数
void DetectorSetting::on_btn_ok_accepted()
{
    JsonSettings* settings = GlobalSettings::instance()->mUserSettings;
    ScopedFileLock lock(settings);

    // 网络配置读取
    QString ip_det1 = ui->widget_detIP1->getIP();
    QString ip_det2 = ui->widget_detIP2->getIP();
    QString ip_relay = ui->widget_relayIP->getIP();
    
    QString port_det1 = ui->le_portDet1->text();
    QString port_det2 = ui->le_portDet2->text();
    QString port_relay = ui->le_portRelay->text();

    settings->setValueByPath("network/ip1", ip_det1);
    settings->setValueByPath("network/ip2", ip_det2);
    settings->setValueByPath("network/ip_relay", ip_relay);
    settings->setValueByPath("network/port_det1", port_det1);
    settings->setValueByPath("network/port_det2", port_det2);
    settings->setValueByPath("network/port_relay", port_relay);

    //硬件参数读取
    settings->setValueByPath("FPGA/spec_refash_time", ui->spb_specRefashTime->value());
    settings->setValueByPath("FPGA/threshold", ui->spb_threshold->text());
    settings->setValueByPath("FPGA/deadTime", ui->spb_deadTime->text());

    settings->setValueByPath("FPGA/wave/threshold1", ui->spb_waveThreshold1->text());
    settings->setValueByPath("FPGA/wave/threshold2", ui->spb_waveThreshold2->text());
    settings->setValueByPath("FPGA/wave/threshold3", ui->spb_waveThreshold3->text());
    settings->setValueByPath("FPGA/wave/threshold4", ui->spb_waveThreshold4->text());
    settings->setValueByPath("FPGA/wave/threshold5", ui->spb_waveThreshold5->text());
    settings->setValueByPath("FPGA/wave/threshold6", ui->spb_waveThreshold6->text());
    settings->setValueByPath("FPGA/wave/threshold7", ui->spb_waveThreshold7->text());
    settings->setValueByPath("FPGA/wave/threshold8", ui->spb_waveThreshold8->text());
    settings->setValueByPath("FPGA/wave/threshold9", ui->spb_waveThreshold9->text());
    settings->setValueByPath("FPGA/wave/threshold10", ui->spb_waveThreshold10->text());
    settings->setValueByPath("FPGA/wave/threshold11", ui->spb_waveThreshold11->text());
    settings->setValueByPath("FPGA/wave/threshold12", ui->spb_waveThreshold12->text());
    settings->setValueByPath("FPGA/wave/threshold13", ui->spb_waveThreshold13->text());
    settings->setValueByPath("FPGA/wave/threshold14", ui->spb_waveThreshold14->text());
    settings->setValueByPath("FPGA/wave/threshold15", ui->spb_waveThreshold15->text());
    settings->setValueByPath("FPGA/wave/threshold16", ui->spb_waveThreshold16->text());

    // 保存文件
    settings->save();
}

