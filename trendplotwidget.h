/*
 * @Author: MrPan
 * @Date: 2026-03-23 16:14:09
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-03-23 17:30:35
 * @Description: 请填写简介
 */
#ifndef TRENDPLOTWIDGET_H
#define TRENDPLOTWIDGET_H

#include <QWidget>
#include "qcustomplothelper.h"

// 用于绘制不断刷新的曲线类型，比如电压/电流/温度监测曲线
class TrendPlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TrendPlotWidget(QWidget *parent = nullptr);
    ~TrendPlotWidget();
    
    void setTitle(const QString& title);
    void setGraphColor(const QColor& color);
    void setGraphName(int graphIndex, const QString& name);
    void setLegendVisible(bool visible);
    void setupNumericLegend();
    void setXAxisLabel(const QString& text);
    void setYAxisLabel(const QString& text);
    void setTimeWindow(int seconds);
    void appendPoint(double x, double y, int graphIndex = 0);
    void appendPoints(double x, const QVector<double>& y);
    void clearData();
    void refreshPlot();
    void ensureGraphCount(int count);
    void addGraph(int count = 1);

signals:

private:
    void styleGraph(QCPGraph *graph, int colorIndex);
    QCustomPlot *m_plot;
    QCPTextElement* m_title = nullptr;
    QCPGraph* m_graph = nullptr;
    int m_timeWindowSeconds = 300;
};

#endif // TRENDPLOTWIDGET_H
