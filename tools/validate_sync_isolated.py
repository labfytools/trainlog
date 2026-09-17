#!/usr/bin/env python3
"""Run Trainlog sync validation with private HOME/XDG/tmp data paths.

This harness isolates application data and test transports by environment and
explicit fixture paths. It is not a system sandbox and intentionally reuses the
already-provisioned Gradle dependency cache.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

SUCCESS = 0
PREFLIGHT_BLOCKED = 2
DEPENDENCY_MISSING = 3
ENVIRONMENT_FAILURE = 4
TEST_FAILURE = 5
INTERRUPTED = 130
MAX_LOG_BYTES = 8 * 1024 * 1024
TARGET_ANDROID_TEST = (
    "com.labfytools.trainlog.data.TrainlogRepositoryDraftTest."
    "finalizeIsAtomicAndDraftNeverExportsBeforeCompletion"
)
ENV_ANDROID_TEST = (
    "com.labfytools.trainlog.data.IsolatedTestEnvironmentTest."
    "private paths reach the Robolectric JVM"
)


class HarnessError(RuntimeError):
    def __init__(self, message: str, exit_code: int) -> None:
        super().__init__(message)
        self.exit_code = exit_code


@dataclass(frozen=True)
class RunPaths:
    root: Path
    home: Path
    data: Path
    config: Path
    cache: Path
    runtime: Path
    native_tmp: Path
    exchange: Path
    databases: Path
    reports: Path


def parse_java_major(output: str) -> int | None:
    match = re.search(r'(?:java|openjdk) version "([0-9]+)(?:\.|\")', output)
    return int(match.group(1)) if match else None


def resolve_jdk(explicit: str | None, inherited: dict[str, str]) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    else:
        if inherited.get("JAVA_HOME"):
            candidates.append(Path(inherited["JAVA_HOME"]))
        candidates.extend(
            [Path("/usr/lib/jvm/java-17-openjdk"), Path("/usr/lib/jvm/java-17-openjdk-amd64")]
        )
    seen: set[Path] = set()
    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        java = resolved / "bin/java"
        if not java.is_file() or not os.access(java, os.X_OK):
            continue
        result = subprocess.run(
            [str(java), "-version"], capture_output=True, text=True, check=False
        )
        version_output = result.stdout + result.stderr
        if result.returncode == 0 and parse_java_major(version_output) == 17:
            return resolved
        if explicit:
            raise HarnessError(
                f"JDK incompatible: {resolved} n'est pas un JDK 17 utilisable",
                PREFLIGHT_BLOCKED,
            )
    raise HarnessError(
        "JDK 17 introuvable; utiliser --jdk /chemin/vers/jdk17",
        DEPENDENCY_MISSING,
    )


def default_run_parent(inherited: dict[str, str]) -> Path:
    cache = inherited.get("XDG_CACHE_HOME")
    if cache:
        base = Path(cache).expanduser()
    else:
        home = inherited.get("HOME")
        if not home:
            raise HarnessError("HOME est absent", PREFLIGHT_BLOCKED)
        base = Path(home).expanduser() / ".cache"
    return base.resolve() / "trainlog/test-runs"


def check_parent(parent: Path) -> Path:
    try:
        parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        parent = parent.resolve(strict=True)
        if not parent.is_dir() or not os.access(parent, os.W_OK | os.X_OK):
            raise OSError("répertoire non accessible en écriture")
        probe = parent / f".write-probe-{os.getpid()}"
        descriptor = os.open(probe, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        os.write(descriptor, b"trainlog-test\n")
        os.close(descriptor)
        probe.unlink()
    except OSError as error:
        raise HarnessError(
            f"parent temporaire inutilisable: {parent}: {error}",
            ENVIRONMENT_FAILURE,
        ) from error
    return parent


def create_run_paths(parent: Path) -> RunPaths:
    os.umask(0o077)
    root = Path(tempfile.mkdtemp(prefix="run-", dir=check_parent(parent))).resolve()
    names = {
        "home": "home",
        "data": "xdg-data",
        "config": "xdg-config",
        "cache": "xdg-cache",
        "runtime": "xdg-runtime",
        "native_tmp": "tmp",
        "exchange": "exchange",
        "databases": "databases",
        "reports": "reports",
    }
    created: dict[str, Path] = {}
    for key, name in names.items():
        path = root / name
        path.mkdir(mode=0o700)
        path.chmod(0o700)
        created[key] = path
    return RunPaths(root=root, **created)


def build_environment(
    paths: RunPaths, jdk: Path, inherited: dict[str, str], gradle_cache: Path
) -> dict[str, str]:
    environment = inherited.copy()
    environment.update(
        {
            "HOME": str(paths.home),
            "XDG_DATA_HOME": str(paths.data),
            "XDG_CONFIG_HOME": str(paths.config),
            "XDG_CACHE_HOME": str(paths.cache),
            "XDG_RUNTIME_DIR": str(paths.runtime),
            "TMPDIR": str(paths.native_tmp),
            "JAVA_HOME": str(jdk),
            "GRADLE_USER_HOME": str(gradle_cache),
            "JAVA_TOOL_OPTIONS": f"-Djava.io.tmpdir={paths.native_tmp}",
            "TRAINLOG_TEST_RUN_ROOT": str(paths.root),
            "TRAINLOG_TEST_TMPDIR": str(paths.native_tmp),
            "TRAINLOG_TEST_EXCHANGE_DIR": str(paths.exchange),
            "TRAINLOG_TEST_DATABASE_DIR": str(paths.databases),
        }
    )
    # CONTRACT: real rclone configuration and production daemon endpoints are
    # not inherited by an isolated validation run.
    environment.pop("RCLONE_CONFIG", None)
    return environment


def command_label(arguments: list[str]) -> str:
    return " ".join(json.dumps(argument) for argument in arguments)


def classify_command_failure(log_path: Path) -> int:
    output = log_path.read_text(encoding="utf-8", errors="replace").lower()
    environment_markers = (
        "disk quota exceeded",
        "no space left on device",
        "unable to load robolectric native runtime library",
        "could not create the java virtual machine",
    )
    return (
        ENVIRONMENT_FAILURE
        if any(marker in output for marker in environment_markers)
        else TEST_FAILURE
    )


def run_command(
    arguments: list[str],
    *,
    cwd: Path,
    environment: dict[str, str],
    log_path: Path,
    active: set[subprocess.Popen[bytes]],
) -> None:
    log_path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    with log_path.open("wb") as log:
        os.chmod(log_path, 0o600)
        header = f"COMMAND {command_label(arguments)}\nCWD {cwd}\n".encode()
        log.write(header)
        try:
            process = subprocess.Popen(
                arguments,
                cwd=cwd,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
        except FileNotFoundError as error:
            log.write(f"DEPENDENCY_MISSING {error}\n".encode())
            raise HarnessError(
                f"outil ou dépendance manquante: {arguments[0]}", DEPENDENCY_MISSING
            ) from error
        active.add(process)
        captured = len(header)
        try:
            assert process.stdout is not None
            while True:
                chunk = process.stdout.read(65536)
                if not chunk:
                    break
                if captured < MAX_LOG_BYTES:
                    retained = chunk[: MAX_LOG_BYTES - captured]
                    log.write(retained)
                    captured += len(retained)
            process.stdout.close()
            return_code = process.wait()
        finally:
            # INVARIANT: an interrupted live child remains registered so the
            # caller can terminate its owned process group before returning.
            if process.poll() is not None:
                active.discard(process)
        log.write(f"\nEXIT {return_code}\n".encode())
    if return_code != 0:
        exit_code = classify_command_failure(log_path)
        raise HarnessError(
            f"commande en échec ({return_code}): {command_label(arguments)}; log={log_path}",
            exit_code,
        )


def stop_children(active: set[subprocess.Popen[bytes]]) -> None:
    for process in tuple(active):
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    deadline = time.monotonic() + 5.0
    for process in tuple(active):
        remaining = max(0.0, deadline - time.monotonic())
        try:
            process.wait(timeout=remaining)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
        active.discard(process)


def safe_cleanup(root: Path, parent: Path) -> None:
    resolved_parent = parent.resolve(strict=True)
    resolved_root = root.resolve(strict=True)
    if resolved_root.parent != resolved_parent or not resolved_root.name.startswith("run-"):
        raise HarnessError(
            f"refus de nettoyer une cible extérieure: {resolved_root}",
            ENVIRONMENT_FAILURE,
        )
    for entry in resolved_root.rglob("*"):
        if entry.is_symlink():
            raise HarnessError(
                f"refus de suivre/nettoyer le symlink: {entry}", ENVIRONMENT_FAILURE
            )
    shutil.rmtree(resolved_root)


def count_android_results(result_dir: Path) -> dict[str, int]:
    totals = {"tests": 0, "failures": 0, "errors": 0, "skipped": 0}
    files = list(result_dir.glob("TEST-*.xml"))
    if not files:
        raise HarnessError(
            f"aucun résultat Android dans {result_dir}", ENVIRONMENT_FAILURE
        )
    for result_file in files:
        suite = ET.parse(result_file).getroot()
        for key in totals:
            totals[key] += int(suite.attrib.get(key, "0"))
    totals["passed"] = (
        totals["tests"] - totals["failures"] - totals["errors"] - totals["skipped"]
    )
    return totals


def verify_gradle_jdk(
    repository: Path,
    environment: dict[str, str],
    reports: Path,
    active: set[subprocess.Popen[bytes]],
) -> None:
    log = reports / "gradle-version.log"
    run_command(
        [str(repository / "android/gradlew"), "--no-daemon", "--version"],
        cwd=repository / "android",
        environment=environment,
        log_path=log,
        active=active,
    )
    output = log.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"Launcher JVM:\s+17(?:\.|\s)", output)
    java_home = Path(environment["JAVA_HOME"]).resolve()
    daemon_line = re.search(r"^Daemon JVM:\s+(.+)$", output, re.MULTILINE)
    daemon = daemon_line is not None and str(java_home) in daemon_line.group(1)
    if not match or not daemon:
        raise HarnessError(
            f"Gradle n'utilise pas JDK 17 (voir {log})", ENVIRONMENT_FAILURE
        )


def required_tools(repository: Path) -> None:
    for executable in ("meson", "python3"):
        if shutil.which(executable) is None:
            raise HarnessError(f"outil manquant: {executable}", DEPENDENCY_MISSING)
    for path in (
        repository / "android/gradlew",
        repository / "tools/validate_json.py",
        repository / "tools/validate_import_contract.py",
    ):
        if not path.exists():
            raise HarnessError(f"prérequis absent: {path}", DEPENDENCY_MISSING)


def commands_for_suite(repository: Path, suite: str) -> list[tuple[str, list[str], Path]]:
    python = sys.executable
    gradle = str(repository / "android/gradlew")
    android = repository / "android"
    smoke = [
        (
            "sync-gap-characterization",
            [python, str(repository / "tests/test_sync_gap_characterization.py")],
            repository,
        ),
        (
            "android-environment-smoke",
            [
                gradle,
                "--no-daemon",
                "--rerun-tasks",
                ":app:testDebugUnitTest",
                "--tests",
                ENV_ANDROID_TEST,
            ],
            android,
        ),
    ]
    if suite == "smoke":
        return smoke
    if suite == "preflight":
        return []
    return [
        ("desktop-compile", ["meson", "compile", "-C", "build"], repository),
        (
            "desktop-tests",
            ["meson", "test", "-C", "build", "--print-errorlogs"],
            repository,
        ),
        (
            "desktop-asan-compile",
            ["meson", "compile", "-C", "build-asan"],
            repository,
        ),
        (
            "desktop-asan-tests",
            ["meson", "test", "-C", "build-asan", "--print-errorlogs"],
            repository,
        ),
        smoke[0],
        ("json-validator", [python, str(repository / "tools/validate_json.py")], repository),
        (
            "import-contract-validator",
            [python, str(repository / "tools/validate_import_contract.py")],
            repository,
        ),
        (
            "android-targeted",
            [
                gradle,
                "--no-daemon",
                "--rerun-tasks",
                ":app:testDebugUnitTest",
                "--tests",
                TARGET_ANDROID_TEST,
            ],
            android,
        ),
        (
            "android-full-and-assemble",
            [
                gradle,
                "--no-daemon",
                "--rerun-tasks",
                ":app:testDebugUnitTest",
                ":app:assembleDebug",
            ],
            android,
        ),
    ]


def write_summary(paths: RunPaths, payload: dict[str, object]) -> None:
    destination = paths.reports / "summary.json"
    destination.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    destination.chmod(0o600)


def parse_arguments(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=("preflight", "smoke", "full"), default="full")
    parser.add_argument("--jdk", help="JDK 17 home; auto-detected when omitted")
    parser.add_argument("--run-parent", type=Path, help="private run parent outside /tmp")
    parser.add_argument("--keep-run-dir", action="store_true", help="retain a successful run")
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_arguments(arguments if arguments is not None else sys.argv[1:])
    repository = Path(__file__).resolve().parents[1]
    inherited = os.environ.copy()
    active: set[subprocess.Popen[bytes]] = set()
    paths: RunPaths | None = None
    parent: Path | None = None
    try:
        required_tools(repository)
        jdk = resolve_jdk(options.jdk, inherited)
        parent = check_parent(
            options.run_parent.resolve()
            if options.run_parent
            else default_run_parent(inherited)
        )
        paths = create_run_paths(parent)
        original_home = Path(inherited.get("HOME", str(Path.home()))).resolve()
        gradle_cache = Path(
            inherited.get("GRADLE_USER_HOME", str(original_home / ".gradle"))
        ).resolve()
        if not gradle_cache.is_dir():
            raise HarnessError(
                f"cache Gradle préprovisionné absent: {gradle_cache}",
                DEPENDENCY_MISSING,
            )
        environment = build_environment(paths, jdk, inherited, gradle_cache)
        verify_gradle_jdk(repository, environment, paths.reports, active)
        executed: list[str] = []
        for label, command, cwd in commands_for_suite(repository, options.suite):
            print(f"RUN {label}: {command_label(command)}", flush=True)
            run_command(
                command,
                cwd=cwd,
                environment=environment,
                log_path=paths.reports / f"{label}.log",
                active=active,
            )
            executed.append(label)
        android_counts = None
        if options.suite == "full":
            android_counts = count_android_results(
                repository / "android/app/build/test-results/testDebugUnitTest"
            )
            if android_counts["failures"] or android_counts["errors"]:
                raise HarnessError(
                    f"résultats Android en échec: {android_counts}", TEST_FAILURE
                )
        summary: dict[str, object] = {
            "status": "success",
            "suite": options.suite,
            "run_root": str(paths.root),
            "jdk_home": str(jdk),
            "java_major": 17,
            "gradle_user_home": str(gradle_cache),
            "isolated_paths": {
                "home": str(paths.home),
                "data": str(paths.data),
                "config": str(paths.config),
                "cache": str(paths.cache),
                "runtime": str(paths.runtime),
                "tmp": str(paths.native_tmp),
                "exchange": str(paths.exchange),
                "databases": str(paths.databases),
            },
            "commands": executed,
            "android": android_counts,
        }
        write_summary(paths, summary)
        print(json.dumps(summary, indent=2, sort_keys=True))
        if options.keep_run_dir:
            print(f"RUN_RETAINED={paths.root}")
        else:
            safe_cleanup(paths.root, parent)
            print(f"RUN_CLEANED={paths.root}")
        return SUCCESS
    except KeyboardInterrupt:
        stop_children(active)
        if paths:
            print(f"INTERRUPTED RUN_RETAINED={paths.root}", file=sys.stderr)
        return INTERRUPTED
    except HarnessError as error:
        stop_children(active)
        if paths:
            try:
                write_summary(
                    paths,
                    {"status": "failed", "exit_code": error.exit_code, "error": str(error)},
                )
            except OSError:
                pass
            print(f"RUN_RETAINED={paths.root}", file=sys.stderr)
        print(f"ERROR[{error.exit_code}] {error}", file=sys.stderr)
        return error.exit_code


if __name__ == "__main__":
    sys.exit(main())
