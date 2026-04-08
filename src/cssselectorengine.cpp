#include "cssselectorengine.h"

#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

CssSelectorEngine::CssSelectorEngine(QObject *parent)
    : QObject(parent)
{}

// ── Fő belépési pont ────────────────────────────────────────────────────────

QString CssSelectorEngine::apply(const QString &html, const QString &css) const
{
    if (css.trimmed().isEmpty()) return html;

    QVector<CssRule> rules = parseRules(css);
    if (rules.isEmpty()) return html;

    // Rendezés specificity szerint (kisebb előbb – a nagyobb felülírja)
    std::stable_sort(rules.begin(), rules.end(), [](const CssRule &a, const CssRule &b) {
        return a.specificity < b.specificity;
    });

    return injectStyles(html, rules);
}

// ── CSS parse ────────────────────────────────────────────────────────────────

QVector<CssSelectorEngine::CssRule> CssSelectorEngine::parseRules(const QString &css) const
{
    QVector<CssRule> rules;

    // Távolítsuk el a kommenteket
    static QRegularExpression commentRe(R"(/\*.*?\*/)",
                                         QRegularExpression::DotMatchesEverythingOption);
    QString cleaned = css;
    cleaned.remove(commentRe);

    // Szabályok parse: selector { declarations }
    static QRegularExpression ruleRe(
        R"(([^{]+)\{([^}]*)\})",
        QRegularExpression::DotMatchesEverythingOption
    );

    auto it = ruleRe.globalMatch(cleaned);
    while (it.hasNext()) {
        auto m = it.next();
        QString selectorPart = m.captured(1).trimmed();
        QString decls        = m.captured(2).trimmed();

        if (selectorPart.isEmpty() || decls.isEmpty()) continue;

        // @-szabályokat kihagyjuk (at-rules már a preprocessor kezeli)
        if (selectorPart.startsWith('@')) continue;

        // Vesszővel elválasztott szelektorok szétbontása
        QStringList selectors;
        for (const QString &s : selectorPart.split(',')) {
            QString trimmed = s.trimmed();
            if (!trimmed.isEmpty()) {
                selectors << trimmed;
            }
        }

        if (selectors.isEmpty()) continue;

        CssRule rule;
        rule.selectors    = selectors;
        rule.declarations = decls;
        // Specificity a legelső szelektor alapján (egyszerűsítés)
        rule.specificity  = calcSpecificity(selectors.first());
        rules.append(rule);
    }

    return rules;
}

int CssSelectorEngine::calcSpecificity(const QString &selector) const
{
    // ID = 100, class/attr/pseudo-class = 10, tag = 1
    int score = 0;
    score += selector.count('#') * 100;
    score += selector.count('.') * 10;
    score += selector.count('[') * 10;
    score += selector.count(':') * 10;

    // Tag nevek száma (szóközök alapján, de nem a speciális karaktereknél)
    static QRegularExpression tagRe(R"((?:^|[\s>+~])([a-zA-Z][a-zA-Z0-9]*)(?![.*#\[]))");
    auto it = tagRe.globalMatch(selector);
    while (it.hasNext()) {
        it.next();
        score += 1;
    }
    return score;
}

// ── HTML DOM inline style inject ─────────────────────────────────────────────

