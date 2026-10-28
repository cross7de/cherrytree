# CherryTree – lokale Projekt-CLI

**Stand:** 28.10.2026 · **Lokaler Funktionsstand:** v1.6  
**Git-Zweig:** `local/project-cli` · **Status:** lokal erprobt, nicht Bestandteil des Upstream-PR #2943

Diese persönliche CherryTree-Erweiterung verwaltet Projekte über den Rich-Text-Knoten **`Projekt`**. Sie legt Projekte aus einer Vorlage an, listet das Projektregister auf, benennt Projekte um und löscht sie. Beim Umbenennen werden interne Projektverweise automatisch mitgepflegt; optional können auch alle sichtbaren Textvorkommen des bisherigen vollständigen Projektnamens ersetzt werden.

Die Befehle ergänzen die bestehende CherryTree-CLI; sie ersetzen deren Optionen nicht. Der kurze Programmname `ct` kann als Shell-Alias oder als Symlink eingerichtet werden. Ohne ihn kann `cherrytree` beziehungsweise während der Entwicklung `./build/cherrytree` verwendet werden.

## Schnellstart

```text
ct [DATEI.ctb] project create "Projektname"
ct [DATEI.ctb] project list [-N | --years N | -a | --all]
ct [DATEI.ctb] project rename PNR "Neuer Projektname" [--all-occurrences]
ct [DATEI.ctb] project delete PNR [-r | --recursive] [-y | --yes]
ct project --help
```

Beispiele mit einem ausdrücklich angegebenen Dokument:

```bash
ct ~/pfad/gedaechtnis.ctb project create "Mein Projekt"
ct ~/pfad/gedaechtnis.ctb project list -3
ct ~/pfad/gedaechtnis.ctb project rename 6P25 "Neuer Name"
ct ~/pfad/gedaechtnis.ctb project rename 6P25 "Neuer Name" --all-occurrences
ct ~/pfad/gedaechtnis.ctb project delete 6P25
```

**PNR** bezeichnet die sichtbare Projektnummer, z. B. `6P25`, **nicht** die interne numerische CherryTree-Knoten-ID.

### Kurzbefehle

Eindeutige Präfixe funktionieren pro Kommandoebene, z. B.:

```bash
ct p c "Mein Projekt"
ct p l -a
ct p r 6P25 "Neuer Name"
ct p r 6P25 "Neuer Name" --all-occurrences
ct p d 6P25 -r -y
```

Für Skripte und dauerhafte Anleitungen die **voll ausgeschriebenen** Namen verwenden. Ein später hinzukommendes Kommando könnte eine Präfixabkürzung mehrdeutig machen.

## Befehle

### `project create NAME`

Erzeugt ein Projekt aus dem aktuellen Platzhalter:

1. Platzhalter und verlinkte Vorlage im Register prüfen.
2. Den Vorlagenknoten als eigenständigen Knoten direkt hinter dem Original duplizieren.
3. In der Kopie die Kopfzeile `# <PNR> - <PROJEKTNAME>` mit der Projektnummer und dem Namen ersetzen.
4. Das Original zur **nächsten Platzhalternummer** umbenennen; seine Vorlagenkopfzeile bleibt unverändert.
5. Die vorhandene Platzhalterreferenz aktualisieren und den neuen Projektlink direkt darunter einfügen.
6. Die `.ctb` speichern.

Bei Erfolg wird die **neue numerische CT-ID** allein als Ergebnis auf `stdout` ausgegeben:

```text
2775
```

Die Projektnummer wird **nicht als Operand eingegeben**; sie stammt aus dem derzeitigen Platzhalter. Der Projektname darf nicht leer sein und darf nicht `PLATZHALTER` lauten.

### `project list [-N | --years N | --all]`

Liest die Projektlinks **unterhalb der Platzhalterreferenz** im Knoten `Projekt`, unabhängig davon, in welchen Ordnern die Zielknoten liegen. Die Statuszeichen vor diesen Links dienen der Anzeige im Register und sind keine Auswahlbedingung für `list`.

Beispielausgabe:

