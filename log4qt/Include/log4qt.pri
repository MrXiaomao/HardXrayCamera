LOG4QTSRCPATH = $$PWD
INCLUDEPATH += -L $$LOG4QTSRCPATH \
           $$LOG4QTSRCPATH/log4qt \
           $$LOG4QTSRCPATH/log4qt/helpers \
           $$LOG4QTSRCPATH/log4qt/spi \
           $$LOG4QTSRCPATH/log4qt/varia

DEPENDPATH  +=  $$LOG4QTSRCPATH \
           $$LOG4QTSRCPATH/log4qt/helpers \
           $$LOG4QTSRCPATH/log4qt/spi \
           $$LOG4QTSRCPATH/log4qt/varia

HEADERS_BASE += \
           $$PWD/log4qt/appender.h \
           $$PWD/log4qt/appenderskeleton.h \
           $$PWD/log4qt/asyncappender.h \
           $$PWD/log4qt/basicconfigurator.h \
           $$PWD/log4qt/binaryfileappender.h \
           $$PWD/log4qt/binarylayout.h \
           $$PWD/log4qt/binarylogger.h \
           $$PWD/log4qt/binaryloggingevent.h \
           $$PWD/log4qt/binarylogstream.h \
           $$PWD/log4qt/binarytotextlayout.h \
           $$PWD/log4qt/binarywriterappender.h \
           $$PWD/log4qt/consoleappender.h \
           $$PWD/log4qt/dailyfileappender.h \
           $$PWD/log4qt/dailyrollingfileappender.h \
           $$PWD/log4qt/fileappender.h \
           $$PWD/log4qt/hierarchy.h \
           $$PWD/log4qt/layout.h \
           $$PWD/log4qt/level.h \
           $$PWD/log4qt/log4qt.h \
           $$PWD/log4qt/log4qtdefs.h \
           $$PWD/log4qt/log4qtshared.h \
           $$PWD/log4qt/log4qtsharedptr.h \
           $$PWD/log4qt/logger.h \
           $$PWD/log4qt/loggerrepository.h \
           $$PWD/log4qt/loggingevent.h \
           $$PWD/log4qt/logmanager.h \
           $$PWD/log4qt/logstream.h \
           $$PWD/log4qt/mainthreadappender.h \
           $$PWD/log4qt/mdc.h \
           $$PWD/log4qt/ndc.h \
           $$PWD/log4qt/patternlayout.h \
           $$PWD/log4qt/propertyconfigurator.h \
           $$PWD/log4qt/rollingbinaryfileappender.h \
           $$PWD/log4qt/rollingfileappender.h \
           $$PWD/log4qt/signalappender.h \
           $$PWD/log4qt/simplelayout.h \
           $$PWD/log4qt/simpletimelayout.h \
           $$PWD/log4qt/systemlogappender.h \
           $$PWD/log4qt/ttcclayout.h \
           $$PWD/log4qt/writerappender.h \
           $$PWD/log4qt/xmllayout.h
HEADERS_HELPERS += \
           $$PWD/log4qt/helpers/appenderattachable.h \
           $$PWD/log4qt/helpers/binaryclasslogger.h \
           $$PWD/log4qt/helpers/classlogger.h \
           $$PWD/log4qt/helpers/configuratorhelper.h \
           $$PWD/log4qt/helpers/datetime.h \
           $$PWD/log4qt/helpers/dispatcher.h \
           $$PWD/log4qt/helpers/factory.h \
           $$PWD/log4qt/helpers/initialisationhelper.h \
           $$PWD/log4qt/helpers/logerror.h \
           $$PWD/log4qt/helpers/optionconverter.h \
           $$PWD/log4qt/helpers/patternformatter.h \
           $$PWD/log4qt/helpers/properties.h
HEADERS_SPI += \
           $$PWD/log4qt/spi/filter.h
HEADERS_VARIA += \
           $$PWD/log4qt/varia/binaryeventfilter.h \
           $$PWD/log4qt/varia/debugappender.h \
           $$PWD/log4qt/varia/denyallfilter.h \
           $$PWD/log4qt/varia/levelmatchfilter.h \
           $$PWD/log4qt/varia/levelrangefilter.h \
           $$PWD/log4qt/varia/listappender.h \
           $$PWD/log4qt/varia/nullappender.h \
           $$PWD/log4qt/varia/stringmatchfilter.h
