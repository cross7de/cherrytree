# CherryTree – lokale Projekt-CLI

**Stand:** 28.10.2026 · **Lokaler Funktionsstand:** v1.5  
**Git-Zweig:** `local/project-cli` · **Status:** lokal erprobt, nicht Bestandteil des Upstream-PR #2943

Diese persönliche CherryTree-Erweiterung verwaltet Projekte über den Rich-Text-Knoten **`Projekt`**. Sie legt Projekte aus einer Vorlage an, listet das Projektregister auf, benennt Projekte um und löscht sie. Dabei werden die internen CherryTree-Verweise mitgepflegt.

Die Befehle ergänzen die bestehende CherryTree-CLI; sie ersetzen deren Optionen nicht. Der kurze Programmname `ct` kann als Shell-Alias oder als Symlink eingerichtet werden. Ohne ihn kann `cherrytree` beziehungsweise während der Entwicklung `./build/cherrytree` verwendet werden.

## Schnellstart

```text
ct [DATEI.ctb] project create "Projektname"
ct [DATEI.ctb] project list [-N | --years N | -a | --all]
ct [DATEI.ctb] project rename PNR "Neuer Projektname"
ct [DATEI.ctb] project delete PNR [-r | --recursive] [-y | --yes]
ct project --help
```

Beispiele mit einem ausdrücklich angegebenen Dokument:

```bash
ct ~/pfad/gedaechtnis.ctb project create "Mein Projekt"
ct ~/pfad/gedaechtnis.ctb project list -3
ct ~/pfad/gedaechtnis.ctb project rename 6P25 "Neuer Name"
ct ~/pfad/gedaechtnis.ctb project delete 6P25
```

**PNR** bezeichnet die sichtbare Projektnummer, z. B. `6P25`, **nicht** die interne numerische CherryTree-Knoten-ID.

### Kurzbefehle

Eindeutige Präfixe funktionieren pro Kommandoebene, z. B.:

```bash
ct p c "Mein Projekt"
ct p l -a
ct p r 6P25 "Neuer Name"
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

### `project rename PNR NAME`

Sucht das Projekt über die **vollständige PNR** und ändert gemeinsam:

- den sichtbaren Knotentitel;
- die erste Kopfzeile im Projektinhalt;
- die Beschriftung des internen Links im Projektregister.

**Unverändert** bleiben die PNR, die CT-ID und das Linkziel. Bei fehlendem, mehrdeutigem oder strukturell abweichendem Projekt bricht die Aktion mit einer Fehlermeldung ab.

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
- Keine externe SQLite-Schreiboperation: Die Änderungen und das Speichern laufen über CherryTree.
- Es gibt noch **keine automatische Rücknahme** eines begonnenen mehrstufigen Vorgangs. Vor schreibenden Operationen daher Sicherungen vorsehen. Ein Speicherfehler kann Änderungen im geöffneten GUI-Zustand hinterlassen.
- Synchronisationen und andere Instanzen sollten die **gleiche CTB nicht gleichzeitig verändern**.

**Exit-Status:** `0` bei Erfolg, ungleich `0` bei Fehler oder Abbruch. Ergebnisse werden auf `stdout`, Diagnosen auf `stderr` ausgegeben; GUI-/GTK-Diagnostik kann ebenfalls auf `stderr` erscheinen. `rename` muss bei Erfolg keinen Ergebnistext erzeugen.

## Hilfe und Tests

```bash
ct project --help
ct project help list
ct project delete --help
```

Zum Testen den **selbst gebauten CherryTree** mit `-S` (separate Sitzung) und einer **CTB-Arbeitskopie** verwenden, damit er nicht unbeabsichtigt die installierte oder produktive GUI-Instanz anspricht:

```bash
cp /pfad/gedaechtnis.test.ctb /tmp/ct-project-test.ctb

