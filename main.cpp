#include "mainwindow.h"

#include <QStyleFactory>
#include <QApplication>

int main(int argc, char* argv[])
{
    // Workaround for a Qt 6.7.0 FFmpeg-backend bug: hardware-decoded (D3D11)
    // video frames are rendered without cropping to the codec's conformance
    // window. A 1920x1080 HEVC motion-photo clip is decoded into a texture
    // padded up to 1920x1152, and the whole padded texture is drawn, which
    // squeezes the visible content and leaves a garbage/black strip along one
    // edge. Forcing the CPU conversion path makes Qt honor the crop. Motion
    // photo clips are short, so the extra copy is not noticeable.
    qputenv("QT_DISABLE_HW_TEXTURES_CONVERSION", "1");

    QApplication a(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    MainWindow w;
    w.show();
    if (QCoreApplication::arguments().size() > 1)
    {
        w.SetImage(QCoreApplication::arguments()[1]);
    }
    return a.exec();
}


// d:\_pictures\DCIMS10\2020-03-22\20200318_063552.jpg
// d:\_pictures\DCIMS10\2021-02-21\20210126_120204.jpg
// d:\_pictures\DCIMS10\2021-02-21\20210119_112641.jpg
// d:\_pictures\DCIMS10\2021-02-21\20210124_000908.jpg
