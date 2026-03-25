/*
 * @Author: MrPan
 * @Date: 2026-03-23 16:14:09
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-03-23 17:28:45
 * @Description: 请填写简介
 */
#include "trendplotwidget.h"
#include "qcustomplot.h"
#include <QDateTime>
#include <QSizePolicy>
#include <QVBoxLayout>

TrendPlotWidget::TrendPlotWidget(QWidget *parent)
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
    QSharedPointer<QCPAxisTickerDateTime> timeTicker(new QCPAxisTickerDateTime);
    timeTicker->setDateTimeFormat("hh:mm:ss");
    timeTicker->setDateTimeSpec(Qt::LocalTime);
    m_plot->xAxis->setTicker(timeTicker);

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

TrendPlotWidget::~TrendPlotWidget()
{
    delete m_plot;
}

void TrendPlotWidget::setTitle(const QString& title)
{
    if (!m_title) {
        m_plot->plotLayout()->insertRow(0);
        m_title = new QCPTextElement(m_plot, title, QFont("微软雅黑", 12, QFont::Bold));
        m_plot->plotLayout()->addElement(0, 0, m_title);
    } else {
        m_title->setText(title);
    }
}

void TrendPlotWidget::setGraphColor(const QColor& color)
{
    if (m_graph) {
        m_graph->setPen(QPen(color));
    }
}

void TrendPlotWidget::setXAxisLabel(const QString& text)
{
    if(m_plot)
    {
        m_plot->xAxis->setLabel(text);
    }
}

void TrendPlotWidget::setYAxisLabel(const QString& text)
{
    if(m_plot)
    {
        m_plot->yAxis->setLabel(text);
    }
}

void TrendPlotWidget::setTimeWindow(int seconds)
{
    if (seconds <= 0 || !m_plot) {
        return;
    }

    m_timeWindowSeconds = seconds;
    const double now = QDateTime::currentDateTime().toSecsSinceEpoch();
    m_plot->xAxis->setRange(now - m_timeWindowSeconds, now);
}

void TrendPlotWidget::appendPoint(double x, double y)
{
    Q_UNUSED(x);

    const double now = QDateTime::currentDateTime().toSecsSinceEpoch();
    m_xData.append(now);
    m_yData.append(y);
    m_graph->addData(now, y);
    m_graph->data()->removeBefore(now - m_timeWindowSeconds);
    m_plot->xAxis->setRange(now - m_timeWindowSeconds, now);
}

void TrendPlotWidget::clearData()
{
    if(m_plot)
    {
        m_graph->data()->clear();
    }
    m_xData.clear();
    m_yData.clear();
}

void TrendPlotWidget::refreshPlot()
{
    if(m_plot)
    {
        m_plot->replot();
    }
}