SOURCES += $$PWD/log4qt/appender.cpp \
           $$PWD/log4qt/appenderskeleton.cpp \
           $$PWD/log4qt/basicconfigurator.cpp \
           $$PWD/log4qt/consoleappender.cpp \
           $$PWD/log4qt/dailyrollingfileappender.cpp \
           $$PWD/log4qt/asyncappender.cpp \
           $$PWD/log4qt/dailyfileappender.cpp \
           $$PWD/log4qt/mainthreadappender.cpp \
           $$PWD/log4qt/fileappender.cpp \
           $$PWD/log4qt/hierarchy.cpp \
           $$PWD/log4qt/layout.cpp \
           $$PWD/log4qt/level.cpp \
           $$PWD/log4qt/logger.cpp \
           $$PWD/log4qt/loggerrepository.cpp \
           $$PWD/log4qt/loggingevent.cpp \
           $$PWD/log4qt/logmanager.cpp \
           $$PWD/log4qt/mdc.cpp \
           $$PWD/log4qt/ndc.cpp \
           $$PWD/log4qt/patternlayout.cpp \
           $$PWD/log4qt/propertyconfigurator.cpp \
           $$PWD/log4qt/rollingfileappender.cpp \
           $$PWD/log4qt/signalappender.cpp \
           $$PWD/log4qt/simplelayout.cpp \
           $$PWD/log4qt/simpletimelayout.cpp \
           $$PWD/log4qt/ttcclayout.cpp \
           $$PWD/log4qt/writerappender.cpp \
           $$PWD/log4qt/systemlogappender.cpp \
           $$PWD/log4qt/helpers/classlogger.cpp \
           $$PWD/log4qt/helpers/appenderattachable.cpp \
           $$PWD/log4qt/helpers/configuratorhelper.cpp \
           $$PWD/log4qt/helpers/datetime.cpp \
           $$PWD/log4qt/helpers/factory.cpp \
           $$PWD/log4qt/helpers/initialisationhelper.cpp \
           $$PWD/log4qt/helpers/logerror.cpp \
           $$PWD/log4qt/helpers/optionconverter.cpp \
           $$PWD/log4qt/helpers/patternformatter.cpp \
           $$PWD/log4qt/helpers/properties.cpp \
           $$PWD/log4qt/helpers/dispatcher.cpp \
           $$PWD/log4qt/spi/filter.cpp \
           $$PWD/log4qt/varia/debugappender.cpp \
           $$PWD/log4qt/varia/denyallfilter.cpp \
           $$PWD/log4qt/varia/levelmatchfilter.cpp \
           $$PWD/log4qt/varia/levelrangefilter.cpp \
           $$PWD/log4qt/varia/listappender.cpp \
           $$PWD/log4qt/varia/nullappender.cpp \
           $$PWD/log4qt/varia/stringmatchfilter.cpp \
           $$PWD/log4qt/logstream.cpp \
           $$PWD/log4qt/binaryloggingevent.cpp \
           $$PWD/log4qt/binarylogger.cpp \
           $$PWD/log4qt/varia/binaryeventfilter.cpp \
           $$PWD/log4qt/binarytotextlayout.cpp \
           $$PWD/log4qt/binarywriterappender.cpp \
           $$PWD/log4qt/binaryfileappender.cpp \
           $$PWD/log4qt/binarylogstream.cpp \
           $$PWD/log4qt/helpers/binaryclasslogger.cpp \
           $$PWD/log4qt/rollingbinaryfileappender.cpp \
           $$PWD/log4qt/binarylayout.cpp \
           $$PWD/log4qt/xmllayout.cpp

msvc {
    QMAKE_CXXFLAGS_WARN_ON -= -w34100
    QMAKE_CXXFLAGS += -wd4100
}

# add databaseappender and -layout if QT contains sql
build_with_db_logging {
    message("Including databaseappender and -layout")
    QT += sql
    DEFINES += LOG4QT_DB_LOGGING_SUPPORT

    HEADERS_BASE += \
        $$PWD/log4qt/databaseappender.h \
        $$PWD/log4qt/databaselayout.h

    SOURCES += \
        $$PWD/log4qt/databaseappender.cpp \
        $$PWD/log4qt/databaselayout.cpp
} else {
    message("Skipping databaseappender and -layout")
}

build_with_telnet_logging {
    message("Including telnetappender")
    QT += network
    DEFINES += LOG4QT_TELNET_LOGGING_SUPPORT

    HEADERS_BASE += \
        $$PWD/log4qt/telnetappender.h

    SOURCES += \
        $$PWD/log4qt/telnetappender.cpp
} else {
    message("Skipping telnetappender")
}

build_with_qml_logging {
    message("Including qml logger")
    QT += qml
    DEFINES += LOG4QT_QML_LOGGING_SUPPORT

    HEADERS_BASE += \
        $$PWD/log4qt/qmllogger.h

    SOURCES += \
        $$PWD/log4qt/qmllogger.cpp
} else {
    message("Skipping qml logger")
}

win32 {
    HEADERS_BASE+=$$PWD/log4qt/wdcappender.h
    SOURCES+=$$PWD/log4qt/wdcappender.cpp
    HEADERS_BASE+=$$PWD/log4qt/colorconsoleappender.h
    SOURCES+=$$PWD/log4qt/colorconsoleappender.cpp
}

HEADERS += $$HEADERS_BASE \
           $$HEADERS_HELPERS \
           $$HEADERS_SPI \
           $$HEADERS_VARIA

QT       += network concurrent
# # network :提供网络功能
# # concurrent :提供多线程和并发编程的支持(日志写入文本,肯定就会用到读写锁,来保证日志的线程安全)

DEFINES +=LOG4QT_STATIC
LOG4QT_VERSION_MAJOR = 1
LOG4QT_VERSION_MINOR = 6
LOG4QT_VERSION_PATCH = 0

DEFINES += LOG4QT_VERSION_MAJOR=$${LOG4QT_VERSION_MAJOR}
DEFINES += LOG4QT_VERSION_MINOR=$${LOG4QT_VERSION_MINOR}
DEFINES += LOG4QT_VERSION_PATCH=$${LOG4QT_VERSION_PATCH}
DEFINES += LOG4QT_VERSION_STR='\\"$${LOG4QT_VERSION_MAJOR}.$${LOG4QT_VERSION_MINOR}.$${LOG4QT_VERSION_PATCH}\\"'

#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00 //禁用 Qt 5.15 之前的所有废弃警告
#DEFINES += QT_DEPRECATED_WARNINGS //启用或禁用所有弃用警告
