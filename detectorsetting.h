#ifndef DETECTORSETTING_H
#define DETECTORSETTING_H

#include <QDialog>

namespace Ui {
class DetectorSetting;
}

class DetectorSetting : public QDialog
{
    Q_OBJECT

public:
    explicit DetectorSetting(QWidget *parent = nullptr);
    ~DetectorSetting();

    void loadSettings();
private slots:
    void on_btn_ok_accepted();

private:
    Ui::DetectorSetting *ui;
};

#endif // DETECTORSETTING_H
