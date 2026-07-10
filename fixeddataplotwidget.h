#ifndef FIXEDDATAPLOTWIDGET_H
#define FIXEDDATAPLOTWIDGET_H

#include <QWidget>
#include "qcustomplothelper.h"

// 用于绘制固定长度的数据类型，比如波形、能谱
class FixedDataPlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FixedDataPlotWidget(QWidget *parent = nullptr);
    ~FixedDataPlotWidget();
    
    void setTitle(const QString& title);
    void setGraphColor(const QColor& color);
    void setGraphColor(int graphIndex, const QColor& color);
    void setGraphName(int graphIndex, const QString& name);
    void setLegendVisible(bool visible);
    void setXAxisLabel(const QString& text);
    void setYAxisLabel(const QString& text);
    void setXRange(double min, double max);
    void setData(const QVector<double>& x, const QVector<double>& y);
    void setData(const QVector<quint32>& x, const QVector<quint32>& y);
    void ensureGraphCount(int count);
    void setGraphData(int graphIndex, const QVector<double>& x, const QVector<double>& y);
    void clearData();
    void clearAllGraphData();
    void refreshPlot(bool rescaleX = true, bool rescaleY = true);

signals:
    void selectRangeChanged(const QCPRange& range);

private:
    QCustomPlot *m_plot;
    QVector<double> m_xData;
    QVector<double> m_yData;

    QCPTextElement* m_title = nullptr;
    QCPGraph* m_graph = nullptr;
    QVector<QCPGraph*> m_graphs;
};

#endif // FIXEDDATAPLOTWIDGET_H
