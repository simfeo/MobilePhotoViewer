#include "frameless.h"
#include "stylermanager.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    FormLess w;
    a.setStyle(new StylerManager);
    w.show();
    return a.exec();
}
