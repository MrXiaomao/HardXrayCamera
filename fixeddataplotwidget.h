#ifndef FIXEDDATAPLOTWIDGET_H
#define FIXEDDATAPLOTWIDGET_H

#include <QWidget>
class QCustomPlot;
class QCPTextElement;
class QCPGraph;

// 用于绘制固定长度的数据类型，比如波形、能谱
class FixedDataPlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FixedDataPlotWidget(QWidget *parent = nullptr);
    ~FixedDataPlotWidget();
    
    void setTitle(const QString& title);
    void setGraphColor(const QColor& color);
    void setXAxisLabel(const QString& text);
    void setYAxisLabel(const QString& text);
    void setXRange(double min, double max);
    void setData(const QVector<double>& x, const QVector<double>& y);
    void setData(const QVector<quint32>& x, const QVector<quint32>& y);
    void clearData();
    void refreshPlot();

signals:

private:
    QCustomPlot *m_plot;
    QVector<double> m_xData;
    QVector<double> m_yData;

    QCPTextElement* m_title = nullptr;
    QCPGraph* m_graph = nullptr;
};

#endif // FIXEDDATAPLOTWIDGET_H
