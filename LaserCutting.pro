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
    positioningdialog.cpp \
    renderarea.cpp \
    rotationmatrixdialog.cpp \
    usercoordinatemanager.cpp \
    weldingprocessdialog.cpp

HEADERS += \
    EfortSdk.h \
    EfortSdkNet.h \
    SdkConstDef.h \
    SdkStructDef.h \
    connectiondialog.h \
    libs/efort-robotics/include/irobot.h \
    libs/efort-robotics/include/math_pose.hpp \
    mainwindow.h \
    modbusmanager.h \
    positioningdialog.h \
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
    import_py.py \
    libs/efort-robotics/libs/x64/efort-robotics-d.dll \
    libs/efort-robotics/libs/x64/efort-robotics-d.ilk \
    libs/efort-robotics/libs/x64/efort-robotics-d.lib \
    libs/efort-robotics/libs/x64/efort-robotics-d.pdb \
    libs/efort-robotics/libs/x64/efort-robotics.dll \
    libs/efort-robotics/libs/x64/efort-robotics.ilk \
    libs/efort-robotics/libs/x64/efort-robotics.lib \
    libs/efort-robotics/libs/x64/efort-robotics.pdb \
    libs/efort-robotics/libs/x86/efort-robotics-d.dll \
    libs/efort-robotics/libs/x86/efort-robotics-d.ilk \
    libs/efort-robotics/libs/x86/efort-robotics-d.lib \
    libs/efort-robotics/libs/x86/efort-robotics-d.pdb \
    libs/efort-robotics/libs/x86/efort-robotics.dll \
    libs/efort-robotics/libs/x86/efort-robotics.ilk \
    libs/efort-robotics/libs/x86/efort-robotics.lib \
    libs/efort-robotics/libs/x86/efort-robotics.pdb \
    libs/linux.tar.gz \
    libs/linux/libEftSdk.so \
    libs/linux/libEftSdkd.so \
    libs/linux/liblog4cpp.so \
    libs/linux/liblog4cpp.so.2.9 \
    libs/linux/liblog4cpp.so.2.9.1 \
    libs/linux/librlibcpp.bcc.so \
    libs/linux/librlibcpp.bcc.so.1 \
    libs/linux/librlibcpp.bcc.so.1.0.0.2 \
    libs/linux/librlibcpp.tool.so \
    libs/linux/librlibcpp.tool.so.1 \
    libs/linux/librlibcpp.tool.so.1.0.0.2 \
    libs/x64/EftSdk.dll \
    libs/x64/EftSdk.exp \
    libs/x64/EftSdk.lib \
    libs/x64/EftSdk.pdb \
    libs/x64/EftSdkd.dll \
    libs/x64/EftSdkd.exp \
    libs/x64/EftSdkd.lib \
    libs/x64/EftSdkd.pdb \
    libs/x64/NTEventLogAppender.dll \
    libs/x64/log4cpp-d.dll \
    libs/x64/log4cpp-d.lib \
    libs/x64/log4cpp.conf \
    libs/x64/log4cpp.dll \
    libs/x64/log4cpp.lib \
    libs/x64/log4cpp.nt.property \
    libs/x64/rlibcpp-bcc-d.dll \
    libs/x64/rlibcpp-bcc-d.lib \
    libs/x64/rlibcpp-bcc.dll \
    libs/x64/rlibcpp-bcc.lib \
    libs/x64/rlibcpp-tool-d.dll \
    libs/x64/rlibcpp-tool-d.lib \
    libs/x64/rlibcpp-tool.dll \
    libs/x64/rlibcpp-tool.lib \
    libs/x86/EftSdk.dll \
    libs/x86/EftSdk.exp \
    libs/x86/EftSdk.lib \
    libs/x86/EftSdk.pdb \
    libs/x86/EftSdkd.dll \
    libs/x86/EftSdkd.exp \
    libs/x86/EftSdkd.lib \
    libs/x86/EftSdkd.pdb \
    libs/x86/NTEventLogAppender.dll \
    libs/x86/log4cpp-d.dll \
    libs/x86/log4cpp-d.lib \
    libs/x86/log4cpp.conf \
    libs/x86/log4cpp.dll \
    libs/x86/log4cpp.lib \
    libs/x86/rlibcpp-bcc-d.dll \
    libs/x86/rlibcpp-bcc-d.lib \
    libs/x86/rlibcpp-bcc.dll \
    libs/x86/rlibcpp-bcc.lib \
    libs/x86/rlibcpp-tool-d.dll \
    libs/x86/rlibcpp-tool-d.lib \
    libs/x86/rlibcpp-tool.dll \
    libs/x86/rlibcpp-tool.lib

win32: LIBS += -LC:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/libs/ -lpython313

INCLUDEPATH += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/include
DEPENDPATH += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/include

win32:!win32-g++: PRE_TARGETDEPS += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/libs/python313.lib
else:win32-g++: PRE_TARGETDEPS += C:/Users/zhangpeng/AppData/Local/Programs/Python/Python313/libs/libpython313.a

RESOURCES += \
    resources.qrc

TARGET = MXEditer

RC_ICONS=icons1.ico
DEFINES += NOMINMAX
INCLUDEPATH += $$PWD
CONFIG(debug, debug|release) {
    LIBS += "$$PWD/libs/x64/EftSdk.lib"
} else {
    LIBS += "$$PWD/libs/x64/EftSdk.lib"
}
