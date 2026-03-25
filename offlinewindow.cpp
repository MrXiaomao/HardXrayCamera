#include "offlinewindow.h"
#include "ui_offlinewindow.h"

offlineWindow::offlineWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::offlineWindow)
{
    ui->setupUi(this);
}

offlineWindow::~offlineWindow()
{
    delete ui;
}
