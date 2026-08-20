/*
 * @Author: MrPan
 * @Date: 2026-03-23 16:14:09
 * @LastEditors: Maoxiaoqing
 * @LastEditTime: 2026-03-23 17:28:45
 * @Description: 请填写简介
 */
#include "trendplotwidget.h"
#include <QDateTime>
#include <QSizePolicy>
#include <QVBoxLayout>

TrendPlotWidget::TrendPlotWidget(QWidget *parent)
    : QWidget{parent}
{
    m_plot = new QCustomPlot(this);
    m_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_plot->legend->setVisible(false);
    QCustomPlotHelper* customPlotHelper = new QCustomPlotHelper(m_plot, this);
    (void)customPlotHelper;

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
    m_plot->setAutoAddPlottableToLegend(false);

    //创建一条曲线
    m_graph = m_plot->addGraph();
    styleGraph(m_graph, 0);
}

void TrendPlotWidget::styleGraph(QCPGraph *graph, int colorIndex)
{
    static const QVector<QColor> kColors = {
        Qt::black, Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta
    };

    graph->setPen(QPen(kColors.at(colorIndex % kColors.size())));
    graph->setLineStyle(QCPGraph::lsLine);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));
    graph->setBrush(QBrush(QColor(0, 0, 255, 20)));
    graph->setAntialiased(true);
    graph->setAntialiasedFill(true);
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

void TrendPlotWidget::setGraphName(int graphIndex, const QString& name)
{
    if (!m_plot || graphIndex < 0 || graphIndex >= m_plot->graphCount())
        return;
    m_plot->graph(graphIndex)->setName(name);
}

void TrendPlotWidget::setLegendVisible(bool visible)
{
    if (m_plot && m_plot->legend)
        m_plot->legend->setVisible(visible);
}

void TrendPlotWidget::setupNumericLegend()
{
    if (!m_plot || !m_plot->legend)
        return;

    m_plot->legend->clearItems();
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph *graph = m_plot->graph(i);
        graph->setName(QString::number(i + 1));
        graph->addToLegend();
    }

    setLegendVisible(m_plot->graphCount() > 0);
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

void TrendPlotWidget::appendPoint(double x, double y, int graphIndex)
{
    Q_UNUSED(x);

    const double now = QDateTime::currentDateTime().toSecsSinceEpoch();
    m_plot->graph(graphIndex)->addData(now, y);
    m_plot->graph(graphIndex)->data()->removeBefore(now - m_timeWindowSeconds);
    m_plot->xAxis->setRange(now - m_timeWindowSeconds, now);
}

void TrendPlotWidget::appendPoints(double x, const QVector<double>& y)
{
    Q_UNUSED(x);

    const double now = QDateTime::currentDateTime().toSecsSinceEpoch();
    const int graphCount = qMin(y.size(), m_plot->graphCount());
    for (int i = 0; i < graphCount; ++i)
    {
        m_plot->graph(i)->addData(now, y[i]);
        m_plot->graph(i)->data()->removeBefore(now - m_timeWindowSeconds);
    }

    m_plot->xAxis->setRange(now - m_timeWindowSeconds, now);
}

void TrendPlotWidget::clearData()
{
    if(m_plot)
    {
        for (int i=0; i<m_plot->graphCount(); ++i) {
            m_plot->graph(i)->data().clear();
        }
    }
}

void TrendPlotWidget::refreshPlot()
{
    if (!m_plot)
        return;

    // Y 轴只按当前 X 轴时间窗口内的可见点计算，不考虑已滑出屏幕的历史峰值
    bool foundRange = false;
    QCPRange yRange;
    const QCPRange xRange = m_plot->xAxis->range();
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        bool found = false;
        const QCPRange r = m_plot->graph(i)->getValueRange(found, QCP::sdBoth, xRange);
        if (found) {
            yRange = foundRange ? yRange.expanded(r) : r;
            foundRange = true;
        }
    }

    if (foundRange) {
        const double yMax = qMax(yRange.upper, 0.0);
        m_plot->yAxis->setRange(0, (yMax > 0.0 ? yMax : 1.0) * 1.2);
    } else {
        m_plot->yAxis->setRange(0, 1.0);
    }
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void TrendPlotWidget::ensureGraphCount(int count)
{
    if (!m_plot || count <= 0)
        return;

    while (m_plot->graphCount() < count) {
        QCPGraph *graph = m_plot->addGraph();
        styleGraph(graph, m_plot->graphCount() - 1);
    }
    while (m_plot->graphCount() > count)
        m_plot->removeGraph(m_plot->graph(m_plot->graphCount() - 1));

    m_graph = m_plot->graph(0);
}

void TrendPlotWidget::addGraph(int count)
{
    if (!m_plot || count <= 0)
        return;

    ensureGraphCount(m_plot->graphCount() + count);
}
