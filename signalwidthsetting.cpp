#include "signalwidthsetting.h"
#include "ui_signalwidthsetting.h"

SignalWidthSetting::SignalWidthSetting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SignalWidthSetting)
{
    ui->setupUi(this);
}

SignalWidthSetting::~SignalWidthSetting()
{
    delete ui;
}

#include "commandhelper.h"
void SignalWidthSetting::on_pushButton_ok_clicked()
{
    CommandHelper *commandHelper = CommandHelper::instance();
    commandHelper->sendTriggerSignalTimeWidth(0x10, ui->spinBox_timeWidth->value());
}


void SignalWidthSetting::on_pushButton_close_clicked()
{
    //CommandHelper *commandHelper = CommandHelper::instance();
    //commandHelper->sendTriggerSignalTimeWidth(0x10);
    this->close();
}
