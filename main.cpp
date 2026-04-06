#include "mainwindow.h"
#include <QApplication>
#include "DatabaseManager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Khởi tạo Database
    if (!DatabaseManager::initDatabase()) {
        return -1;
    }

    MainWindow w;
    w.show();
    return a.exec();
}