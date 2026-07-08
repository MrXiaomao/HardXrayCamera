#ifndef SIGNALWIDTHSETTING_H
#define SIGNALWIDTHSETTING_H

#include <QDialog>

namespace Ui {
class SignalWidthSetting;
}

class SignalWidthSetting : public QDialog
{
    Q_OBJECT

public:
    explicit SignalWidthSetting(QWidget *parent = nullptr);
    ~SignalWidthSetting();

private slots:
    void on_pushButton_ok_clicked();

    void on_pushButton_close_clicked();

private:
    Ui::SignalWidthSetting *ui;
};

#endif // SIGNALWIDTHSETTING_H
