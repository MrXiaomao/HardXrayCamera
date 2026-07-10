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

    // 开机监听 UDP 期间禁止修改端口
    void setUdpPortEditable(bool editable);

    // 校验 16 道能谱能窗 CSV，供本类及其他模块直接调用：DetectorSetting::validate16SpecEnWindowCsv(path)
    static bool validate16SpecEnWindowCsv(const QString& filePath, QString* errorMessage = nullptr);
    // 解析 CSV 为 32 通道×17 个道址边界；须先通过 validate16SpecEnWindowCsv
    static bool load16SpecEnWindowCsv(const QString& filePath,
                                      QVector<QVector<quint16>>& energyBoundaries,
                                      QString* errorMessage = nullptr);
    // 从配置路径重新加载 16 道能量边界到静态缓存
    static bool reloadEnergyBoundaries(QString* errorMessage = nullptr);
    static bool reloadEnergyBoundaries(const QString& filePath, QString* errorMessage = nullptr);
    static const QVector<QVector<quint16>>& energyBoundaries();

private slots:
    void on_btn_ok_accepted();
    void onSelectCsvFile();

private:
    static QVector<QVector<quint16>> s_energyBoundaries;

    QString m_csvFilePath;
    Ui::DetectorSetting *ui;
};

#endif // DETECTORSETTING_H
