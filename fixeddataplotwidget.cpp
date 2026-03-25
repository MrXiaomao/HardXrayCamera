/*
 * @Author: MrPan
 * @Date: 2026-03-23 16:15:48
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-03-24 10:08:42
 * @Description: 请填写简介
 */
#include "fixeddataplotwidget.h"
#include "qcustomplot.h"
#include <QSizePolicy>
#include <QVBoxLayout>

FixedDataPlotWidget::FixedDataPlotWidget(QWidget *parent)
    : QWidget{parent}
{
    m_plot = new QCustomPlot(this);
    m_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Use a layout so QCustomPlot always fills this widget on resize.
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_plot);

    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    //创建一条曲线
    m_graph = m_plot->addGraph();
    m_graph->setPen(QPen(Qt::black));
    m_graph->setLineStyle(QCPGraph::lsLine);
    m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));
    m_graph->setBrush(QBrush(QColor(0, 0, 255, 20)));
    m_graph->setAntialiased(true);
    m_graph->setAntialiasedFill(true);
    m_graph->setAntialiasedFill(true);
}

FixedDataPlotWidget::~FixedDataPlotWidget()
{
    delete m_plot;
}

void FixedDataPlotWidget::setTitle(const QString& title)
{
    if (!m_title) {
        m_plot->plotLayout()->insertRow(0);
        m_title = new QCPTextElement(m_plot, title, QFont("微软雅黑", 12, QFont::Bold));
        m_plot->plotLayout()->addElement(0, 0, m_title);
    } else {
        m_title->setText(title);
    }
}

void FixedDataPlotWidget::setGraphColor(const QColor& color)
{
    if (m_graph) {
        m_graph->setPen(QPen(color));
    }
}

void FixedDataPlotWidget::setXAxisLabel(const QString& text)
{
    if (m_plot) {
        m_plot->xAxis->setLabel(text);
    }
}

void FixedDataPlotWidget::setYAxisLabel(const QString& text)
{
    if (m_plot) {
        m_plot->yAxis->setLabel(text);
    }
}

void FixedDataPlotWidget::setXRange(double min, double max)
{
    if (m_plot) {
        m_plot->xAxis->setRange(min,max);
    }
}

void FixedDataPlotWidget::setData(const QVector<double>& x, const QVector<double>& y)
{
    if (m_plot) {
        m_xData = x;
        m_yData = y;
        m_graph->setData(m_xData, m_yData);
    }
}

void FixedDataPlotWidget::clearData()
{
    if (m_plot) {
        m_graph->data()->clear();
    }
    m_xData.clear();
    m_yData.clear();
}

void FixedDataPlotWidget::refreshPlot()
{
    if (m_plot) {
        m_plot->replot();
    }
}
