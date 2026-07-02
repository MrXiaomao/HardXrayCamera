#ifndef QGOODWINDOWHELPER_H
#define QGOODWINDOWHELPER_H

#include <QGoodWindow>
#include <QGoodCentralWidget>

class QGoodWindowHelper : public QGoodWindow
{
    Q_OBJECT
public:
    explicit QGoodWindowHelper(QWidget *parent = nullptr);
    ~QGoodWindowHelper();
    void setupUiHelper(QMainWindow*, bool isDarkTheme = true);

    void fixMenuBarWidth(void) {
        if (mMenuBar) {
            int width = 0;
            int itemSpacingPx = mMenuBar->style()->pixelMetric(QStyle::PM_MenuBarItemSpacing);
            for (int i = 0; i < mMenuBar->actions().size(); i++) {
                QString text = mMenuBar->actions().at(i)->text();
                QFontMetrics fm(mMenuBar->font());
                width += fm.size(0, text).width() + itemSpacingPx*1.5;
            }
            mGoodCentraWidget->setLeftTitleBarWidth(width);
        }
    }

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent * event) override;

private:
    QGoodCentralWidget *mGoodCentraWidget;
    QMenuBar *mMenuBar = nullptr;
    QMainWindow *mCentralWidget;
};

#endif // QGOODWINDOWHELPER_H
