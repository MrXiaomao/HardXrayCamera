#ifndef QGOODWINDOWHELPER_H
#define QGOODWINDOWHELPER_H

#ifdef Q_OS_WIN
#include <objidl.h>  // 包含 FORMATETC、STGMEDIUM 定义
#include <oleidl.h>  // 包含 IDataObject 定义
#endif //Q_OS_WIN

#include <QGoodWindow>
#include <QGoodCentralWidget>

class MenuIconLayoutFixer : public QObject
{
    Q_OBJECT
public:
    // 全局一键修复，传入主窗口的menuBar指针即可
    static void fixAllMenuIconLayout(QMenuBar* menuBar)
    {
        if (!menuBar) return;

        // 遍历菜单栏所有顶层菜单
        const auto topLevelMenus = menuBar->findChildren<QMenu*>("", Qt::FindChildOption::FindDirectChildrenOnly);
        for (QMenu* topMenu : topLevelMenus)
        {
            fixSingleMenuAndSubMenus(topMenu);
        }
    }

    static void fixSingleMenuAndSubMenus(QMenu* menu)
    {
        if (!menu) return;

        QFontMetrics fm = menu->fontMetrics();
        int maxRequiredWidth = 0;

        // 遍历当前菜单下所有action，计算图标+文字总占用宽度
        bool existsIcon = false;
        int iconWidth = 0;
        for (QAction* action : menu->actions())
        {
            if (action->isSeparator()) continue;

            QString actionText = action->text();
            // 计算当前文字的像素宽度，Qt6推荐用horizontalAdvance替代旧的width()
            int textWidth = fm.horizontalAdvance(actionText);

            // 预留边距余量：图标区域+文字+左右padding的安全距离
            int totalActWidth = textWidth + 40;
            if (!action->icon().isNull())
            {
                existsIcon = true;
                // 计算图标实际渲染宽度，最大支持24px标准菜单图标
                iconWidth = qMax(iconWidth, action->icon().actualSize(QSize(24,24)).width());
            }
            maxRequiredWidth = qMax(maxRequiredWidth, totalActWidth);

            // 如果这个action本身带下拉子菜单，递归进去修复
            if (action->menu())
            {
                fixSingleMenuAndSubMenus(action->menu());
            }
        }

        if (existsIcon){
            maxRequiredWidth += iconWidth;
        }

        // 只在计算出的宽度大于当前菜单最小宽度时更新，避免撑太小的菜单
        if (maxRequiredWidth > menu->minimumWidth())
        {
            menu->setMinimumWidth(maxRequiredWidth);
        }
    }
    static void fixSingleMenuBar(QMenuBar* menu)
    {
        if (!menu) return;

        QFontMetrics fm = menu->fontMetrics();
        int maxRequiredWidth = 0;

        // 遍历当前菜单下所有action，计算图标+文字总占用宽度
        for (QAction* action : menu->actions())
        {
            if (action->isSeparator()) continue;

            // 计算图标实际渲染宽度，最大支持24px标准菜单图标
            int iconWidth = action->icon().actualSize(QSize(24,24)).width();
            QString actionText = action->text();
            // 计算当前文字的像素宽度，Qt6推荐用horizontalAdvance替代旧的width()
            int textWidth = fm.horizontalAdvance(actionText);

            // 预留边距余量：图标区域+文字+左右padding的安全距离
            int totalActWidth = iconWidth + textWidth + 40;
            maxRequiredWidth = qMax(maxRequiredWidth, totalActWidth);

            // 如果这个action本身带下拉子菜单，递归进去修复
            if (action->menu())
            {
                fixSingleMenuAndSubMenus(action->menu());
            }
        }

        // 只在计算出的宽度大于当前菜单最小宽度时更新，避免撑太小的菜单
        if (maxRequiredWidth > menu->minimumWidth())
        {
            menu->setMinimumWidth(maxRequiredWidth * 2);
        }
    }
};

class QGoodWindowHelper : public QGoodWindow
{
    Q_OBJECT
public:
    explicit QGoodWindowHelper(QWidget *parent = nullptr);
    ~QGoodWindowHelper();
    void setupUiHelper(QMainWindow*);
    void setupUiHelper(QMainWindow*, bool isDarkTheme);

    void fixMenuBarWidth(void) {
        if (mMenuBar) {
            int width = 0;
            int itemSpacingPx = mMenuBar->style()->pixelMetric(QStyle::PM_MenuBarItemSpacing);
            for (int i = 0; i < mMenuBar->actions().size(); i++) {
                QString text = mMenuBar->actions().at(i)->text();
                QFontMetrics fm(mMenuBar->font());
                width += fm.size(0, text).width() + itemSpacingPx*1.5;
            }

            mGoodCentraWidget->setLeftTitleBarWidth(width * 1.2);
            MenuIconLayoutFixer::fixSingleMenuBar(mMenuBar);
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