QString CssSelectorEngine::injectStyles(const QString &html,
                                         const QVector<CssRule> &rules) const
{
    // HTML tageket egyenként parse-olunk és módosítunk
    // Stratégia: az összes nyitó taget megtaláljuk, és szükség esetén style-t adunk hozzá

    static QRegularExpression tagRe(
        R"(<([a-zA-Z][a-zA-Z0-9]*)(\s[^>]*?)?(/)?>)",
        QRegularExpression::DotMatchesEverythingOption
    );

    // Előkészítés: megszámoljuk az elemeket az first/last-child meghatározáshoz
    // Egyszerűsítve: végigiterálunk a tagekon, szülő-gyerek szintet nem követünk
    // (teljes DOM-fa követése regex-szel nem reális, de alap esetekre elegendő)

    QString result = html;
    QString out;
    out.reserve(result.size() + result.size() / 4);

    int last = 0;
    auto it = tagRe.globalMatch(result);

    // Gyerek-sorszám követés (egyszerűsített: tag névtől függetlenül)
    // A valódi :first-child/:last-child csak közelítő lesz
    QMap<int, int> depthCount; // mélység → sorszám az adott mélységben
    int depth = 0;

    // Zárótag figyelő
    static QRegularExpression closeTagRe(R"(<(/[a-zA-Z][a-zA-Z0-9]*)\s*>)");

    // Kettős menet: 1) csak nyitótagek pozíciói, 2) módosítás
    // Elég egy menetben feldolgozni: minden nyitótagnél megvizsgáljuk a szabályokat

    while (it.hasNext()) {
        auto m = it.next();

        // Szöveg a tagek között (beleértve a zárótageket) változatlanul
        out += result.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();

        QString tagName   = m.captured(1).toLower();
        QString attrsPart = m.captured(2); // pl. ' class="foo" id="bar"'
        QString selfClose = m.captured(3); // "/" ha önzáró

        // Csak nyitótageket kezeljük (zárótageket átengedjük)
        // (A regex csak nyitótagekre illeszkedik, zárótageket nem kapja el)

        // Attribútumok kinyerése
        QString id        = getAttr(attrsPart, "id");
        QString classAttr = getAttr(attrsPart, "class");

        // Összegyűjtjük az alkalmazandó stílusokat
        QMap<int, QStringList> stylesBySpec; // specificity → deklarációk

        for (const CssRule &rule : rules) {
            for (const QString &selector : rule.selectors) {
                if (matchesSelector(selector, tagName, id, classAttr, attrsPart,
                                    false, false)) {
                    stylesBySpec[rule.specificity] << rule.declarations;
                    break; // egy szabályból elég egyszer
                }
            }
        }

        if (!stylesBySpec.isEmpty()) {
            // Összefűzzük specificity sorrendben
            QString newStyles;
            for (auto sit = stylesBySpec.begin(); sit != stylesBySpec.end(); ++sit) {
                for (const QString &decl : sit.value()) {
                    if (!newStyles.isEmpty() && !newStyles.endsWith(';'))
                        newStyles += ';';
                    newStyles += decl;
                }
            }

            // Meglévő style kiegészítése
            QString existingStyle = getAttr(attrsPart, "style");
            QString merged = mergeStyles(existingStyle, newStyles);

            // style attribútum frissítése / hozzáadása
            static QRegularExpression styleAttrRe(
                R"(style\s*=\s*["'][^"']*["'])",
                QRegularExpression::CaseInsensitiveOption
            );
            if (!existingStyle.isEmpty()) {
                // Csere
                QString newAttrs = attrsPart;
                newAttrs.replace(styleAttrRe, "style=\"" + merged + "\"");
                attrsPart = newAttrs;
            } else {
                // Hozzáadás
                attrsPart += " style=\"" + merged + "\"";
            }
        }

        // Tag újraépítés
        out += "<" + tagName + attrsPart + selfClose + ">";
    }
    out += result.mid(last);

    return out;
}

// ── Szelektor illesztés ──────────────────────────────────────────────────────

