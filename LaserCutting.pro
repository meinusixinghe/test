QT       += core gui
# Modbus TCP
QT += serialbus serialport network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    connectiondialog.cpp \
    main.cpp \
    mainwindow.cpp \
    modbusmanager.cpp \
    pathplanner.cpp \
    pathplanningdialog.cpp \
    renderarea.cpp \
    rotationmatrixdialog.cpp \
    usercoordinatemanager.cpp \
    weldingprocessdialog.cpp

HEADERS += \
    connectiondialog.h \
    mainwindow.h \
    modbusmanager.h \
    pathplanner.h \
    pathplanningdialog.h \
    renderarea.h \
    rotationmatrixdialog.h \
    usercoordinatemanager.h \
    weldingprocessdialog.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    import_py_test.py

win32: LIBS += -LC:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/libs/ -lpython313

INCLUDEPATH += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/include
DEPENDPATH += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/include

win32:!win32-g++: PRE_TARGETDEPS += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/libs/python313.lib
else:win32-g++: PRE_TARGETDEPS += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/libs/libpython313.a

RESOURCES += \
    resources.qrc

TARGET = MXEditer

RC_ICONS=icons1.ico
