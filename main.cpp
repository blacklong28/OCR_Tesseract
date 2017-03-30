#include "WindowsForFun.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WindowsForFun w;
    w.show();
    return a.exec();
}
