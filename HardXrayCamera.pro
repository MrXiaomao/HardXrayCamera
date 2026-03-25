QT       += core gui widgets network

# QCustomPlot所需
QT       += printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    detectorsetting.cpp \
    fixeddataplotwidget.cpp \
    globalsettings.cpp \
    ipaddress.cpp \
    main.cpp \
    mainwindow.cpp \
    offlinewindow.cpp \
    qcustomplot.cpp \
    trendplotwidget.cpp

HEADERS += \
    detectorsetting.h \
    fixeddataplotwidget.h \
    globalsettings.h \
    ipaddress.h \
    mainwindow.h \
    offlinewindow.h \
    qcustomplot.h \
    trendplotwidget.h

FORMS += \
    detectorsetting.ui \
    mainwindow.ui \
    offlinewindow.ui

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

# DESTDIR = $$DESTDIR/qt$$QT_VERSION/
message(DESTDIR = $$DESTDIR)

TARGET = HardXrayCamera

DISTFILES +=

RESOURCES += \
    resource.qrc
