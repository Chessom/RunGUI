#include "RunGUI.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    RunGUI w;
    w.show();
    return a.exec();
}
