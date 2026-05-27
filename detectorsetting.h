#ifndef DETECTORSETTING_H
#define DETECTORSETTING_H

#include <QDialog>
#include <QString>

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

    // 校验 16 道能谱能窗 CSV，供本类及其他模块直接调用：DetectorSetting::validate16SpecEnWindowCsv(path)
    static bool validate16SpecEnWindowCsv(const QString& filePath, QString* errorMessage = nullptr);

private slots:
    void on_btn_ok_accepted();
    void onSelectCsvFile();

private:
    void initThresholdTable();
    void applyThresholdToAll();
    void applyThresholdToChecked();
    int thresholdValue(int channel) const;
    void setThresholdValue(int channel, int value);

    QString m_csvFilePath;
    Ui::DetectorSetting *ui;
};

#endif // DETECTORSETTING_H
