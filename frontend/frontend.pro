QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    PreviewDialog.cpp \
    app/AppConfig.cpp \
    app/BackendClient.cpp \
    model/ImagePairModel.cpp \
    model/TiePointModel.cpp

HEADERS += \
    mainwindow.h \
    PreviewDialog.h \
    app/AppConfig.h \
    app/BackendClient.h \
    model/ImagePairModel.h \
    model/TiePointModel.h

FORMS += \
    mainwindow.ui

# Translations
TRANSLATIONS += \
    translations/rigidlabeler_zh.ts

# Include translation resources
RESOURCES += \
    translations/translations.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