```text
[2775]  6P25 - Mein Projekt
[2772]  6P24 - CS
[2771]  6P23 - KPA
```

Die CT-IDs stehen vorne in eckigen Klammern und sind rechtsbündig auf die längste angezeigte ID ausgerichtet. Die Reihenfolge entspricht dem Projektregister. Die Vorlage selbst wird nicht als reguläres Projekt ausgegeben.

| Filter | Wirkung |
|---|---|
| `list` oder `list -1` | Jüngstes **vorhandenes Projektjahr** |
| `list -3` oder `list --years 3` | Drei Kalenderjahre, einschließlich des jüngsten Projektjahres; Jahre ohne Projekte zählen mit |
| `list -a` oder `list --all` | Sämtliche im Register gefundenen Projekte |

Das Bezugsjahr ist **nicht das Systemdatum**, sondern das Jahr des jüngsten regulären Projekts. `-a` gehört hier zum `project`-Unterkommando und kollidiert nicht mit CherryTrees bisherigem globalem `-a` (`--anchor`).

**Aktuelle Einschränkung:** Gemischte ein- und zweistellige Jahreskennungen sind bei zeitlich begrenzten Listen nicht unterstützt; dafür vorerst `--all` verwenden. Für die bisherige einstellige Kennung ist der Jahresfilter auf höchstens zehn Jahre begrenzt. Es wird keine Dekade aus dem aktuellen Datum hinzuerfunden.

### `project rename PNR NAME [--all-occurrences]`

Sucht das Projekt über die **vollständige PNR**. Ohne Zusatzoption werden geändert:

- der sichtbare Knotentitel des Projekts;
- die erste Kopfzeile im Projektinhalt (`# PNR - Projektname`);
- die Beschriftung des internen Projektlinks im Register;
- alle weiteren **internen Links auf dieselbe CT-ID**, sofern deren sichtbare Beschriftung den **vollständigen alten Projektnamen** enthält – auch innerhalb längerer Beschriftungen oder mehrfach in einem Link.

Andere Linkbeschriftungen (beispielsweise `Meine Projektnotizen`) und gewöhnlicher, nicht verlinkter Text bleiben im Standardmodus unverändert.

Mit `--all-occurrences` werden **zusätzlich alle Vorkommen des vollständigen alten Projektnamens im sichtbaren Haupttext aller Knoten** ersetzt – unabhängig davon, ob die Zeichen unverlinkt oder Teil einer anderen Linkbeschriftung sind:

```text
Alter Name: 6P25 - Mein Projekt
Neuer Name: 6P25 - Neues Projekt

Ohne --all-occurrences:
  Siehe 6P25 - Mein Projekt       (Link auf dieses Projekt)  → aktualisiert
  6P25 - Mein Projekt            (unverlinkter Text)        → unverändert
  6P25 - Mein Projekt            (Link auf anderen Knoten)  → unverändert

Mit --all-occurrences:
  Alle drei sichtbaren Textvorkommen                       → aktualisiert
```

**Grenzen und Schutzmaßnahmen:**

- Gesucht wird die **vollständige bisherige Zeichenfolge** `PNR - Alter Projektname`, nicht die PNR allein und nicht einzelne Wörter des Titels.
- CT-ID, interne Linkziele, URLs als Linkziele und Knotennamen anderer Knoten bleiben unverändert. Ausgetauscht wird nur die sichtbare Beschriftung beziehungsweise der Text.
- Der Text eines Treffers darf keine Grenze zwischen unterschiedlichen Links oder zwischen verlinktem und unverlinktem Text überschreiten. Andernfalls wird die globale Umbenennung verweigert, statt Linkattribute zu beschädigen.
- **Eingebettete Tabellen und Codeboxen** werden nicht durchsucht; `--all-occurrences` ist damit keine vollständige Ersetzung sämtlicher Datenfelder einer CTB.
- Alle betroffenen Textstellen werden vor Änderungen ermittelt und geprüft. Ein schreibgeschützter betroffener Knoten oder nicht sicher bestimmbarer Zeichenversatz führt zum Abbruch.
- Bei fehlender, mehrdeutiger oder strukturell abweichender Projektzuordnung wird die Aktion ebenfalls verweigert.

