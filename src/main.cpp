#include "mainwindow.h"
#include "configmanager.h"
#include "integritychecker.h"
#include <QApplication>
#include <QWebEngineProfile>
#include <QMessageBox>
#include <QDir>
#include <QDateTime>

int main(int argc, char *argv[])
{
    // WebEngine szükséges inicializáció az app előtt
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    app.setApplicationName("nimrod");
    app.setOrganizationName("nimrod");
    app.setApplicationVersion("2.0");

    // ── Integrity check ──────────────────────────────────────────────────
    QString configDir = QDir::homePath() + "/.config/nimrod";
    QDir().mkpath(configDir);

    bool integrityOk = IntegrityChecker::check(configDir + "/integrity.db");
    if (!integrityOk) {
        QMessageBox warn;
        warn.setWindowTitle("⚠ Biztonsági figyelmeztetés – Nimród");
        warn.setIcon(QMessageBox::Warning);
        warn.setText("<b>A böngésző bináris fájlja megváltozott!</b>");
        warn.setInformativeText(
            "A tárolt ujjlenyomat eltér az aktuális futtatható fájltól.\n\n"
            "Ez történhet frissítés, újrafordítás, vagy illetéktelen módosítás esetén.\n\n"
            "Az ujjlenyomat frissítve lett az aktuális állapotra.");
        warn.setStandardButtons(QMessageBox::Ok);
        warn.exec();
    }
    // ────────────────────────────────────────────────────────────────────

    MainWindow window;
    window.show();

    return app.exec();
}
