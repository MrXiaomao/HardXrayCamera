#ifndef MEASUREMENTBINARYWRITER_H
#define MEASUREMENTBINARYWRITER_H

#include <QObject>
#include <QFile>
#include <QString>
#include <atomic>

// 在独立线程中写二进制测量文件，避免 TCP 收包写盘占用 UI 主线程
class MeasurementBinaryWriter : public QObject
{
    Q_OBJECT
public:
    explicit MeasurementBinaryWriter(QObject *parent = nullptr);
    ~MeasurementBinaryWriter() override;

public slots:
    void openFiles(const QString &fpga1MainPath,
                   const QString &fpga2MainPath,
                   const QString &fpga1WavePath,
                   const QString &fpga2WavePath);
    void closeFiles();

    void writeFpga1Main(const QByteArray &data);
    void writeFpga2Main(const QByteArray &data);
    void writeFpga1Wave(const QByteArray &data);
    void writeFpga2Wave(const QByteArray &data);

    void setUpgradeActive(bool active);

private:
    void writeIfOpen(QFile &file, const QByteArray &data);

    QFile m_fpga1MainFile;
    QFile m_fpga2MainFile;
    QFile m_fpga1WaveFile;
    QFile m_fpga2WaveFile;
    std::atomic_bool m_upgradeActive{false};
};

#endif // MEASUREMENTBINARYWRITER_H
