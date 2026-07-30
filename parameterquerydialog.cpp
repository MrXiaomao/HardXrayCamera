#include "parameterquerydialog.h"
#include "ui_parameterquerydialog.h"

ParameterQueryDialog::ParameterQueryDialog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ParameterQueryDialog)
{
    ui->setupUi(this);
    initTableWidget();

    commandHelper = CommandHelper::instance();

    // 参数查询
    connect(commandHelper, &CommandHelper::sigSpectrumRefreshTimelengthAck, this, [=](quint8 index, quint16 v){
        if (index==1){
            ui->spb_specRefashTime->setValue(v);
        }
        else{
            ui->spb_specRefashTime_2->setValue(v);
        }
    }, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigSpecSpectrumTriggerThresholdAck, this, [=](quint8 index, quint16 v){
        if (index==1){
            ui->spb_threshold->setValue(v);
        }
        else{
            ui->spb_threshold_2->setValue(v);
        }
    }, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigSpecSpectrumDieTimelengthAck, this, [=](quint8 index, quint16 v){
        if (index==1){
            ui->spb_deadTime->setValue(v);
        }
        else{
            ui->spb_deadTime_2->setValue(v);
        }
    }, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigSpecTriggerSignalTimeWidthAck, this, [=](quint8 index, quint16 v){
        if (index==1){
            ui->spinBox_timeWidth->setValue(v);
        }
        else{
            ui->spinBox_timeWidth_2->setValue(v);
        }
    }, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigSpecSpectrumTimeWindowAck, this, [=](quint8 index, quint8 commandIndex, quint16 e1, quint16 e2){
        quint8 channel = commandIndex / 9;
        quint8 col = commandIndex % 9;
        quint8 row = channel + (index-1)*16;

        ui->tableWidget->item(row, col*2 + 1)->setText(QString::number(e1));
        if (col != 8)
            ui->tableWidget->item(row, col*2 + 2)->setText(QString::number(e2));
    }, Qt::QueuedConnection);
    connect(commandHelper, &CommandHelper::sigOTAVersionAck, this, [=](quint8 index, quint8 v){
        if (index == 1)
            ui->lineEdit_ver->setText(QString::number(v, 16));
        else
            ui->lineEdit_ver_2->setText(QString::number(v, 16));
    }, Qt::QueuedConnection);
}

ParameterQueryDialog::~ParameterQueryDialog()
{
    delete ui;
}

void ParameterQueryDialog::initTableWidget()
{
    // 1. 基础尺寸初始化
    ui->tableWidget->setRowCount(32);
    ui->tableWidget->setColumnCount(18);

    // 2. 行标题设置：CH1~CH32
    for(int row = 0; row < 32; row++){
        ui->tableWidget->setVerticalHeaderItem(row, new QTableWidgetItem(QString("CH%1").arg(row%16+1)));
    }

    // 3. 列标题设置：Boundary1~Boundary17，第18列留空对应业务扩展
    for(int col = 0; col < 17; col++){
        ui->tableWidget->setHorizontalHeaderItem(col+1, new QTableWidgetItem(QString("Boundary%1").arg(col+1)));
    }
    // 隐藏第0列默认的原生表头，让合并区域更整洁
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("channel"));

    // 4. 合并单元格设置
    // 第一列 第0行~第15行 合并为水平相机
    ui->tableWidget->setSpan(0, 0, 16, 1);
    QTableWidgetItem* horizontalCamItem = new QTableWidgetItem("水平相机");
    horizontalCamItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(0, 0, horizontalCamItem);

    // 第一列 第16行~第31行 合并为垂直相机
    ui->tableWidget->setSpan(16, 0, 16, 1);
    QTableWidgetItem* verticalCamItem = new QTableWidgetItem("垂直相机");
    verticalCamItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(16, 0, verticalCamItem);

    // 5. 所有单元格默认赋值0 + 全局居中
    for(int row = 0; row < 32; row++){
        // 跳过第0列已经手动设置合并项的两行
        if(row != 0 && row !=16){
            QTableWidgetItem* emptyItem = new QTableWidgetItem();
            emptyItem->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, 0, emptyItem);
        }
        // 遍历1~17列，全部设置默认值0且居中
        for(int col = 1; col < 18; col++){
            QTableWidgetItem* defaultItem = new QTableWidgetItem("0");
            defaultItem->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, col, defaultItem);
        }
    }

    // 6. 可选优化（适配之前你处理大表格不卡顿的优化经验）
    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->resizeRowsToContents();
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers); // 按需开启不可编辑模式
}

void ParameterQueryDialog::on_pushButton_query_clicked()
{
    commandHelper->queryStart();
}

void ParameterQueryDialog::on_pushButton_close_clicked()
{
    this->close();
}

void ParameterQueryDialog::closeEvent(QCloseEvent*)
{
    commandHelper->queryEnd();
}

// 核心复制函数
#include <QClipboard>
void copyTableToClipboard(QTableWidget* table, bool onlySelected = true)
{
    if (!table) return;

    QString clipboardText;
    int rowCount = table->rowCount();
    int colCount = table->columnCount();

    // 获取选中区域的有效行/列范围，或者全表范围
    int startRow = 0, endRow = rowCount - 1;
    int startCol = 1, endCol = colCount - 1;
    auto selectedRanges = table->selectedRanges();

    if (onlySelected && !selectedRanges.isEmpty()) {
        // 取所有选中区域的边界范围
        startRow = selectedRanges[0].topRow();
        endRow = selectedRanges[0].bottomRow();
        startCol = selectedRanges[0].leftColumn();
        endCol = selectedRanges[0].rightColumn();
    }

    // 先拼列表头行
    QStringList headerRow;
    headerRow.append(QStringLiteral("channel"));
    for (int j = startCol; j <= endCol; j++) {
        if (table->isColumnHidden(j)) continue;
        headerRow.append(table->horizontalHeaderItem(j)->text());
    }
    clipboardText.append(headerRow.join('\t') + '\n');

    // 再拼接原有数据行...

    // 逐行逐单元格拼接制表符分隔的文本
    for (int i = startRow; i <= endRow; i++) {
        // 跳过隐藏行
        if (table->isRowHidden(i)) continue;

        QStringList rowData;
        rowData.append(QString::number(i+1));
        for (int j = startCol; j <= endCol; j++) {
            // 跳过隐藏列
            if (table->isColumnHidden(j)) continue;

            // 取出单元格文本，空单元格也保留占位保证列对齐
            QTableWidgetItem* item = table->item(i, j);
            QString cellText = item ? item->text() : QString();
            // 如果单元格本身含制表符，替换为空格避免Excel列错位
            rowData.append(cellText.replace('\t', ' '));
        }

        // 行之间用换行符拼接
        clipboardText.append(rowData.join('\t') + '\n');
    }

    // 把内容写入系统剪贴板
    QApplication::clipboard()->setText(clipboardText);
}

void ParameterQueryDialog::on_pushButton_copy_clicked()
{
    // 仅复制用户选中的单元格区域
    //copyTableToClipboard(ui->tableWidget, true);

    // 如果需要直接复制整个表格的所有内容，把第二个参数改成false即可
    copyTableToClipboard(ui->tableWidget, false);
}
