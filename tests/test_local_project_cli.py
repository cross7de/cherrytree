#!/usr/bin/env python3
"""Local end-to-end regression test for CherryTree's personal project CLI.

Runs exclusively on a temporary copy of the supplied CTB. Link test data is
added only to projects created by this test, with CherryTree closed.
"""
import argparse
import os
import re
import shutil
import signal
import sqlite3
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path


TARGET_TITLE = "CLI-Linktest-Ziel"
SOURCE_TITLE = "CLI-Linktest-Quelle"
RENAMED_TITLE = "CLI-Linktest-Umbenannt"
RICH_TEXT_SYNTAX = "custom-colors"


def project_template(database):
    with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as con:
        rows = con.execute("SELECT node_id, name FROM node").fetchall()
        if con.execute("PRAGMA quick_check").fetchone()[0] != "ok":
            raise RuntimeError("Die CTB besteht den SQLite-Integritätstest nicht")
    found = [(nid, name, re.fullmatch(r"([0-9]{1,2}P[0-9]{2}) - PLATZHALTER", name))
             for nid, name in rows]
    found = [(nid, name, match.group(1)) for nid, name, match in found if match]
    if len(found) != 1:
        raise RuntimeError("Die Test-CTB muss genau einen Projektplatzhalter enthalten")
    return found[0]


def next_pnr(pnr):
    year, number = pnr.split("P", 1)
    return f"{year}P{int(number) + 1:02d}"


def test_names(pnr):
    return f"{pnr} - {TARGET_TITLE}", f"{next_pnr(pnr)} - {SOURCE_TITLE}"


def run(binary, fixture, *args, success=True, error_contains=None):
    argv = [str(binary), "-S", str(fixture), "project", *args]
    # Separate files instead of PIPEs: desktop services started indirectly by
    # CherryTree may retain inherited output descriptors after the CLI exits.
    with tempfile.TemporaryFile(mode="w+t") as stdout, tempfile.TemporaryFile(mode="w+t") as stderr:
        result = subprocess.run(argv, text=True, stdout=stdout, stderr=stderr,
                                stdin=subprocess.DEVNULL, timeout=18, check=False)
        stdout.seek(0)
        stderr.seek(0)
        out = stdout.read()
        err = stderr.read()
    if (result.returncode == 0) != success or (error_contains and error_contains not in err):
        raise RuntimeError(f"Unerwartetes Ergebnis (Status {result.returncode}): {' '.join(args)}\n"
                           f"stdout: {out}\nstderr: {err}")
    return out.strip()


def confirm_node(db, node_id, expected_name=None):
    with sqlite3.connect(f"file:{db}?mode=ro", uri=True) as con:
        row = con.execute("SELECT name FROM node WHERE node_id=?", (node_id,)).fetchone()
    if expected_name is not None and (not row or row[0] != expected_name):
        raise RuntimeError(f"Knoten {node_id}: erwartet '{expected_name}', tatsächlich {row}")
    return row is not None


def find_created_node(db, name, original_ids):
    with sqlite3.connect(f"file:{db}?mode=ro", uri=True) as con:
        rows = con.execute("SELECT node_id, txt, syntax, is_ro FROM node WHERE name=?", (name,)).fetchall()
    if len(rows) != 1 or rows[0][0] in original_ids:
        raise RuntimeError(f"Linktest: kein eindeutig NEU angelegter Knoten '{name}'")
    node_id, xml, syntax, read_only = rows[0]
    # In CherryTree, custom-colors is the rich-text syntax. is_ro is a
    # bitfield: bit 0 denotes read-only; higher bits store the icon ID.
    if syntax != RICH_TEXT_SYNTAX or read_only is None or (read_only & 1):
        raise RuntimeError(
            f"Linktest: Testknoten {node_id} ist nicht schreibbarer Rich-Text "
            f"(syntax={syntax!r}, is_ro={read_only!r})")
    root = ET.fromstring(xml)
    if root.tag != "node" or not "".join(e.text or "" for e in root.iter("rich_text")).startswith("# " + name):
        raise RuntimeError(f"Linktest: unerwarteter Inhalt im Testknoten {node_id}")
    return node_id, root


