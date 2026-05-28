#ifndef DETECTORSETTING_H
#define DETECTORSETTING_H

#include <QDialog>
#include <QString>
#include <QVector>

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
    // 解析 CSV 为 32 通道×17 个道址边界；须先通过 validate16SpecEnWindowCsv
    static bool load16SpecEnWindowCsv(const QString& filePath,
                                      QVector<QVector<quint16>>& channelBoundaries,
                                      QString* errorMessage = nullptr);

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