bool CssSelectorEngine::matchesSelector(const QString &selector,
                                         const QString &tag,
                                         const QString &id,
                                         const QString &classAttr,
                                         const QString &allAttrs,
                                         bool isFirstChild,
                                         bool isLastChild) const
{
    // Komplex szelektort (leszármazott, >) az utolsó részére illesztjük
    // (egyszerűsítés: csak az utolsó szimpla szelektor érvényes)
    QString simple = selector.trimmed();

    // A > B → utolsó rész
    if (simple.contains('>')) {
        simple = simple.section('>', -1).trimmed();
    }
    // A B → utolsó rész (leszármazott)
    else if (simple.contains(' ')) {
        simple = simple.section(' ', -1).trimmed();
    }

    // :first-child / :last-child pseudo
    bool needFirst = simple.contains(":first-child");
    bool needLast  = simple.contains(":last-child");
    simple.remove(QRegularExpression(R"(:[\w-]+(?:\([^)]*\))?)"));

    if (needFirst && !isFirstChild) return false;
    if (needLast  && !isLastChild)  return false;

    // [attr] és [attr=val] attribútum szelektor
    static QRegularExpression attrSelRe(R"(\[([^\]=]+)(?:=["']?([^"'\]]+)["']?)?\])");
    {
        auto am = attrSelRe.match(simple);
        if (am.hasMatch()) {
            QString attrName  = am.captured(1).trimmed();
            QString attrVal   = am.captured(2).trimmed();
            QString actualVal = getAttr(allAttrs, attrName);
            simple.remove(am.capturedStart(), am.capturedLength());
            simple = simple.trimmed();

            if (actualVal.isNull()) return false; // attribútum nem létezik
            if (!attrVal.isEmpty() && actualVal != attrVal) return false;
        }
    }

    // .class szelektor
    QStringList classes = classAttr.split(' ', Qt::SkipEmptyParts);

    // Szelektor parse: tagNev + .class + #id kombinációk
    static QRegularExpression selectorRe(
        R"(^([a-zA-Z][a-zA-Z0-9]*)?([.#][a-zA-Z_-][a-zA-Z0-9_-]*)*$)"
    );

    // Tag rész
    static QRegularExpression tagPartRe(R"(^([a-zA-Z][a-zA-Z0-9]*)?)");
    auto tagM = tagPartRe.match(simple);
    QString selectorTag = tagM.hasMatch() ? tagM.captured(1).toLower() : "";

    if (!selectorTag.isEmpty() && selectorTag != tag) return false;

    // .class részek
    static QRegularExpression classPartRe(R"(\.([\w-]+))");
    auto cit = classPartRe.globalMatch(simple);
    while (cit.hasNext()) {
        auto cm = cit.next();
        if (!classes.contains(cm.captured(1))) return false;
    }

    // #id rész
    static QRegularExpression idPartRe(R"(#([\w-]+))");
    auto idM = idPartRe.match(simple);
    if (idM.hasMatch() && idM.captured(1) != id) return false;

    // Ha elértük idáig, illeszkedik
    return true;
}

// ── Stílus összefésülés ──────────────────────────────────────────────────────

QString CssSelectorEngine::mergeStyles(const QString &existing, const QString &newStyle) const
{
    // Meglévő style-ba a new értékeket bemásoljuk (felülírva az azonos kulcsokat)
    QMap<QString, QString> props;

    auto parseDecls = [&props](const QString &css) {
        for (const QString &decl : css.split(';', Qt::SkipEmptyParts)) {
            int colon = decl.indexOf(':');
            if (colon < 0) continue;
            QString key = decl.left(colon).trimmed().toLower();
            QString val = decl.mid(colon + 1).trimmed();
            if (!key.isEmpty()) props[key] = val;
        }
    };

    parseDecls(existing);
    parseDecls(newStyle);

    QString result;
    for (auto pit = props.begin(); pit != props.end(); ++pit) {
        result += pit.key() + ":" + pit.value() + ";";
    }
    return result;
}

// ── Attribútum kinyerés ──────────────────────────────────────────────────────

QString CssSelectorEngine::getAttr(const QString &tagHtml, const QString &attr) const
{
    if (attr.isEmpty() || tagHtml.isEmpty()) return {};
    QRegularExpression re(
        attr + R"(\s*=\s*["']([^"']*)["'])",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = re.match(tagHtml);
    if (m.hasMatch()) return m.captured(1);

    // Idézőjel nélküli érték
    QRegularExpression re2(
        attr + R"(\s*=\s*([^\s>]+))",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m2 = re2.match(tagHtml);
    if (m2.hasMatch()) return m2.captured(1);

    // Értéknélküli attribútum (boolean)
    QRegularExpression re3(
        R"((?:^|\s))" + attr + R"((?:\s|>|$))",
        QRegularExpression::CaseInsensitiveOption
    );
    if (re3.match(tagHtml).hasMatch()) return QString(""); // létezik, de üres

    return QString(); // null → nem létezik
}
