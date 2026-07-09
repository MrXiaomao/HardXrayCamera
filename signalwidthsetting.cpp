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
#include <QMessageBox>
void SignalWidthSetting::on_pushButton_ok_clicked()
{
    CommandHelper *commandHelper = CommandHelper::instance();
    commandHelper->sendTriggerSignalTimeWidth(0x11, ui->spinBox_timeWidth->value());
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("设置成功"));
}


void SignalWidthSetting::on_pushButton_close_clicked()
{
    CommandHelper *commandHelper = CommandHelper::instance();
    commandHelper->sendTriggerSignalTimeWidth(0x11);
    this->close();
}
