#include "mainwindow.h"
#include <QApplication>
#include <QNetworkProxy>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    MainWindow w;
    w.setWindowTitle("激光切割");
    w.show();
    return a.exec();
}
