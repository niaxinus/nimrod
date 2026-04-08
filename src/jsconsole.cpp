#include "jsconsole.h"

#include <QWebEnginePage>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QDateTime>
#include <QFont>

// ── Beviteli sor – history kezeléssel ───────────────────────────────────────

class JsInputLine : public QLineEdit
{
public:
    explicit JsInputLine(JsConsole *console, QWidget *parent = nullptr)
        : QLineEdit(parent), m_console(console) {}

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Up)        { m_console->historyUp();   return; }
        if (e->key() == Qt::Key_Down)      { m_console->historyDown(); return; }
        QLineEdit::keyPressEvent(e);
    }
private:
    JsConsole *m_console;
};

// ── JsConsole ────────────────────────────────────────────────────────────────

JsConsole::JsConsole(QWidget *parent)
    : QDockWidget("JavaScript konzol", parent)
{
    setupUi();
    setMinimumHeight(200);
}

void JsConsole::setupUi()
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *vbox  = new QVBoxLayout(container);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // Kimenet
    m_output = new QPlainTextEdit(container);
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(2000);
    QFont mono("Monospace", 10);
    mono.setStyleHint(QFont::Monospace);
    m_output->setFont(mono);
    m_output->setStyleSheet(
        "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; border: none; }"
    );
    vbox->addWidget(m_output, 1);

    // Bevitel sor
    QHBoxLayout *hbox = new QHBoxLayout();
    m_input = new JsInputLine(this, container);
    m_input->setFont(mono);
    m_input->setPlaceholderText("JavaScript kód...");
    m_input->setStyleSheet(
        "QLineEdit { background: #2d2d2d; color: #9cdcfe; border: 1px solid #555; padding: 2px 6px; }"
    );
    connect(m_input, &QLineEdit::returnPressed, this, &JsConsole::onRun);

    m_runBtn = new QPushButton("▶ Futtat", container);
    m_runBtn->setFixedWidth(80);
    connect(m_runBtn, &QPushButton::clicked, this, &JsConsole::onRun);

    m_clearBtn = new QPushButton("✕", container);
    m_clearBtn->setFixedWidth(30);
    m_clearBtn->setToolTip("Konzol törlése");
    connect(m_clearBtn, &QPushButton::clicked, this, &JsConsole::clear);

    hbox->addWidget(new QLabel(">", container));
    hbox->addWidget(m_input, 1);
    hbox->addWidget(m_runBtn);
    hbox->addWidget(m_clearBtn);
    vbox->addLayout(hbox);

    setWidget(container);

    appendOutput("// Nimród JS konzol – az aktuális oldal kontextusában fut", false);
}

void JsConsole::setPage(QWebEnginePage *page)
{
    m_page = page;
}

void JsConsole::appendOutput(const QString &text, bool isError)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString colored = isError
        ? "<span style='color:#f44747'>[" + ts + "] " + text.toHtmlEscaped() + "</span>"
        : "<span style='color:#9cdcfe'>[" + ts + "] " + text.toHtmlEscaped() + "</span>";
    m_output->appendHtml(colored);
}

void JsConsole::clear()
{
    m_output->clear();
}

void JsConsole::onRun()
{
    QString code = m_input->text().trimmed();
    if (code.isEmpty()) return;

    // History mentés
    if (m_history.isEmpty() || m_history.last() != code)
        m_history.append(code);
    m_historyIdx = m_history.size();

    // Bemenet megjelenítése
    m_output->appendHtml(
        "<span style='color:#dcdcaa'>&gt; " + code.toHtmlEscaped() + "</span>"
    );
    m_input->clear();

    runCode(code);
}

void JsConsole::runCode(const QString &code)
{
    if (!m_page) {
        appendOutput("Hiba: nincs oldal betöltve.", true);
        return;
    }

    QString escaped = code;
    escaped.replace('\\', "\\\\");
    escaped.replace('"',  "\\\"");
    escaped.replace('\n', "\\n");
    escaped.replace('\r', "\\r");

    m_page->runJavaScript(
        QString("(function(){ try { var _r = eval(\"%1\"); "
                "return typeof _r==='undefined'?'undefined':"
                "typeof _r==='object'&&_r!==null?JSON.stringify(_r,null,2):String(_r);"
                "} catch(e){ return 'HIBA: '+e.message; } })()")
            .arg(escaped),
        [this](const QVariant &result) {
            QString r = result.toString();
            bool isErr = r.startsWith("HIBA:");
            appendOutput(r, isErr);
        }
    );
}

void JsConsole::historyUp()
{
    if (m_history.isEmpty()) return;
    m_historyIdx = qMax(0, m_historyIdx - 1);
    m_input->setText(m_history[m_historyIdx]);
}

void JsConsole::historyDown()
{
    if (m_history.isEmpty()) return;
    m_historyIdx = qMin(m_history.size(), m_historyIdx + 1);
    m_input->setText(m_historyIdx < m_history.size() ? m_history[m_historyIdx] : QString());
}
