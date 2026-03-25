#ifndef OFFLINEWINDOW_H
#define OFFLINEWINDOW_H

#include <QMainWindow>

namespace Ui {
class offlineWindow;
}

class offlineWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit offlineWindow(QWidget *parent = nullptr);
    ~offlineWindow();

private:
    Ui::offlineWindow *ui;
};

#endif // OFFLINEWINDOW_H
