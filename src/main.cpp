#include "mainwindow.h"
#include "configmanager.h"
#include <QApplication>
#include <QThreadPool>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("nimrod");
    app.setOrganizationName("nimrod");

    // Config betöltése – szálszám detektálása (csak egyszer)
    ConfigManager config;
    config.load();
    QThreadPool::globalInstance()->setMaxThreadCount(config.threadCount());

    MainWindow window;
    window.show();

    return app.exec();
}