`--all-occurrences` ist eine ausdrücklich anzufordernde dokumentweite Textersetzung. Der Standardmodus bleibt auf das Projekt selbst und die eindeutig zugehörigen internen Projektlinks begrenzt. Die Einschränkung auf eindeutige Textstellen verhindert allerdings **keine** inhaltlich unerwünschte Ersetzung eines zufällig gleichlautenden Texts; daher vorher sichern.

### `project delete PNR [-r] [-y]`

Sucht das Projekt zuerst über die **PNR** und prüft den Projektknoten, seine Referenz und die Löschbedingungen. **Erst danach** wird eine Bestätigung angefordert:

```text
Projekt 6P25 - Mein Projekt wirklich löschen? [j/N]
```

Nur `j` oder `ja` (auch `J`/`JA`) bestätigt die Löschung. `-y` / `--yes` unterdrückt nur die Rückfrage, **nicht** die Vorprüfung. Ohne interaktives Terminal ist `--yes` notwendig.

| Option | Bedeutung |
|---|---|
| Keine | Projekt ohne Unterknoten nach Rückfrage löschen |
| `-r`, `--recursive` | Auch alle Unterknoten löschen |
| `-y`, `--yes` | Rückfrage unterdrücken |
| `-r -y` | Unterbaum ohne Rückfrage löschen (Reihenfolge von `-r` und `-y` beliebig) |

**Wichtige Einschränkungen:** Shared Nodes sind aus Sicherheitsgründen ausgeschlossen. Die Routine entfernt den Registereintrag und den Projektknoten einschließlich ausdrücklich zugelassener Unterknoten. Links aus **anderen** CherryTree-Knoten auf gelöschte IDs werden nicht automatisch bereinigt.

Wird das **jüngste Projekt direkt unter der Platzhalterreferenz** gelöscht, setzt die Routine den Platzhalter auf dessen frei gewordene PNR zurück – aber nur bei eindeutig aufeinanderfolgenden, konsistenten Nummern. Andernfalls wird die Löschung vor der Rückfrage verweigert. Bei älteren Projekten bleibt der Platzhalter unverändert.

## Voraussetzung: Aufbau der CherryTree-Daten

Die Erweiterung erwartet **genau einen** Rich-Text-Knoten namens `Projekt`. Unter diesem liegt genau ein aktueller Platzhalterknoten, beispielsweise:

```text
6P25 - PLATZHALTER
```

Sein Inhalt muss mit folgender **erster Zeile** beginnen:

```text
# <PNR> - <PROJEKTNAME>
```

Das Projektregister steht im Rich-Text-Inhalt des Knotens `Projekt`, beispielsweise:

```text
# 6P25 - PLATZHALTER
- 6P24 - CS
- 6P23 - KPA
```

Die Projektbeschriftungen sind **interne CherryTree-Links**, keine bloßen Textzeilen. Die `#`-Zeile verlinkt auf den Platzhalter; die folgenden Zeilen verlinken auf die jeweiligen Projektknoten. Mögliche Kennzeichen vor vorhandenen Projektlinks sind `-`, `v`, `x`, `r`, `w` oder `i`.

Die tatsächlichen Projektknoten dürfen in verschiedenen Unterordnern liegen. **Das Register ist maßgeblich**, nicht die Ablageposition. Die CT-ID des Platzhalters ist nicht fest im Quellcode vorgegeben.

Derzeitige PNR-Schreibweisen sind `6P25` und – grundsätzlich erkennbar – `26P25` (`[0-9]{1,2}P[0-9]{2}`). Die Erweiterung **konvertiert diese Schreibweisen nicht**. Eine langfristig eindeutige Jahreslogik ist noch offen.

## Dateiauswahl und Sicherheit

