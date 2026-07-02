#include "qgoodwindowhelper.h"

QGoodWindowHelper::QGoodWindowHelper(QWidget *parent)
    : QGoodWindow(parent) {

}

void QGoodWindowHelper::setupUiHelper(QMainWindow* centralWidget, bool isDarkTheme)
{
    mCentralWidget = centralWidget;
    mCentralWidget->setWindowFlags(Qt::Widget);
    mGoodCentraWidget = new QGoodCentralWidget(this);

#ifdef Q_OS_MAC
    //macOS uses global menu bar
    if(QApplication::testAttribute(Qt::AA_DontUseNativeMenuBar)) {
#else
    if(true) {
#endif
        mMenuBar = mCentralWidget->menuBar();
        if (mMenuBar)
        {
            //Set font of menu bar
            QFont font = mMenuBar->font();
#ifdef Q_OS_WIN
            font.setFamily("Segoe UI");
#else
            font.setFamily(qApp->font().family());
#endif
            mMenuBar->setFont(font);

            QTimer::singleShot(0, this, [&]{
                const int title_bar_height = mGoodCentraWidget->titleBarHeight();
                mMenuBar->setStyleSheet(QString("QMenuBar {height: %0px;}").arg(title_bar_height));
            });

            connect(mGoodCentraWidget,&QGoodCentralWidget::windowActiveChanged,this, [&](bool active){
                mMenuBar->setEnabled(active);
            });

            mGoodCentraWidget->setLeftTitleBarWidget(mMenuBar);
        }
    }

    connect(qGoodStateHolder, &QGoodStateHolder::currentThemeChanged, this, [](){
        if (qGoodStateHolder->isCurrentThemeDark())
            QGoodWindow::setAppDarkTheme();
        else
            QGoodWindow::setAppLightTheme();
    });
    connect(this, &QGoodWindow::systemThemeChanged, this, [&]{
        qGoodStateHolder->setCurrentThemeDark(QGoodWindow::isSystemThemeDark());
    });
    qGoodStateHolder->setCurrentThemeDark(isDarkTheme);

    mGoodCentraWidget->setCentralWidget(mCentralWidget);
    setCentralWidget(mGoodCentraWidget);

    setWindowIcon(mCentralWidget->windowIcon());
    setWindowTitle(mCentralWidget->windowTitle());

    mGoodCentraWidget->setTitleAlignment(Qt::AlignCenter);
}

QGoodWindowHelper::~QGoodWindowHelper()
{
    delete mCentralWidget;
}

void QGoodWindowHelper::closeEvent(QCloseEvent *event)
{
    QApplication::sendEvent(mCentralWidget, event);
}

bool QGoodWindowHelper::event(QEvent * event)
{
    if(event->type() == QEvent::StatusTip) {
        return QApplication::sendEvent(mCentralWidget, event);
    }

    return QGoodWindow::event(event);
}