def add_link(root, caption, destination=None, parts=None):
    ET.SubElement(root, "rich_text").text = "\n"
    values = parts if parts is not None else [caption]
    attrs = {"link": f"node {destination}"} if destination is not None else {}
    for value in values:
        ET.SubElement(root, "rich_text", attrs).text = value


def prepare_link_test(db, pnr, original_ids):
    """Only modify the two dedicated projects, with no CherryTree instance open."""
    target_name, source_name = test_names(pnr)
    target_id, target_xml = find_created_node(db, target_name, original_ids)
    source_id, source_xml = find_created_node(db, source_name, original_ids)
    if target_id == source_id:
        raise RuntimeError("Linktest: Ziel und Quelle haben dieselbe CT-ID")
    # CherryTree necessarily adds register links to newly created projects.
    # Allow exactly these two links in the overview, but reject any reference
    # from other pre-existing nodes (for example a dangling ID reused by us).
    with sqlite3.connect(f"file:{db}?mode=ro", uri=True) as con:
        overview_ids = [row[0] for row in con.execute(
            "SELECT node_id FROM node WHERE name='Projekt'")]
        existing = con.execute("SELECT node_id, txt FROM node WHERE syntax=?",
                               (RICH_TEXT_SYNTAX,)).fetchall()
    if len(overview_ids) != 1 or overview_ids[0] not in original_ids:
        raise RuntimeError("Linktest: Projektregister nicht eindeutig vorhanden")
    overview_id = overview_ids[0]
    _, register_links = link_spans(db, overview_id)
    for node_id, expected_name in ((target_id, target_name), (source_id, source_name)):
        captions = [caption for link, caption in register_links if link == f"node {node_id}"]
        if captions != [expected_name]:
            raise RuntimeError(
                f"Linktest: unerwarteter Registerverweis auf Testknoten {node_id}: {captions!r}")
    for node_id, xml in existing:
        if node_id not in original_ids or node_id == overview_id:
            continue
        root = ET.fromstring(xml)
        if any(e.attrib.get("link") in (f"node {target_id}", f"node {source_id}")
               for e in root.iter("rich_text")):
            raise RuntimeError(
                f"Linktest: vorhandener Knoten {node_id} verweist auf eine neue Test-ID")

    add_link(target_xml, target_name, target_id)  # self-reference
    add_link(source_xml, target_name, target_id)
    add_link(source_xml, None, target_id, ["Siehe ", target_name, " (wichtig)"])
    add_link(source_xml, target_name + " / " + target_name, target_id)
    add_link(source_xml, "Meine Projektnotizen", target_id)
    add_link(source_xml, target_name, source_id)  # another link target
    add_link(source_xml, target_name)             # ordinary unlinked text

    with sqlite3.connect(db) as con:
        for node_id, xml in ((target_id, target_xml), (source_id, source_xml)):
            con.execute("UPDATE node SET txt=? WHERE node_id=?",
                        (ET.tostring(xml, encoding="unicode"), node_id))
    # Validate the prepared fixture before invoking rename.
    assert_link_test(db, pnr, target_id, source_id, renamed=False)
    return target_id, source_id


def link_spans(db, node_id):
    with sqlite3.connect(f"file:{db}?mode=ro", uri=True) as con:
        row = con.execute("SELECT txt FROM node WHERE node_id=?", (node_id,)).fetchone()
    if not row:
        raise RuntimeError(f"Linktest: Knoten {node_id} fehlt")
    root = ET.fromstring(row[0])
    spans = [(element.attrib.get("link"), element.text or "")
             for element in root.iter("rich_text")]
    # Rich-text formatting can split a single contiguous linked caption.
    merged = []
    for link, caption in spans:
        if link is not None and merged and merged[-1][0] == link:
            merged[-1] = (link, merged[-1][1] + caption)
        else:
            merged.append((link, caption))
    return spans, merged