- Mit `DATEI.ctb` wird genau diese Datei angesprochen; empfohlen insbesondere für schreibende Aufrufe.
- Ohne Dateipfad ist nur dann eine Ausführung möglich, wenn in **derselben CherryTree-Anwendungsinstanz** genau ein geeignetes `.ctb`-Dokument offen ist. Andernfalls bricht die CLI ab. Eine mit `-S` gestartete sekundäre Sitzung hat eine andere Anwendungskennung und zählt für den normalen Aufruf ohne `-S` nicht mit.
- Der Projektbereich unterstützt derzeit nur **unverschlüsselte SQLite-`.ctb`-Dateien**.
- `create`, `rename` und `delete` verweigern die Ausführung bei **ungespeicherten Änderungen** im betroffenen geöffneten Dokument.
- Keine externe SQLite-Schreiboperation bei der eigentlichen Projektverwaltung: Änderungen und Speichern laufen über CherryTree. Der **Regressionstest** bearbeitet dagegen seine separate temporäre CTB-Kopie ausdrücklich auch mit SQLite, und nur bei geschlossener GUI.
- Es gibt noch **keine automatische Rücknahme** eines begonnenen mehrstufigen Vorgangs. Vor schreibenden Operationen daher Sicherungen vorsehen. Ein Speicherfehler kann Änderungen im geöffneten GUI-Zustand hinterlassen.
- Synchronisationen und andere Instanzen sollten die **gleiche CTB nicht gleichzeitig verändern**.

**Exit-Status:** `0` bei Erfolg, ungleich `0` bei Fehler oder Abbruch. Ergebnisse werden auf `stdout`, Diagnosen auf `stderr` ausgegeben; GUI-/GTK-Diagnostik kann ebenfalls auf `stderr` erscheinen. `rename` muss bei Erfolg keinen Ergebnistext erzeugen.

## Hilfe und Tests

```bash
ct project --help
ct project help list
ct project rename --help
ct project delete --help
```

### Automatischer Regressionstest

Den **selbst gebauten CherryTree** und eine CTB mit intaktem Projektregister und Platzhalter bereitstellen:

```bash
python3 tests/test_local_project_cli.py \
    --binary ./build/cherrytree \
    --fixture /pfad/gedaechtnis.test.ctb
```

Der Test verwendet SQLite-Backup zum Erzeugen **einer eigenen temporären CTB-Kopie**. Die mit `--fixture` angegebene Datei wird nicht verändert. Alle Testprojekte werden ausschließlich in dieser Kopie angelegt; vorhandene reale Projektknoten dienen **niemals als Testobjekte**.

Getestet werden:

1. `create → rename → delete` in derselben laufenden GUI-Sitzung;
2. automatische Aktualisierung interner Projektlinks einschließlich Textzusätzen und mehrfach vorkommendem Projektnamen;
3. die zusätzlichen Ersetzungen mit `rename --all-occurrences` in gewöhnlichem Text und Beschriftungen anderer Links;
4. Ablehnung unzulässiger Argumente und anschließende Entfernung der selbst angelegten Testprojekte;
5. Unverändertheit aller ursprünglichen Projekt-/Inhaltsknoten (abgesehen vom vorübergehend verwalteten Register und Platzhalter), Wiederherstellung des Platzhalters und `PRAGMA quick_check` der Testdatenbank.

Der Test benötigt eine grafische Sitzung (X11/Wayland) und `dbus-run-session`. **Er kann nach Ende der GUI-Phasen noch einige Zeit arbeiten.** Nach folgender Meldung bitte nicht mit `Ctrl+C` abbrechen, sondern bis zur abschließenden `OK:`-Meldung warten:

```text
INFO: GUI-Tests abgeschlossen. Abschließende Daten- und SQLite-Integritätsprüfung läuft; bitte bis zur letzten OK-Meldung nicht abbrechen.
OK: ursprüngliche Knoten unverändert, Testprojekte entfernt, CTB konsistent
```

Diese Prüfungen wurden am 28.10.2026 unter Debian 13 / GTK3 erfolgreich ausgeführt. Das Projekt wird mit `./build.sh` gebaut.

### Manuelle CLI-Prüfung

