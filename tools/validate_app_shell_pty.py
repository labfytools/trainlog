#!/usr/bin/env python3
"""Exercise APP_SHELL_V1 through a real Notcurses process in an isolated tmux PTY.

This is deliberately a black-box companion to the C geometry tests.  Capture-pane
is text, so it proves routes, overlays, footer reservation and interaction; it does
not infer terminal cell widths from Python UTF-8 string lengths.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import uuid
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SIZES = ((120, 35), (120, 31), (100, 30), (100, 25), (80, 24), (72, 20))


class Failure(RuntimeError):
    pass


class PtyValidation:
    def __init__(self, binary, output):
        self.binary = binary.resolve()
        self.output_root = output.resolve()
        self.output = self.output_root / ("run-" + uuid.uuid4().hex)
        self.data_home = self.output / "xdg-data"
        self.socket = f"trainlog-app-shell-{os.getpid()}-{uuid.uuid4().hex[:8]}"
        self.session = "app-shell"
        self.checks = []
        self.captures = {}
        self.height = 35

    def tmux(self, *args, timeout=5, check=True):
        command = ["tmux", "-L", self.socket, *args]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=timeout)
        if check and result.returncode:
            raise Failure("tmux failed: %s\nstderr: %s" % (command, result.stderr.strip()))
        return result

    def send(self, *keys):
        self.tmux("send-keys", "-t", self.session, *keys)
        time.sleep(0.35)

    def literal(self, value):
        self.tmux("send-keys", "-t", self.session, "-l", value)
        time.sleep(0.35)

    def capture(self, name):
        text = self.tmux("capture-pane", "-p", "-t", self.session).stdout
        path = self.output / f"pty-{name}.txt"
        path.write_text(text, encoding="utf-8")
        self.captures[name] = path.name
        return text

    def require(self, condition, name, detail=""):
        if not condition:
            raise Failure("assertion failed: %s%s" % (name, ": " + detail if detail else ""))
        self.checks.append(name)

    def footer(self, text):
        rows = text.splitlines()
        return "\n".join(rows[-2:])

    def check_shell(self, name, text, marker):
        rows = text.splitlines()
        self.require(len(rows) == self.height, name + ": terminal height")
        self.require("TRAINLOG" in rows[0], name + ": header")
        # CONTRACT: section labels in the persistent sidebar are not evidence
        # that a destination opened. Only the reserved route header identifies it.
        route = "Accueil" if marker in ("Votre entraînement", "Accueil") else marker
        self.require(rows[1].strip().startswith(route), name + ": active route", route)
        self.require(marker in text, name + ": route marker", marker)
        self.require("F7 Actions" in self.footer(text), name + ": reserved two-line footer")

    def resize(self, width, height):
        self.tmux("resize-window", "-t", self.session, "-x", str(width), "-y", str(height))
        self.height = height
        time.sleep(0.45)

    def pane_dead(self):
        return self.tmux("display-message", "-p", "-t", self.session,
                         "#{pane_dead}").stdout.strip()

    def data_snapshot(self):
        """Hash initialized data after startup; navigation must not mutate it."""
        snapshot = {}
        for path in self.data_home.rglob("*"):
            if path.is_file():
                snapshot[str(path.relative_to(self.data_home))] = hashlib.sha256(
                    path.read_bytes()).hexdigest()
        return snapshot

    @staticmethod
    def selected_list_line(text):
        """Return the content selection marker, excluding the sidebar marker."""
        for line in text.splitlines():
            # Content may share a physical row with a sidebar label. Its marker
            # follows the content padding; the sidebar marker starts in column 1.
            selected = re.search(r" {2,}(>\s+\S.*)", line)
            if selected:
                return selected.group(1).strip()
        return ""

    def run(self):
        self.output.mkdir(parents=True, exist_ok=False)
        self.data_home.mkdir(parents=True, exist_ok=True)
        if not self.binary.is_file() or not os.access(self.binary, os.X_OK):
            raise Failure("binary is not executable: %s" % self.binary)
        # A distinct server/socket and XDG directory make this run unable to touch
        # a user's tmux server or normal Trainlog database.  Do not set HOME.
        environment = ["env", "XDG_DATA_HOME=" + str(self.data_home),
                       "TERM=xterm-256color", str(self.binary)]
        self.tmux("new-session", "-d", "-s", self.session, "-x", "120", "-y", "35", *environment,
                  timeout=8)
        self.tmux("set-option", "-t", self.session, "remain-on-exit", "on")
        # Notcurses may wait for DA.  Send it as terminal input, then normalize any
        # resulting '?' help route with a physical Escape and explicit Home route.
        self.literal("\x1b[?1;2c")
        self.send("Escape")
        self.send("0")
        time.sleep(0.6)
        home = self.capture("home")
        self.check_shell("home", home, "Votre entraînement")
        database_before_navigation = self.data_snapshot()

        # All six contract dimensions must retain a usable shell and footer.
        for width, height in SIZES:
            self.resize(width, height)
            view = self.capture(f"size-{width}x{height}")
            self.check_shell(f"geometry-{width}x{height}", view,
                             "Votre entraînement" if width >= 80 else "Accueil")
        self.resize(120, 35)

        self.send("?")
        help_view = self.capture("help")
        self.require("Aide contextuelle" in help_view, "question opens help")
        self.send("Escape")
        self.check_shell("escape closes help", self.capture("after-help-escape"), "Votre entraînement")

        # F6 is a sidebar focus at wide sizes, and an overlay at compact sizes.
        self.send("F6", "Down", "Down", "Down", "Enter")
        equipment = self.capture("equipment-from-f6")
        self.check_shell("F6 navigation opens equipment", equipment, "Équipements")
        self.send("F6", "Escape")
        restored = self.capture("equipment-after-f6-escape")
        self.check_shell("F6 Escape restores equipment", restored, "Équipements")
        self.send("Tab", "BTab")
        self.check_shell("Tab and Shift-Tab preserve route", self.capture("equipment-after-tabs"), "Équipements")

        # A sidebar focus survives compact resize as the visible Navigation
        # control. Its overlay restores both focus and section selection.
        self.send("F6", "Down", "Down", "Down")
        self.resize(100, 25)
        compact_navigation = self.capture("compact-navigation-focus")
        self.require("> F6 Navigation" in compact_navigation,
                     "sidebar focus becomes visible compact Navigation control")
        self.send("Enter")
        self.require("Navigation" in self.capture("compact-navigation-overlay"),
                     "compact Navigation Enter opens overlay")
        self.send("Escape")
        restored_navigation = self.capture("compact-navigation-restored")
        self.require("> F6 Navigation" in restored_navigation,
                     "Navigation Escape restores compact control focus")
        self.send("BTab")
        self.require("> F7 Actions" in self.capture("compact-reverse-actions"),
                     "reverse Tab reaches rendered Actions control")
        self.send("BTab", "BTab", "BTab")
        self.require("> F6 Navigation" in self.capture("compact-reverse-navigation"),
                     "reverse Tab follows actions, content, search, navigation")
        self.send("Tab", "Tab")
        self.resize(120, 35)

        self.send("Down")
        before_detail = self.capture("equipment-selected")
        selected_before_detail = self.selected_list_line(before_detail)
        self.require(bool(selected_before_detail), "equipment selection visible before detail")
        self.send("Enter")
        detail = self.capture("equipment-detail")
        self.require("Équipements / Fiche" in detail, "equipment detail opens")
        self.send("Escape")
        back_to_list = self.capture("equipment-detail-back")
        self.check_shell("detail Escape restores equipment list", back_to_list, "Équipements")
        self.require("Équipements / Fiche" not in back_to_list,
                     "detail Escape restores list route")
        self.require(self.selected_list_line(back_to_list) == selected_before_detail,
                     "detail Escape restores equipment selection")

        self.send("F7")
        actions = self.capture("actions")
        self.require("Actions" in actions, "F7 opens actions")
        self.send("Escape")
        self.check_shell("F7 Escape restores equipment", self.capture("after-actions-escape"), "Équipements")

        # The minimum terminal still supports list search and overlay isolation.
        self.resize(72, 20)
        self.send("4")
        minimum_equipment = self.capture("minimum-equipment")
        self.check_shell("72x20 equipment", minimum_equipment, "Équipements")
        self.send("/", "l", "e", "g")
        filtered = self.capture("equipment-filtered")
        self.require("leg" in filtered.lower(), "live equipment filter")
        self.send("Escape")
        cleared = self.capture("equipment-search-cleared")
        self.require("Recherche : —  [saisie]" in cleared,
                     "first Escape clears query and retains search focus")
        self.send("Escape")
        search_closed = self.capture("equipment-search-closed")
        self.check_shell("second Escape returns equipment list", search_closed, "Équipements")
        self.require("[saisie]" not in search_closed, "second Escape clears search focus")
        self.send("?")
        self.require("Aide contextuelle" in self.capture("minimum-help"), "help at minimum geometry")
        self.send("3")
        self.require("Aide contextuelle" in self.capture("help-overlay-isolates-global-route"),
                     "global route ignored while help overlay open")
        self.resize(100, 25)
        self.require("Aide contextuelle" in self.capture("help-after-resize"), "resize preserves help overlay")
        self.send("Escape")
        self.check_shell("Escape restores equipment after overlay resize",
                         self.capture("after-overlay-resize-escape"), "Équipements")

        self.send("0", "q")
        time.sleep(0.5)
        self.require(self.pane_dead() == "1", "q exits pane")
        self.require(self.tmux("display-message", "-p", "-t", self.session,
                                "#{pane_dead_status}").stdout.strip() == "0", "q exits zero")
        self.require(self.data_snapshot() == database_before_navigation,
                     "navigation leaves initialized temporary database unchanged")

    def close(self):
        # This is our unique socket, never the caller's tmux server.
        self.tmux("kill-server", check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/tui/trainlog")
    parser.add_argument("--output", type=Path, default=Path("/tmp/trainlog-app-shell-v1"))
    args = parser.parse_args()
    runner = PtyValidation(args.binary, args.output)
    result = {"binary": str(args.binary), "output": str(runner.output), "checks": [], "captures": {}}
    try:
        runner.run()
        result.update(status="PASS", checks=runner.checks, captures=runner.captures)
        print("APP_SHELL_PTY=PASS")
        print("ARTIFACT_DIR=" + str(runner.output))
        for check in runner.checks:
            print("CHECK=" + check)
    except (Failure, subprocess.TimeoutExpired, OSError) as error:
        result.update(status="FAIL", error=str(error), checks=runner.checks, captures=runner.captures)
        print("APP_SHELL_PTY=FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        runner.output.mkdir(parents=True, exist_ok=True)
        (runner.output / "results.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                                                   encoding="utf-8")
        (runner.output / "pty-script.md").write_text(
            "# APP_SHELL_V1 PTY evidence\n\n"
            "`validate_app_shell_pty.py` starts the supplied binary in a unique tmux "
            "server with a temporary `XDG_DATA_HOME`. Captures are text snapshots of "
            "the actual pane. They verify route and overlay behavior and the reserved "
            "footer content at each requested geometry; they do not attempt to prove "
            "terminal-cell width from UTF-8 byte or Python-string length. The C geometry "
            "tests provide that complementary cell-bound coverage.\n",
            encoding="utf-8")
        runner.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