def assert_link_test(db, pnr, target_id, source_id, renamed, all_occurrences=False):
    old_name, _ = test_names(pnr)
    new_name = f"{pnr} - {RENAMED_TITLE}"
    expected_name = new_name if renamed else old_name
    if not confirm_node(db, target_id, expected_name):
        raise RuntimeError("Linktest: Zielprojekt fehlt")
    _, own = link_spans(db, target_id)
    raw, source = link_spans(db, source_id)
    expected = [
        (f"node {target_id}", expected_name),
        (f"node {target_id}", "Siehe " + expected_name + " (wichtig)"),
        (f"node {target_id}", expected_name + " / " + expected_name),
        (f"node {target_id}", "Meine Projektnotizen"),
        (f"node {source_id}", expected_name if renamed and all_occurrences else old_name),
    ]
    if (f"node {target_id}", expected_name) not in own:
        raise RuntimeError(f"Linktest: Selbstverweis falsch: {own!r}")
    for item in expected:
        if item not in source:
            raise RuntimeError(f"Linktest: Verweis fehlt oder falsch geändert: {item}; Segmente: {source!r}")
    # Plain text changes only when global replacement is explicitly requested.
    plain_text = "".join(caption for link, caption in raw if link is None)
    if renamed and all_occurrences:
        if old_name in plain_text or expected_name not in plain_text:
            raise RuntimeError("Linktest: globaler Text wurde nicht vollständig ersetzt")
    elif old_name not in plain_text:
        raise RuntimeError("Linktest: gewöhnlicher Text wurde verändert")
    if renamed and any(old_name in caption for link, caption in source if link == f"node {target_id}"):
        raise RuntimeError("Linktest: alter Projektname in Zielverweis verblieben")


def original_nodes(db):
    """Remember all existing nodes except the register and managed placeholder."""
    with sqlite3.connect(f"file:{db}?mode=ro", uri=True) as con:
        return {node_id: (name, txt) for node_id, name, txt in
                con.execute("SELECT node_id, name, txt FROM node")
                if name != "Projekt" and not re.fullmatch(
                    r"[0-9]{1,2}P[0-9]{2} - PLATZHALTER", name)}


