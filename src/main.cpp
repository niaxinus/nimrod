#include "mainwindow.h"
#include "configmanager.h"
#include "integritychecker.h"
#include <QApplication>
#include <QWebEngineProfile>
#include <QMessageBox>
#include <QDir>
#include <QDateTime>
#include <QLockFile>

int main(int argc, char *argv[])
{
    // ── Chromium flag-ek – a QApplication előtt kell beállítani ───────────
    // - autoplay: a Facebook / YouTube videók néma automatikus lejátszása
    // - WebRTC hálózati lista: FB Messenger hívások stabilabb ICE-je
    // - UserAgentClientHint KI: a böngésző Firefox UA-t ad ki (lásd
    //   mainwindow.cpp); a Firefox nem küld Sec-CH-UA client hint-eket, így
    //   ha a Chromium mégis küldené, az ellentmondás lenne, amit a Google
    //   bejelentkezés "nem biztonságos böngészőként" tiltana. Kikapcsolva
    //   nincs Sec-CH-UA fejléc és nincs navigator.userAgentData sem.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--autoplay-policy=no-user-gesture-required "
            "--enable-features=WebRTCPipeWireCapturer "
            "--disable-features=UserAgentClientHint");

    // WebEngine szükséges inicializáció az app előtt
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    app.setApplicationName("nimrod");
    app.setOrganizationName("nimrod");
    app.setApplicationVersion("2.0");

    QString configDir = QDir::homePath() + "/.config/nimrod";
    QDir().mkpath(configDir);

    // ── Single-instance zár ──────────────────────────────────────────────
    // A QWebEngineProfile perzisztens tára (cookie-k, előzmények, service
    // worker DB-k) egyszerre csak EGY folyamatból írható. Ha egy második
    // példány is megnyitja ugyanazt a profilt, a Chromium tárrétege
    // "Database IO error"-t dob, és a böngészőfolyamat elszáll (SIGSEGV) –
    // tipikusan akkor, amikor egy oldal (pl. Google) service worker-t
    // regisztrál. Ezért itt lezárjuk a profilt egy lock fájllal.
    // (A QLockFile felismeri, ha a korábbi PID már nem él – crash után nem
    //  marad bent a zár.)
    static QLockFile lockFile(configDir + "/nimrod.lock");
    if (!lockFile.tryLock(200)) {
        qint64 pid = 0;
        QString host, app;
        lockFile.getLockInfo(&pid, &host, &app);
        QMessageBox box;
        box.setWindowTitle("Nimród már fut");
        box.setIcon(QMessageBox::Warning);
        box.setText("<b>A Nimród böngésző már fut ebből a felhasználói fiókból.</b>");
        box.setInformativeText(
            QString("Egyszerre csak egy példány futhat, mert közös a profil-tár "
                    "(cookie-k, előzmények, bejelentkezések).\n\n"
                    "Zárd be a másik ablakot, vagy állítsd le a régi folyamatot:\n"
                    "    pkill nimrod\n\n"
                    "(Zároló folyamat PID-je: %1)")
                .arg(pid > 0 ? QString::number(pid) : QStringLiteral("ismeretlen")));
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
        return 1;
    }

    // ── Integrity check ──────────────────────────────────────────────────
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
