# Nimród böngésző – v2.0

**Qt6/C++ alapú asztali webböngesző – Chromium (QWebEngine) motorral**

---

## Újdonságok a 2.0-ban – Facebook / modern oldal kompatibilitás

A v1.0 az ismeretlen (QtWebEngine) User-Agent és a hiányzó pop-up kezelés
miatt a Facebookon lebutított oldalt adott, és a bejelentkezés is akadozott.
A v2.0 ezt javítja:

| Változás | Mit old meg |
|---|---|
| **Asztali Firefox User-Agent** + `--disable-features=UserAgentClientHint` | FB nem "nem támogatott böngészőt" jelez; a Google bejelentkezés sem tiltja ("a böngésző nem biztonságos"), mert Firefox-ként nincs `Sec-CH-UA` client hint, amit a Google keresztbe nézhetne |
| **`NimrodPage::createWindow()`** – `window.open()` / `target="_blank"` / OAuth pop-up új lapként nyílik | FB bejelentkezés, "Belépés Google-fiókkal", fotónézegető, megosztás ablakok |
| **Teljes képernyő kérés kezelése** (`fullScreenRequested`) | FB videók, Reels, Watch teljes képernyőn |
| **Jogosultság-kezelés** (`permissionRequested`) – értesítés engedélyezve, kamera/mikrofon/helyzet tisztán elutasítva | a JS Promise nem "lóg be", az oldal nem akad meg |
| **`ForcePersistentCookies` + tartós jogosultságok** | a bejelentkezés újraindítás után is megmarad |
| **Autoplay engedélyezve** (`--autoplay-policy`, `PlaybackRequiresUserGesture=false`) | néma videó-automatalejátszás, mint egy sima Chrome |
| **`hu-HU` Accept-Language** | magyar nyelvű FB felület |
| **Single-instance zár** (`QLockFile`) | két egyidejű példány közös profilja `Database IO error`-t és összeomlást (SIGSEGV) okozott service worker-t regisztráló oldalakon (pl. Google-keresés); a második példány most figyelmeztet és kilép |
| **Cookie-visszatöltés kikapcsolva** | a `CookieStore` veszteségesen (SameSite, forrás-origin nélkül) töltötte vissza a cookie-kat, ezért a Google `CookieMismatch`-et jelzett; a `ForcePersistentCookies` mellett a QtWebEngine saját tára veszi át, a `cookies.db` már csak olvasható tükör |

Érintett fájlok: `src/nimrodpage.{h,cpp}` (új), `src/browsertab.{h,cpp}`,
`src/mainwindow.cpp`, `src/main.cpp`, `src/integritychecker.cpp`, `CMakeLists.txt`.

---

## Funkciók

- 🌐 **Teljes HTML5 / CSS3 / JavaScript** támogatás (Qt WebEngine / Chromium motor)
- 🗂️ **Többablakos tabkezelés** – Firefox stílusú fülek
- 🔖 **Könyvjelzők** – eszköztár, mappák, drag-and-drop mentés, kezelő (Ctrl+Shift+O)
- 🔐 **AES-256-CBC titkosított jelszótár** – automatikus kitöltés (kulcs: Linux felhasználónév)
- 🛡️ **Induláskor futó integritás-ellenőrzés** – SHA-256 bináris ujjlenyomat, AES titkosítva
- 🔍 **Oldalon belüli keresés** – Ctrl+F
- 🧩 **Userscript támogatás** – egyéni JS szkriptek betöltése oldalanként
- 🖥️ **JavaScript fejlesztői konzol** – Ctrl+J
- 🛠️ **DevTools** – F12
- 🍪 **Cookie tár** – SQLite alapú, titkosítás nélküli session cookie kezelés
- ⚙️ **Konfiguráció** – `~/.config/nimrod/config.json`, kilépéskor automatikusan mentve
- 📋 **Menüsor** – Fájl / Szerkesztés / Súgó (billentyűkombinációk, Névjegy)

---

## Billentyűkombinációk

| Kombináció | Funkció |
|---|---|
| `Ctrl+T` | Új lap |
| `Ctrl+W` | Lap bezárása |
| `Ctrl+L` | URL mező fókusz |
| `Ctrl+R` / `F5` | Oldal újratöltése |
| `Alt+←` / `Alt+→` | Vissza / Előre |
| `Ctrl+F` | Keresés az oldalon |
| `Ctrl+D` | Oldal könyvjelzőzése |
| `Ctrl+Shift+O` | Könyvjelzőkezelő |
| `Ctrl+J` | JavaScript konzol |
| `F12` | DevTools |

---

## Könyvjelzők

A könyvjelző-eszköztár a navigációs sáv **alatt**, a lapfülek **felett** jelenik meg.

- **Mentés**: kattints a ★ gombra, vagy húzd a 📄 lapikont a toolbar-ra
- **Mappák**: jobb klikk a toolbaron → *Új mappa*
- **Kezelő**: Ctrl+Shift+O (fa nézet, szerkesztés, törlés)

---

## Biztonsági funkciók

### Credential Autofill
A böngésző észleli az `input[type=password]` és `input[type=text/email]` mezőket,
és az adott oldalhoz tartozó korábbi hitelesítési adatokat automatikusan kitölti.
Az adatok **AES-256-CBC** titkosítással tárolódnak SQLite adatbázisban
(`~/.config/nimrod/credentials.db`). A titkosítási kulcs a Linux rendszer
felhasználónevéből (SHA-256) képzett.

### Induláskor futó integritás-ellenőrzés
Minden induláskor a böngésző SHA-256 ujjlenyomatot számít a futtatható binárisból
(`/proc/self/exe`), és összeveti a `~/.config/nimrod/integrity.db`-ben tárolt,
AES-titkosított korábbi értékkel. Ha eltérést észlel, figyelmeztető ablak jelenik meg.

---

## Build

```bash
# Függőségek (Debian/Ubuntu)
sudo apt install qt6-base-dev qt6-webengine-dev qt6-tools-dev \
                 libqt6sql6-sqlite libssl-dev cmake build-essential

# Fordítás
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
./nimrod
```

---

## Konfiguráció

`~/.config/nimrod/config.json` – automatikusan létrejön az első indításkor,
és kilépéskor frissül (ablakméret, utolsó URL, stb.).

---

## Rendszerkövetelmények

- Linux (x86_64)
- Qt 6.4 vagy újabb
- OpenSSL 3.x
- CMake 3.16+

---

## Licenc és szerzői jogok

**Copyright © 2026 Komka László – Minden jog fenntartva.**

A szoftver forráskódja és minden kapcsolódó fájlja Komka László szellemi tulajdona.
Engedély nélküli másolás, terjesztés, módosítás vagy felhasználás tilos.
