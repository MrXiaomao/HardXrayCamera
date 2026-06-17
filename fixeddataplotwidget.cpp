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
    ensureGraphCount(1);
}

FixedDataPlotWidget::~FixedDataPlotWidget()
{
    delete m_plot;
}

void FixedDataPlotWidget::setTitle(const QString& title)
{
    if (!m_title) {
        m_plot->plotLayout()->insertRow(1);
        m_title = new QCPTextElement(m_plot, title, QFont("微软雅黑", 12, QFont::Bold));
        m_title->setTextFlags(Qt::AlignCenter);
        m_plot->plotLayout()->addElement(1, 0, m_title);
    } else {
        m_title->setText(title);
    }
}

void FixedDataPlotWidget::setGraphColor(const QColor& color)
{
    setGraphColor(0, color);
}

void FixedDataPlotWidget::setGraphColor(int graphIndex, const QColor& color)
{
    if (graphIndex >= 0 && graphIndex < m_graphs.size() && m_graphs.at(graphIndex))
        m_graphs.at(graphIndex)->setPen(QPen(color));
}

void FixedDataPlotWidget::setGraphName(int graphIndex, const QString& name)
{
    if (graphIndex >= 0 && graphIndex < m_graphs.size() && m_graphs.at(graphIndex))
        m_graphs.at(graphIndex)->setName(name);
}

void FixedDataPlotWidget::setLegendVisible(bool visible)
{
    if (m_plot && m_plot->legend)
        m_plot->legend->setVisible(visible);
}

void FixedDataPlotWidget::ensureGraphCount(int count)
{
    if (count <= 0)
        return;

    static const QVector<QColor> kDefaultColors = {
        Qt::black, Qt::red, Qt::blue, Qt::green, Qt::magenta, Qt::cyan
    };

    while (m_graphs.size() < count) {
        QCPGraph *graph = m_plot->addGraph();
        const QColor color = kDefaultColors.at(m_graphs.size() % kDefaultColors.size());
        graph->setPen(QPen(color));
        graph->setLineStyle(QCPGraph::lsLine);
        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));
        graph->setBrush(Qt::NoBrush);
        graph->setAntialiased(true);
        m_graphs.append(graph);
    }

    m_graph = m_graphs.first();
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

void FixedDataPlotWidget::setData(const QVector<quint32>& x, const QVector<quint32>& y)
{
    if (m_plot) {
        m_xData.resize(x.size());
        for (int i = 0; i < x.size(); ++i) {
            m_xData[i] = static_cast<double>(x[i]);
        }

        m_yData.resize(y.size());
        for (int i = 0; i < y.size(); ++i) {
            m_yData[i] = static_cast<double>(y[i]);
        }

        m_graph->setData(m_xData, m_yData);
    }
}

void FixedDataPlotWidget::setGraphData(int graphIndex, const QVector<double>& x, const QVector<double>& y)
{
    if (graphIndex < 0 || graphIndex >= m_graphs.size())
        return;

    m_graphs.at(graphIndex)->setData(x, y);
}

void FixedDataPlotWidget::clearData()
{
    clearAllGraphData();
}

void FixedDataPlotWidget::clearAllGraphData()
{
    for (QCPGraph *graph : m_graphs) {
        if (graph)
            graph->data()->clear();
    }
    m_xData.clear();
    m_yData.clear();
}

void FixedDataPlotWidget::refreshPlot(bool rescaleX, bool rescaleY)
{
    if (m_plot) {
        if (rescaleX)
            m_plot->xAxis->rescale(true);
        if (rescaleY)
            m_plot->yAxis->rescale(true);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}
