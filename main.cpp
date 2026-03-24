#include "mainwindow.h"
#include <QApplication>
#include <QNetworkProxy>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 添加这一行：强制禁用代理，使用直连模式
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    MainWindow w;
    w.show();
    return a.exec();
}
