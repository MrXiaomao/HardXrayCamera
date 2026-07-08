QT       += core gui widgets network

# QCustomPlot所需
QT       += printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    commandhelper.cpp \
    otaupgradewindow.cpp \
    signalwidthsetting.cpp \
    udpshotreceiver.cpp \
    detectorsetting.cpp \
    fixeddataplotwidget.cpp \
    globalsettings.cpp \
    ipaddress.cpp \
    main.cpp \
    mainwindow.cpp \
    offlinewindow.cpp \
    order.cpp \
    qcustomplot.cpp \
    switchbutton.cpp \
    tcpclient.cpp \
    trendplotwidget.cpp

HEADERS += \
    commandhelper.h \
    otaupgradewindow.h \
    signalwidthsetting.h \
    udpshotreceiver.h \
    detectorsetting.h \
    fixeddataplotwidget.h \
    globalsettings.h \
    ipaddress.h \
    mainwindow.h \
    offlinewindow.h \
    order.h \
    qcustomplot.h \
    switchbutton.h \
    tcpclient.h \
    trendplotwidget.h

FORMS += \
    detectorsetting.ui \
    mainwindow.ui \
    offlinewindow.ui \
    otaupgradewindow.ui \
    signalwidthsetting.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DESTDIR = $$PWD/../build_HardXrayCamera
contains(QT_ARCH, x86_64) {
    # x64
    DESTDIR = $$DESTDIR/x64
} else {
    # x86
    DESTDIR = $$DESTDIR/x86
}


# 获取QT版本
#############################################################################################################
exists (./.git) {
    GIT_BRANCH   = $$system(git rev-parse --abbrev-ref HEAD)
    GIT_DATE = $$system(git show --oneline --format=\"%ci\" -s HEAD)
    GIT_HASH     = $$system(git show --oneline --format=\"%H\" -s HEAD)
    GIT_VERSION = "Git: $${GIT_BRANCH}: $${GIT_DATE} $${GIT_HASH}"
} else {
    GIT_BRANCH      = None
    GIT_DATE        = None
    GIT_HASH        = None
    GIT_VERSION     = None
}

# git 日期含空格时不能直接进 -D，否则 MSVC/clangd 会拆成多个宏（问题项里 Expected ')'）
GIT_DATE ~= s/ /_/g
GIT_DATE ~= s/:/-/g
GIT_DATE ~= s/+//g
GIT_VERSION ~= s/ /_/g
GIT_VERSION ~= s/:/-/g
GIT_VERSION ~= s/+//g

DEFINES += GIT_BRANCH=\"\\\"$$GIT_BRANCH\\\"\"
DEFINES += GIT_DATE=\"\\\"$$GIT_DATE\\\"\"
DEFINES += GIT_HASH=\"\\\"$$GIT_HASH\\\"\"
DEFINES += GIT_VERSION=\"\\\"$$GIT_VERSION\\\"\"
DEFINES += APP_VERSION="\\\"V2.0.0\\\""

message(GIT_BRANCH":  ""$$GIT_BRANCH")
message(GIT_TIME":  ""$$GIT_TIME")
message(APP_VERSION":  ""$$APP_VERSION")

# DESTDIR = $$DESTDIR/qt$$QT_VERSION/
message(DESTDIR = $$DESTDIR)

TARGET = HardXrayCamera

DISTFILES +=

# 图标资源
RESOURCES += \
    resource.qrc

RC_ICONS = $$PWD/resource/LOGO.ico

#把所有警告都关掉眼不见为净
# CONFIG += warn_off

# 第三方库
include($$PWD/log4qt/Include/log4qt.pri)
include($$PWD/QGoodWindow/QGoodWindowHelper/QGoodWindowHelper.pri)