./build/cherrytree -S /tmp/ct-project-test.ctb project list -a
./build/cherrytree -S /tmp/ct-project-test.ctb project create "CLI-Test"
./build/cherrytree -S /tmp/ct-project-test.ctb project rename 6P25 "CLI-Test-neu"
./build/cherrytree -S /tmp/ct-project-test.ctb project delete 6P25
```

Das Beispiel für `rename`/`delete` setzt voraus, dass die tatsächlich angelegte PNR `6P25` ist. Bei der letzten Löschabfrage zunächst `n` eingeben; die echte Löschung erst nach Kontrolle durchführen. Danach die CTB erneut öffnen und Knoten, Referenzen sowie Platzhalter überprüfen.

Ein bereits laufender **älterer** CherryTree-Prozess kann CLI-Aufrufe über D-Bus abweisen. Die separate Testinstanz mit `-S` umgeht dies für die Entwicklung. Das Projekt baut weiterhin mit `./build.sh`; in diesem lokalen Stand wurden Build und Grundfunktionen auf Debian 13 / GTK3 praktisch geprüft.

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

**Aktueller Befund vom 28.10.2026:** Auf dem Testsystem ist laut `apt policy` Version `1.7.2-2` installiert; das neu erzeugte lokale `.deb` meldet intern Version `1.7.2`. Der Paketmanager bewertet `1.7.2` als **älter** als `1.7.2-2` und kündigt deshalb ein *Downgrade* an. Die Paketversionsnummer ist unabhängig davon, ob der Quellstand lokale Erweiterungen besitzt. `apt -s install …` ist nur eine Simulation und installiert noch nichts.

### 2. Debian-Paket erstellen und prüfen

Aus dem CherryTree-Repository auf dem gewünschten lokalen Commit:

```bash
./build.sh deb
ls -lh build/*.deb
```

Danach **den tatsächlichen Dateinamen** aus der Ausgabe verwenden. Beispiel für das bisher erzeugte Paket:

```bash
dpkg-deb -f build/cherrytree-1.7.2~Debian13_amd64.deb Package Version Architecture
sudo apt -s install ./build/cherrytree-1.7.2~Debian13_amd64.deb
```

In der Simulation auf unerwartete Installationen/Entfernungen achten. Besonders prüfen, ob `apt` ein Downgrade ankündigt. Bei einem bereits funktionierenden installierten Fork besteht **kein Grund**, allein wegen eines neu gebauten Pakets eine ältere Paketversionsnummer einzuspielen.

### 3. Bewusst installieren oder aktualisieren

Wenn die geprüfte Paketdatei tatsächlich installiert werden soll:

1. CherryTree einschließlich eventuell versteckter Tray-Instanzen regulär beenden.
2. Die verwendeten `.ctb`-Dokumente sowie `~/.config/cherrytree/` sichern.
3. Die zuvor geprüfte lokale Paketdatei installieren:

```bash
sudo apt install ./build/cherrytree-1.7.2~Debian13_amd64.deb
```

**Nur ausführen, wenn genau diese Paketdatei beabsichtigt ist.** Das bestehende Paket `cherrytree` wird über die Paketverwaltung ersetzt; ein vorgängiges `apt remove` oder `apt purge` ist nicht nötig. Die eigenen CTB-Dokumente liegen außerhalb der Paketdateien. Trotzdem vor jedem Upgrade sichern.

Danach prüfen:

```bash
apt policy cherrytree
cherrytree project --help
```

Bereits vor der Installation gestartete CherryTree-Prozesse verwenden weiterhin ihren alten Code, bis sie beendet und neu gestartet werden. Auf Linux kann ein nachfolgender CLI-Aufruf über D-Bus an genau diesen laufenden Primärprozess weitergeleitet werden.

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

Bei einer bewusst gewünschten späteren Aktualisierung zuerst `sudo apt-mark unhold cherrytree` verwenden. Für künftige eigene Builds bietet sich eine **eindeutig als lokal erkennbare Debian-Paketrevision** an (z. B. `1.7.2-2+local1` statt `1.7.2`). Dafür stellt CPack die Variable `CPACK_DEBIAN_PACKAGE_RELEASE` bereit. Das ist eine mögliche spätere Verbesserung der Paketierung, **noch keine in diesem Branch implementierte Einstellung**. Nach jeder Paketgenerierung die tatsächliche Version mit `dpkg-deb -f` kontrollieren.

Die Git-Historie bleibt getrennt: Der Upstream-PR basiert auf `d1ba1c16`, die lokale Erweiterung auf dem nachfolgenden Commit `1450087b`. Nach Änderungen am gepushten `local/project-cli` bevorzugt neue Folgecommits erstellen, nicht unnötig den veröffentlichten Commit amendieren.

## Quellcode und Pflege des Forks

| Datei | Aufgabe |
|---|---|
| `src/ct/ct_project_cli.h` | Parser, Subcommands, Präfixe und Hilfe |
| `src/ct/ct_actions_project.cc` | Projektregister, `create`, `list`, `rename`, `delete` |
| `src/ct/ct_app.cc` | Aufruf über die CherryTree-Anwendungsinstanz, Dateiauswahl und Speicherung |
| `src/ct/ct_actions.h` | Deklarationen der Projektfunktionen |
| `src/ct/ct_main.cc` | CLI-Ausgabe und Dispatcher-Anbindung |
| `src/ct/CMakeLists.txt` | Einbindung der Projektquelldatei in den Build |

Die lokale Entwicklung gehört auf den Branch `local/project-cli`. Der Upstream-PR #2943 für `--node-id` und `--duplicate` ist davon **getrennt**. Beim Nachziehen neuer CherryTree-Versionen die lokalen Commits gezielt auf einen passenden Upstream-Stand übertragen, mögliche Überschneidungen mit inzwischen integrierten Upstream-Änderungen kontrollieren und anschließend Build sowie Projektfunktionen erneut testen.

**Nicht Bestandteil dieses Standes:** eine generische Projektverwaltungs-API, eine automatische Neustrukturierung der Ordner, vollständige Dekadenauflösung und automatische Bereinigung aller externen Links beim Löschen.
