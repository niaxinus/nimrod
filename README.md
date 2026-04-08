# Nimród böngésző

> ⚠️ **KÍSÉRLETI PROJEKT – NEM ÍGY KELL CSINÁLNI**
>
> Ez a projekt kizárólag tanulási és kísérletezési célt szolgál.
> Valódi böngésző fejlesztéséhez **ne** ezt a megközelítést használd –
> léteznek erre a célra kifejlesztett, megbízható motorok (lásd lent).

---

## Mi ez?

A Nimród egy Qt6/C++ alapú, kísérleti jellegű webböngesző, amely **QTextBrowser**
és **QTextDocument** segítségével renderel HTML/CSS tartalmat. Saját preprocesszor
pipeline-t valósít meg, hogy a Qt beépített, korlátozott HTML4/CSS2.1 renderelőjét
kibővítse.

## Miért kísérleti?

A Qt `QTextBrowser` / `QTextDocument` egy **dokumentumnézegető**, nem böngészőmotor.
Ezért a projekt saját kóddal próbálja meg pótolni, amit egy igazi motor ingyen ad:

| Amit pótolni kell | Valódi megoldás |
|---|---|
| HTML5 szemantikus tagek | WebKit / Blink natívan kezeli |
| CSS szelektorok (`.class`, `#id`) | CSS engine a motorban van |
| Flexbox / Grid layout | Beépített layout motor |
| CSS shorthand kibontás | Beépített CSS parser |
| JavaScript | V8 / SpiderMonkey motor |

Ezek mind manuálisan, regex alapon vannak implementálva – ez törékeny,
lassú, és soha nem lesz teljes vagy hibamentes.

## Hogyan kellene helyesen csinálni?

Ha Qt-ban szeretnél böngészőt készíteni, használd:

- **[Qt WebEngine](https://doc.qt.io/qt-6/qtwebengine-index.html)** – Chromium alapú,
  teljes HTML5/CSS3/JS támogatással. Ez a helyes út.
- **[Qt WebView](https://doc.qt.io/qt-6/qtwebview-index.html)** – platformfüggő
  natív webview (iOS Safari, Android WebView).

Ha saját motort akarsz írni (komolyan), nézd meg:
- [WebKit](https://webkit.org/) – nyílt forrású böngészőmotor
- [servo](https://servo.org/) – Rust alapú kísérleti motor (Mozilla)

## Amit ez a projekt megvalósít

- ✅ HTML5 → HTML4 normalizálás (`HtmlNormalizer`)
- ✅ CSS preprocesszálás: `<style>`, `<link>`, `@import`, `--var`, `calc()`, media query
- ✅ CSS shorthand kibontás (`background`, `font`, `border`, `margin`, `padding`)
- ✅ CSS szelektorok → inline style inject (`HtmlNormalizer`)
- ✅ Flexbox / Grid → `<table>` konverzió (`CssLayoutConverter`)
- ✅ Képrenderelés (`loadResource` override, HTTP + file://)
- ✅ JavaScript nullázás (`<script>`, `on*` attribútumok)
- ✅ Konfiguráció: `~/.config/nimrod/config.ini`, kilépéskor mentés
- ✅ Thread count detektálás (`QThread::idealThreadCount`)
- ✅ Link hover URL megjelenítés a státuszsávban

## Build

```bash
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
./nimrod
```

## Konfiguráció

`~/.config/nimrod/config.ini` – automatikusan létrejön első futtatáskor.

```ini
[window]
geometry=...

[system]
threadCount=8
```

---

*Nimród – kísérleti projekt. Fejlesztő: GitHub Copilot CLI segítségével.*