def inside(binary, database, pnr, phase):
    # Keep one GUI instance open per phase. The C++ data can only be modified
    # between phases, after the process has terminated completely.
    log = tempfile.TemporaryFile(mode="w+t")
    app = subprocess.Popen([str(binary), "-S", str(database)],
                           stdout=log, stderr=log, stdin=subprocess.DEVNULL)
    try:
        for _ in range(40):
            if app.poll() is not None:
                raise RuntimeError(f"CherryTree wurde mit Status {app.returncode} beendet")
            try:
                run(binary, database, "list", "--all")
                break
            except (RuntimeError, subprocess.TimeoutExpired):
                time.sleep(0.25)
        else:
            raise RuntimeError("Die Testinstanz ist nicht erreichbar")

        if phase == "create":
            template_id, original_name, current_pnr = project_template(database)
            if current_pnr != pnr:
                raise RuntimeError("Linktest: unerwartete Nummer des Projektplatzhalters")
            title = "CLI-Regression-Neu"
            renamed = "CLI-Regression-Umbenannt"
            project_name = f"{pnr} - {title}"
            renamed_name = f"{pnr} - {renamed}"
            run(binary, database, "delete", "NOT-A-PNR", "--yes", success=False,
                error_contains="Project number not found or ambiguous")
            new_id_text = run(binary, database, "create", title)
            if not re.fullmatch(r"[0-9]+", new_id_text):
                raise RuntimeError(f"Create muss nur eine numerische CT-ID ausgeben: {new_id_text!r}")
            new_id = int(new_id_text)
            confirm_node(database, new_id, project_name)

            # Regression: the GUI instance stays open while we immediately rename
            # the saved project, without an intervening manual Ctrl+S.
            # --node-id navigation is deliberately NOT part of this test: it is a
            # separate GUI focus operation and previously caused an unrelated timeout.
            run(binary, database, "rename", pnr, renamed)
            confirm_node(database, new_id, renamed_name)
            if renamed_name not in run(binary, database, "list", "--all"):
                raise RuntimeError("Die Projektübersicht zeigt den umbenannten Knoten nicht")
            run(binary, database, "delete", str(new_id), "--yes", success=False,
                error_contains="Project number not found or ambiguous")
            run(binary, database, "delete", pnr, "--yes")
            if confirm_node(database, new_id):
                raise RuntimeError("Der gelöschte Projektknoten existiert weiterhin")
            confirm_node(database, template_id, original_name)
            print(f"OK: create -> rename -> delete in derselben GUI-Sitzung ({pnr})")

            # Create separate projects for the link test; no existing project is used.
            target_name, source_name = test_names(pnr)
            for name in (target_name, source_name):
                number, title = name.split(" - ", 1)
                new_id_text = run(binary, database, "create", title)
                if not new_id_text.isdecimal() or not confirm_node(database, int(new_id_text), name):
                    raise RuntimeError(f"Linktest: Projekt '{number}' nicht erzeugt")
            print("OK: zwei eigene Linktest-Projekte angelegt")
        elif phase in ("rename", "rename-global"):
            all_occurrences = phase == "rename-global"
            target_name, source_name = test_names(pnr)
            with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as con:
                ids = [con.execute("SELECT node_id FROM node WHERE name=?", (name,)).fetchone()
                       for name in (target_name, source_name)]
            if any(row is None for row in ids):
                raise RuntimeError("Linktest: die eigenen Testprojekte fehlen")
            target_id, source_id = (ids[0][0], ids[1][0])
            assert_link_test(database, pnr, target_id, source_id, renamed=False)
            if all_occurrences:
                # Invalid options must fail before opening/modifying the document.
                run(binary, database, "rename", pnr, RENAMED_TITLE,
                    "--all-occurrences", "--all-occurrences", success=False,
                    error_contains="rename")
                assert_link_test(database, pnr, target_id, source_id, renamed=False)
            args = ("rename", pnr, RENAMED_TITLE)
            if all_occurrences:
                args += ("--all-occurrences",)
            run(binary, database, *args)
            assert_link_test(database, pnr, target_id, source_id, renamed=True,
                             all_occurrences=all_occurrences)
            # Delete in reverse creation order, so the placeholder can be restored.
            run(binary, database, "delete", next_pnr(pnr), "--yes")
            run(binary, database, "delete", pnr, "--yes")
            if confirm_node(database, target_id) or confirm_node(database, source_id):
                raise RuntimeError("Linktest: Testprojekte wurden nicht gelöscht")
            if all_occurrences:
                print("OK: --all-occurrences ersetzt verlinkten und gewöhnlichen Text")
            else:
                print("OK: interne Links auf eigene Projekte umbenannt und bereinigt")
        elif phase == "create-global":
            # Same self-created projects as the normal mode, no baseline repetition.
            for name in test_names(pnr):
                number, title = name.split(" - ", 1)
                new_id_text = run(binary, database, "create", title)
                if not new_id_text.isdecimal() or not confirm_node(database, int(new_id_text), name):
                    raise RuntimeError(f"Linktest: Projekt '{number}' nicht erzeugt")
            print("OK: zwei eigene Projekte für --all-occurrences angelegt")
        else:
            raise RuntimeError(f"Unbekannte Testphase: {phase}")
    finally:
        if app.poll() is None:
            app.terminate()
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()
                app.wait(timeout=5)
        log.close()