Für manuelle Versuche den selbst gebauten CherryTree mit `-S` (separate Sitzung) und einer **CTB-Arbeitskopie** verwenden, damit er nicht unbeabsichtigt die installierte oder produktive GUI-Instanz anspricht:

```bash
cp /pfad/gedaechtnis.test.ctb /tmp/ct-project-test.ctb

./build/cherrytree -S /tmp/ct-project-test.ctb project list -a
./build/cherrytree -S /tmp/ct-project-test.ctb project create "CLI-Test"
./build/cherrytree -S /tmp/ct-project-test.ctb project rename 6P25 "CLI-Test-neu"
./build/cherrytree -S /tmp/ct-project-test.ctb project delete 6P25
```

Das Beispiel für `rename`/`delete` setzt voraus, dass die tatsächlich angelegte PNR `6P25` ist. Bei der letzten Löschabfrage zunächst `n` eingeben; die echte Löschung erst nach Kontrolle durchführen. Danach die CTB erneut öffnen und Knoten, Referenzen sowie Platzhalter überprüfen.

Ein bereits laufender **älterer** CherryTree-Prozess kann CLI-Aufrufe über D-Bus abweisen. Die separate Testinstanz mit `-S` umgeht dies für die Entwicklung.

## Deployment unter Debian 13 (eigener CherryTree-Fork)

Das Repository unter `local/project-cli` ist die **Entwicklungsquelle**. Für den täglichen KDE-Einsatz empfiehlt sich ein selbst erzeugtes **Debian-Paket** statt eines direkten Kopierens von Dateien nach `/usr/bin` oder `sudo ninja install`. So kann `dpkg` die installierten Programmdateien weiterhin verwalten.

### 1. Installierten Zustand feststellen

```bash
apt policy cherrytree
dpkg-query -W -f='${Version}\n' cherrytree
command -v cherrytree
dpkg -S /usr/bin/cherrytree
cherrytree project --help
```

`cherrytree project --help` prüft, ob **die installierte Binärdatei** die lokale Erweiterung kennt. `./build/cherrytree project --help` testet dagegen nur das Entwicklungs-Binary. Wenn das installierte Programm bereits funktioniert, ist keine Neuinstallation erforderlich.

**Paketversionsschema des lokalen Forks:** `1.7.2-2+ctprojN`. `1.7.2` ist die CherryTree-Programmversion (aus `debian/changelog` über `CMakeLists.txt`), `-2` die Debian-Basisrevision und `+ctprojN` die laufende lokale Paketrevisionsnummer. Bei einer neuen verteilten Paketfassung `N` erhöhen; beispielsweise folgt auf `1.7.2-2+ctproj1` die Version `1.7.2-2+ctproj2`. Das ändert die Debian-Paketversion, nicht die im Programm angezeigte CherryTree-Version.

### 2. Debian-Paket erstellen (ohne `sudo`)

Dieser Abschnitt wird im **CherryTree-Quellverzeichnis** als normaler Benutzer ausgeführt. Die einzige manuell zu ändernde Versionsangabe ist `CTPROJ_REV`; bei jedem neuen Paketstand hochzählen (`2` → `3` usw.). Die Basisversion samt Debian-Revision kommt aus `debian/changelog`, die Architektur aus `dpkg`.

```bash
(
set -e  # Bei Fehler abbrechen, ohne die aufrufende Shell zu beenden

# Bei jeder neuen Paketfassung nur diese Zahl erhöhen:
CTPROJ_REV=2

# Nach Quellcodeänderungen das Programm neu bauen:
./build.sh release

BASE_VERSION="$(dpkg-parsechangelog -SVersion)"  # z. B. 1.7.2-2
DEBIAN_REV="${BASE_VERSION##*-}"             # z. B. 2
ARCH="$(dpkg --print-architecture)"           # z. B. amd64
PACKAGE_VERSION="${BASE_VERSION}+ctproj${CTPROJ_REV}"
PACKAGE_STEM="cherrytree_${PACKAGE_VERSION}_${ARCH}"
PACKAGE_FILE="build/${PACKAGE_STEM}.deb"

(
    cd build
    cpack -G DEB \
        -D "CPACK_DEBIAN_PACKAGE_RELEASE=${DEBIAN_REV}+ctproj${CTPROJ_REV}" \
        -D "CPACK_PACKAGE_FILE_NAME=${PACKAGE_STEM}"
)

# Interne Paketversion prüfen; bei Abweichung abbrechen:
dpkg-deb -f "$PACKAGE_FILE" Package Version Architecture
test "$(dpkg-deb -f "$PACKAGE_FILE" Version)" = "$PACKAGE_VERSION"

# Eigenständige Installationsdatei mit dauerhaft gleichem Namen:
cp -f -- "$PACKAGE_FILE" build/cherrytree-local.deb
)
```

