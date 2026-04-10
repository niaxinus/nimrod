# Nimród böngésző

**Qt6/C++ alapú asztali webböngesző – Chromium (QWebEngine) motorral**

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