def run_phase(binary, db, pnr, phase, work):
    cmd = ["dbus-run-session", "--", sys.executable, str(Path(__file__).resolve()),
           "--inside", "--phase", phase, "--pnr", pnr,
           "--binary", str(binary), "--fixture", str(db)]
    with (work / f"{phase}.log").open("w+") as output:
        proc = subprocess.Popen(cmd, env=dict(os.environ,
            XDG_CONFIG_HOME=str(work / "config"), XDG_CACHE_HOME=str(work / "cache")),
            stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            result = proc.wait(timeout=100)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
            raise RuntimeError(f"Testphase '{phase}': Timeout nach 100 Sekunden")
        output.seek(0)
        logs = output.read()
    if result != 0:
        raise RuntimeError(f"Testphase '{phase}' fehlgeschlagen (Status {result}):\n{logs[-8000:]}")
    messages = [line for line in logs.splitlines() if line.startswith("OK:")]
    if not messages:
        raise RuntimeError(f"Testphase '{phase}' ohne Erfolgsbestätigung:\n{logs[-4000:]}")
    for message in messages:
        print(message)


def main():
    parser = argparse.ArgumentParser(description=(
        "Testet CherryTree-Projektbefehle und Linkumbenennung ausschließlich mit "
        "selbst angelegten Projekten in einer temporären CTB-Kopie."))
    parser.add_argument("--binary", default="./build/cherrytree", help="Pfad zur selbst gebauten CherryTree-Binärdatei")
    parser.add_argument("--fixture", required=True, help="Vorhandene CTB mit 6Pxx- oder 26Pxx-Platzhalter")
    parser.add_argument("--inside", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--phase", choices=("create", "rename", "create-global", "rename-global"), help=argparse.SUPPRESS)
    parser.add_argument("--pnr", help=argparse.SUPPRESS)
    args = parser.parse_args()
    binary = Path(args.binary).resolve()
    fixture = Path(args.fixture).resolve()
    if not binary.is_file() or not fixture.is_file():
        parser.error("CherryTree-Binärdatei oder CTB-Testdatei nicht gefunden")
    if not args.inside and not (os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")):
        parser.error("Eine grafische Sitzung (X11/Wayland) ist erforderlich")
    template_id, template_name, pnr = project_template(fixture)
    if not args.inside and int(pnr.split("P")[1]) > 97:
        parser.error("Der Platzhalter muss bei P97 oder darunter liegen (zwei Testprojekte erforderlich)")

    if args.inside:
        if not args.phase or not args.pnr:
            parser.error("Ungültiger interner Testaufruf")
        # The create phase adds two projects and advances the placeholder.
        # The rename phases must therefore expect PNR+2, not the initial PNR.
        expected_placeholder = (next_pnr(next_pnr(args.pnr))
                                if args.phase in ("rename", "rename-global")
                                else args.pnr)
        if pnr != expected_placeholder:
            parser.error(f"Ungültiger Testzustand: Platzhalter {pnr}, "
                         f"erwartet {expected_placeholder}")
        inside(binary, fixture, args.pnr, args.phase)
        return

    if shutil.which("dbus-run-session") is None:
        parser.error("'dbus-run-session' fehlt (Paket dbus-daemon)")
    with tempfile.TemporaryDirectory(prefix="ct-project-test-") as temporary:
        work = Path(temporary)
        copy = work / "test.ctb"
        # SQLite's backup API includes a consistent snapshot even if the
        # original CTB has a WAL sidecar; the source is opened read-only.
        with sqlite3.connect(f"file:{fixture}?mode=ro", uri=True) as source, sqlite3.connect(copy) as target:
            source.backup(target)
        before = original_nodes(copy)
        target_name, _ = test_names(pnr)
        if any(target_name in name or target_name in (txt or "")
               for name, txt in before.values()):
            raise RuntimeError("Testfixture enthält bereits unseren Linktest-Projektnamen")
        with sqlite3.connect(f"file:{copy}?mode=ro", uri=True) as con:
            original_ids = {row[0] for row in con.execute("SELECT node_id FROM node")}
        run_phase(binary, copy, pnr, "create", work)
        prepare_link_test(copy, pnr, original_ids)
        run_phase(binary, copy, pnr, "rename", work)
        run_phase(binary, copy, pnr, "create-global", work)
        prepare_link_test(copy, pnr, original_ids)
        run_phase(binary, copy, pnr, "rename-global", work)
        # The GUI test phases have finished, but the consistency checks may
        # take noticeably longer on a larger CTB. Flush the notice immediately.
        print("INFO: GUI-Tests abgeschlossen. Abschließende Daten- und SQLite-Integritätsprüfung läuft; "
              "bitte bis zur letzten OK-Meldung nicht abbrechen.", flush=True)
        if original_nodes(copy) != before:
            raise RuntimeError("Ein vor Testbeginn vorhandener Knoten wurde verändert")
        confirm_node(copy, template_id, template_name)
        if project_template(copy)[2] != pnr:
            raise RuntimeError("Der Projektplatzhalter wurde nicht zurückgesetzt")
        with sqlite3.connect(f"file:{copy}?mode=ro", uri=True) as con:
            if con.execute("PRAGMA quick_check").fetchone()[0] != "ok":
                raise RuntimeError("SQLite quick_check fehlgeschlagen")
        print("OK: ursprüngliche Knoten unverändert, Testprojekte entfernt, CTB konsistent")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"FEHLER: {exc}", file=sys.stderr)
        sys.exit(1)
