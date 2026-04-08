#include "mainwindow.h"
#include "configmanager.h"
#include <QApplication>
#include <QWebEngineProfile>

int main(int argc, char *argv[])
{
    // WebEngine szükséges inicializáció az app előtt
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    app.setApplicationName("nimrod");
    app.setOrganizationName("nimrod");
    app.setApplicationVersion("2.0");

    MainWindow window;
    window.show();

    return app.exec();
}
