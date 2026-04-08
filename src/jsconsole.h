#pragma once

#include <QDockWidget>
#include <QString>

class QWebEnginePage;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;

/**
 * JsConsole – beépített JavaScript konzol
 *
 * Ctrl+J billentyűre nyílik/zárul (MainWindow kezeli).
 * Az aktuális oldal JS kontextusában futtat kódot QWebEnginePage::runJavaScript() segítségével.
 */
class JsConsole : public QDockWidget
{
    Q_OBJECT

public:
    explicit JsConsole(QWidget *parent = nullptr);

    /** Az aktuális oldal beállítása – ez fogja végrehajtani a JS kódot */
    void setPage(QWebEnginePage *page);

public slots:
    /** Konzolba ír (pl. console.log üzenetek) */
    void appendOutput(const QString &text, bool isError = false);
    void clear();

public:
    void historyUp();
    void historyDown();

private slots:
    void onRun();

private:
    void setupUi();
    void runCode(const QString &code);

    QWebEnginePage  *m_page = nullptr;
    QPlainTextEdit  *m_output;
    QLineEdit       *m_input;
    QPushButton     *m_runBtn;
    QPushButton     *m_clearBtn;

    QStringList m_history;
    int         m_historyIdx = -1;
};