Ergebnis bei `CTPROJ_REV=2`: `build/cherrytree_1.7.2-2+ctproj2_amd64.deb` und eine **inhaltlich identische, eigenständige** Datei `build/cherrytree-local.deb`. Die zweite Datei kann separat weitergegeben oder später installiert werden, ohne die Versionsnummer kennen zu müssen. Sie ist ausdrücklich **kein Symlink**. Ein erneuter Build überschreibt sie mit der jeweils neu erzeugten Paketfassung.

Für `dpkg-parsechangelog` wird `dpkg-dev` benötigt. Wenn ausschließlich die Paketrevision geändert und seit dem letzten Build kein Quellcode verändert wurde, kann `./build.sh release` entfallen; die übrigen Befehle bleiben gleich. Die CPack-Konfiguration wird von `CMakeLists.txt` unter `build/CPackConfig.cmake` erzeugt. Für den lokalen Revisionszähler weder `debian/changelog` noch die Build-Dateien manuell ändern.

### 3. Debian-Paket installieren (mit `sudo`)

**Dieser Schritt ist unabhängig von Abschnitt 2.** Der installierende Benutzer benötigt nur die fertige `.deb`-Datei. Weder `CTPROJ_REV` noch andere Variablen aus dem Build sind erforderlich. Liegt die Datei an anderer Stelle, den Pfad entsprechend ersetzen.

Vor der Installation CherryTree einschließlich möglicher Tray-Instanzen schließen und die verwendeten `.ctb`-Dateien sowie `~/.config/cherrytree/` sichern.

```bash
# Im Quellverzeichnis, falls das lokale Paket dort bereitliegt:
dpkg-deb -f ./build/cherrytree-local.deb Package Version Architecture
sudo apt -s install ./build/cherrytree-local.deb

# Nach Prüfung der Simulation tatsächlich installieren:
sudo apt install ./build/cherrytree-local.deb
```

Alternativ kann jede beliebig benannte `.deb`-Datei direkt angegeben werden, etwa `sudo apt install /pfad/zum/paket.deb`; entscheidend ist **die intern gespeicherte Version**, nicht der Dateiname. Das bestehende Paket `cherrytree` wird über die Paketverwaltung ersetzt, ein vorheriges `apt remove` oder `apt purge` ist nicht nötig. Bereits laufende CherryTree-Prozesse verwenden weiterhin ihren alten Code, bis sie beendet und neu gestartet werden.

Nach der Installation prüfen:

```bash
apt policy cherrytree
cherrytree project --help
```

### 4. Kurzname `ct`: Alias oder Symlink

Aktuell ist `ct` auf dem Testsystem bereits ein Shell-Alias für `cherrytree`:

```bash
type -a ct
alias ct
```

Das reicht für den **interaktiven Terminalbetrieb**. Der Alias verursacht nicht die Meldung `Open exactly one .ctb document`; diese kommt aus der Dateiauswahl der Projekt-CLI.

Für CS oder Skripte ist ein echter ausführbarer Name im `PATH` geeigneter, da Bash-Aliasse in nichtinteraktiven Aufrufen normalerweise nicht expandiert werden. Dafür optional:

```bash
mkdir -p ~/.local/bin
ln -s /usr/bin/cherrytree ~/.local/bin/ct
```

