QT       += core gui
QT       += multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    heifmanager.cpp \
    imagerenderwidget.cpp \
    jpeginmemoryvideo.cpp \
    jpegmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    tinyexif/TinyEXIF.cpp \
    tinyexif/tinyxml2.cpp \
    videodecoder.cpp

HEADERS += \
    heifmanager.h \
    imagerenderwidget.h \
    jpeginmemoryvideo.h \
    jpegmanager.h \
    mainwindow.h \
    tinyexif/TinyEXIF.h \
    tinyexif/tinyxml2.h \
    videodecoder.h

FORMS += \
    imagerenderwidget.ui \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32
{
    DEFINES += _MSC_VER=1928
    INCLUDEPATH += $$PWD/../../ffmpeg_own/installed/include
    LIBS += -L$$PWD/../../ffmpeg_own/installed/bin -lavcodec -lavformat -lavfilter -lavutil -lswscale -lswresample


    INCLUDEPATH += $$PWD/../../libheif/libheif/
    INCLUDEPATH += $$PWD/../../libheif/
    CONFIG(debug, debug|release)
    {
        LIBS += -L$$PWD/../../libheif_build/Debug -lheif
    }
    CONFIG(release, debug|release)
    {
        LIBS += -L$$PWD/../../libheif_build/Release -lheif
    }
}
