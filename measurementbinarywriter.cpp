#include "measurementbinarywriter.h"
#include <QDebug>

MeasurementBinaryWriter::MeasurementBinaryWriter(QObject *parent)
    : QObject(parent)
{
}

MeasurementBinaryWriter::~MeasurementBinaryWriter()
{
    closeFiles();
}

void MeasurementBinaryWriter::openFiles(const QString &fpga1MainPath,
                                        const QString &fpga2MainPath,
                                        const QString &fpga1WavePath,
                                        const QString &fpga2WavePath)
{
    closeFiles();

    m_fpga1MainFile.setFileName(fpga1MainPath);
    m_fpga2MainFile.setFileName(fpga2MainPath);
    m_fpga1WaveFile.setFileName(fpga1WavePath);
    m_fpga2WaveFile.setFileName(fpga2WavePath);

    if (!m_fpga1MainFile.open(QIODevice::WriteOnly | QIODevice::Append))
        qWarning() << "水平相机能谱数据创建失败，文件名：" << fpga1MainPath;
    if (!m_fpga2MainFile.open(QIODevice::WriteOnly | QIODevice::Append))
        qWarning() << "垂直相机能谱数据创建失败，文件名：" << fpga2MainPath;
    if (!m_fpga1WaveFile.open(QIODevice::WriteOnly | QIODevice::Append))
        qWarning() << "水平相机波形数据创建失败，文件名：" << fpga1WavePath;
    if (!m_fpga2WaveFile.open(QIODevice::WriteOnly | QIODevice::Append))
        qWarning() << "垂直相机波形数据创建失败，文件名：" << fpga2WavePath;
}

void MeasurementBinaryWriter::closeFiles()
{
    if (m_fpga1MainFile.isOpen())
        m_fpga1MainFile.close();
    if (m_fpga2MainFile.isOpen())
        m_fpga2MainFile.close();
    if (m_fpga1WaveFile.isOpen())
        m_fpga1WaveFile.close();
    if (m_fpga2WaveFile.isOpen())
        m_fpga2WaveFile.close();
}

void MeasurementBinaryWriter::setUpgradeActive(bool active)
{
    m_upgradeActive.store(active);
}

void MeasurementBinaryWriter::writeIfOpen(QFile &file, const QByteArray &data)
{
    if (m_upgradeActive.load() || data.isEmpty() || !file.isOpen())
        return;
    file.write(data);
}

void MeasurementBinaryWriter::writeFpga1Main(const QByteArray &data)
{
    writeIfOpen(m_fpga1MainFile, data);
}

void MeasurementBinaryWriter::writeFpga2Main(const QByteArray &data)
{
    writeIfOpen(m_fpga2MainFile, data);
}

void MeasurementBinaryWriter::writeFpga1Wave(const QByteArray &data)
{
    writeIfOpen(m_fpga1WaveFile, data);
}

void MeasurementBinaryWriter::writeFpga2Wave(const QByteArray &data)
{
    writeIfOpen(m_fpga2WaveFile, data);
}
