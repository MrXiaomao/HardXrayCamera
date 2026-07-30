#ifndef PARAMETERQUERYDIALOG_H
#define PARAMETERQUERYDIALOG_H

#include <QWidget>
#include "commandhelper.h"

namespace Ui {
class ParameterQueryDialog;
}

class ParameterQueryDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterQueryDialog(QWidget *parent = nullptr);
    ~ParameterQueryDialog();

    void initTableWidget();

    virtual void closeEvent(QCloseEvent*) override;

private slots:
    void on_pushButton_query_clicked();

    void on_pushButton_close_clicked();

    void on_pushButton_copy_clicked();

signals:
    // 参数查询
    void sigSpectrumRefreshTimelengthAck(quint8, quint16);
    void sigSpecSpectrumTriggerThresholdAck(quint8, quint16);
    void sigSpecSpectrumDieTimelengthAck(quint8, quint16);
    void sigSpecTriggerSignalTimeWidthAck(quint8, quint16);
    void sigSpecSpectrumTimeWindowAck(quint8, quint8, quint16, quint16);

private:
    Ui::ParameterQueryDialog *ui;
    CommandHelper* commandHelper;
};

#endif // PARAMETERQUERYDIALOG_H