Vorher sicherstellen, dass unter `~/.local/bin/ct` noch nichts existiert. Der Symlink setzt voraus, dass `~/.local/bin` im `PATH` liegt. Ein weiterhin vorhandener gleichnamiger Shell-Alias hat in der interaktiven Bash Vorrang; er kann in der bereits verwendeten Shell-/shenv-Konfiguration entfernt werden. Für den laufenden Shell-Prozess genügt `unalias ct`.

**Wichtig für Dateiauswahl und Tests:**

```bash
# Eindeutige, lesende Abfrage, unabhängig von offenen GUI-Fenstern:
ct /pfad/gedaechtnis.test.ctb project list -a

# Ohne Dateipfad nur bei genau einer offenen CTB im selben Anwendungsprozess:
ct project list

# Eigene Sitzung (andere Anwendungskennung), mit expliziter Testdatei:
ct -S /pfad/gedaechtnis.test.ctb project list
```

`ct project list` sieht **keine** ausschließlich mit `-S` geöffnete CTB. Sind mehrere Dateien geöffnet oder ist kein passendes Dokument vorhanden, muss der Pfad explizit angegeben werden. Schreibende Aufrufe immer erst an einer CTB-Kopie prüfen.

### 5. Paketstand sichern und spätere Updates

Das getestete `.deb` aufbewahren. Optional kann eine automatische Paketaktualisierung verhindert werden:

```bash
sudo apt-mark hold cherrytree
```

Bei einer bewusst gewünschten späteren Aktualisierung zuerst `sudo apt-mark unhold cherrytree` verwenden. **Für die nächste Paketfassung in Abschnitt 2 ausschließlich `CTPROJ_REV` erhöhen** und den dortigen Block erneut ausführen. `CPACK_DEBIAN_PACKAGE_RELEASE`, Dateiname und Prüfpfad werden daraus abgeleitet. Nicht lediglich eine `.deb`-Datei umbenennen: Das ändert die intern gespeicherte Paketversion nicht. Nach jeder Paketgenerierung mit `dpkg-deb -f` kontrollieren.

Die Git-Historie bleibt getrennt: Der Upstream-PR basiert auf `d1ba1c16`, die lokale Erweiterung auf dem nachfolgenden Commit `1450087b`. Nach Änderungen am gepushten `local/project-cli` bevorzugt neue Folgecommits erstellen, nicht unnötig den veröffentlichten Commit amendieren.

## Quellcode und Pflege des Forks

| Datei | Aufgabe |
|---|---|
| `src/ct/ct_project_cli.h` | Parser, Subcommands, Präfixe, Option `--all-occurrences` und Hilfe |
| `src/ct/ct_actions_project.cc` | Projektregister, `create`, `list`, `rename`, `delete`; Link- und Textaktualisierung |
| `src/ct/ct_app.cc` | Aufruf über die CherryTree-Anwendungsinstanz, Dateiauswahl und Speicherung |
| `src/ct/ct_actions.h` | Deklarationen der Projektfunktionen |
| `src/ct/ct_main.cc` | CLI-Ausgabe und Dispatcher-Anbindung |
| `src/ct/CMakeLists.txt` | Einbindung der Projektquelldatei in den Build |
| `tests/test_local_project_cli.py` | End-to-End-Regression mit selbst erzeugten Projekten und SQLite-Integritätsprüfung |

Die lokale Entwicklung gehört auf den Branch `local/project-cli`. Der Upstream-PR #2943 für `--node-id` und `--duplicate` ist davon **getrennt**. Beim Nachziehen neuer CherryTree-Versionen die lokalen Commits gezielt auf einen passenden Upstream-Stand übertragen, mögliche Überschneidungen mit inzwischen integrierten Upstream-Änderungen kontrollieren und anschließend Build sowie Projektfunktionen erneut testen.

**Nicht Bestandteil dieses Standes:** eine generische Projektverwaltungs-API, eine automatische Neustrukturierung der Ordner, vollständige Dekadenauflösung und automatische Bereinigung aller externen Links beim Löschen.
