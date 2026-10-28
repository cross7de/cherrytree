#!/usr/bin/env python3
"""Local end-to-end regression test for CherryTree's personal project CLI.

Copies the source fixture and runs the test in a private D-Bus session.
The supplied fixture is never modified.
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
from pathlib import Path


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


def inside(binary, database):
    # Open a persistent GUI instance. All subsequent commands must be sent
    # to this instance to catch stale GTK TextBuffer modified flags.
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

        template_id, original_name, pnr = project_template(database)
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
        output = run(binary, database, "list", "--all")
        if renamed_name not in output:
            raise RuntimeError("Die Projektübersicht zeigt den umbenannten Knoten nicht")

        # The numeric CT ID must not silently be accepted as a PNR.
        run(binary, database, "delete", str(new_id), "--yes", success=False,
            error_contains="Project number not found or ambiguous")
        run(binary, database, "delete", pnr, "--yes")
        if confirm_node(database, new_id):
            raise RuntimeError("Der gelöschte Projektknoten existiert weiterhin")
        confirm_node(database, template_id, original_name)
        with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as con:
            if con.execute("PRAGMA quick_check").fetchone()[0] != "ok":
                raise RuntimeError("SQLite quick_check fehlgeschlagen")
        print(f"OK: create -> rename -> delete in derselben GUI-Sitzung ({pnr})")
    finally:
        if app.poll() is None:
            app.terminate()
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()
                app.wait(timeout=5)
        log.close()


def main():
    parser = argparse.ArgumentParser(description=(
        "Testet create, rename und delete sicher an einer Kopie einer CTB; "
        "die angegebene Datei bleibt unverändert."))
    parser.add_argument("--binary", default="./build/cherrytree", help="Pfad zur selbst gebauten CherryTree-Binärdatei")
    parser.add_argument("--fixture", required=True, help="Vorhandene CTB mit 6Pxx- oder 26Pxx-Platzhalter")
    parser.add_argument("--inside", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    binary = Path(args.binary).resolve()
    fixture = Path(args.fixture).resolve()
    if not binary.is_file() or not fixture.is_file():
        parser.error("CherryTree-Binärdatei oder CTB-Testdatei nicht gefunden")
    if not args.inside and not (os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")):
        parser.error("Eine grafische Sitzung (X11/Wayland) ist erforderlich")
    project_template(fixture)

    if args.inside:
        inside(binary, fixture)
        return

    if shutil.which("dbus-run-session") is None:
        parser.error("'dbus-run-session' fehlt (Paket dbus-daemon)")
    with tempfile.TemporaryDirectory(prefix="ct-project-test-") as temporary:
        work = Path(temporary)
        copy = work / "test.ctb"
        shutil.copy2(fixture, copy)
        env = dict(os.environ)
        env.update(XDG_CONFIG_HOME=str(work / "config"), XDG_CACHE_HOME=str(work / "cache"))
        cmd = ["dbus-run-session", "--", sys.executable, str(Path(__file__).resolve()),
               "--inside", "--binary", str(binary), "--fixture", str(copy)]
        with (work / "test.log").open("w+") as output:
            try:
                proc = subprocess.Popen(cmd, env=env, stdout=output, stderr=subprocess.STDOUT,
                                        start_new_session=True)
                try:
                    result = proc.wait(timeout=100)
                except subprocess.TimeoutExpired:
                    os.killpg(proc.pid, signal.SIGKILL)
                    proc.wait()
                    raise RuntimeError("Test-Timeout nach 100 Sekunden")
            finally:
                output.seek(0)
                logs = output.read()
                if "OK: create -> rename -> delete" in logs:
                    print(logs[logs.index("OK: create -> rename -> delete"):].strip())
                else:
                    print(logs[-8000:], file=sys.stderr)
        if result != 0:
            raise RuntimeError(f"Regressionstest fehlgeschlagen (Status {result})")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"FEHLER: {exc}", file=sys.stderr)
        sys.exit(1)
