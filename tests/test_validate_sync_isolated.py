#!/usr/bin/env python3
"""Unit tests for the isolated validation harness; no production transport."""

import importlib.util
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools/validate_sync_isolated.py"
SPEC = importlib.util.spec_from_file_location("validate_sync_isolated", MODULE_PATH)
assert SPEC and SPEC.loader
HARNESS = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = HARNESS
SPEC.loader.exec_module(HARNESS)


class IsolatedValidationHarnessTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="trainlog-harness-test-")
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def fake_jdk(self, version="17.0.12"):
        jdk = self.root / f"jdk-{version}"
        binary = jdk / "bin/java"
        binary.parent.mkdir(parents=True)
        binary.write_text(f'#!/bin/sh\necho \'openjdk version "{version}"\' >&2\n')
        binary.chmod(0o700)
        return jdk

    def test_required_jdk_is_detected_and_incompatible_jdk_is_rejected(self):
        good = self.fake_jdk()
        self.assertEqual(good.resolve(), HARNESS.resolve_jdk(str(good), {}))
        bad = self.fake_jdk("21.0.4")
        with self.assertRaisesRegex(HARNESS.HarnessError, "JDK incompatible") as raised:
            HARNESS.resolve_jdk(str(bad), {})
        self.assertEqual(HARNESS.PREFLIGHT_BLOCKED, raised.exception.exit_code)
        with self.assertRaisesRegex(HARNESS.HarnessError, "introuvable"):
            HARNESS.resolve_jdk(str(self.root / "missing"), {})

    def test_unusable_parent_is_an_environment_failure(self):
        regular_file = self.root / "not-a-directory"
        regular_file.write_text("sentinel")
        with self.assertRaises(HARNESS.HarnessError) as raised:
            HARNESS.check_parent(regular_file)
        self.assertEqual(HARNESS.ENVIRONMENT_FAILURE, raised.exception.exit_code)

    def test_two_runs_have_private_distinct_paths_and_effective_tmpdir(self):
        parent = self.root / "runs"
        first = HARNESS.create_run_paths(parent)
        second = HARNESS.create_run_paths(parent)
        self.assertNotEqual(first.root, second.root)
        environment = HARNESS.build_environment(
            first, self.fake_jdk(), {"HOME": str(self.root)}, self.root
        )
        for name in (
            "HOME",
            "XDG_DATA_HOME",
            "XDG_CONFIG_HOME",
            "XDG_CACHE_HOME",
            "XDG_RUNTIME_DIR",
            "TMPDIR",
            "TRAINLOG_TEST_EXCHANGE_DIR",
            "TRAINLOG_TEST_DATABASE_DIR",
        ):
            self.assertTrue(Path(environment[name]).resolve().is_relative_to(first.root))
        self.assertEqual(
            f"-Djava.io.tmpdir={first.native_tmp}", environment["JAVA_TOOL_OPTIONS"]
        )
        self.assertEqual(0o700, stat.S_IMODE(first.root.stat().st_mode))

    def test_child_failure_is_propagated_and_logged(self):
        paths = HARNESS.create_run_paths(self.root / "runs")
        with self.assertRaises(HARNESS.HarnessError) as raised:
            HARNESS.run_command(
                [sys.executable, "-c", "raise SystemExit(7)"],
                cwd=self.root,
                environment=os.environ.copy(),
                log_path=paths.reports / "failure.log",
                active=set(),
            )
        self.assertEqual(HARNESS.TEST_FAILURE, raised.exception.exit_code)
        self.assertIn("EXIT 7", (paths.reports / "failure.log").read_text())

    def test_missing_child_and_robolectric_quota_are_classified(self):
        paths = HARNESS.create_run_paths(self.root / "runs")
        with self.assertRaises(HARNESS.HarnessError) as missing:
            HARNESS.run_command(
                [str(self.root / "missing-tool")],
                cwd=self.root,
                environment=os.environ.copy(),
                log_path=paths.reports / "missing.log",
                active=set(),
            )
        self.assertEqual(HARNESS.DEPENDENCY_MISSING, missing.exception.exit_code)
        environment_log = paths.reports / "environment.log"
        environment_log.write_text("Unable to load Robolectric native runtime library\nDisk quota exceeded\n")
        self.assertEqual(
            HARNESS.ENVIRONMENT_FAILURE,
            HARNESS.classify_command_failure(environment_log),
        )

    def test_android_counts_include_skips(self):
        result_dir = self.root / "results"
        result_dir.mkdir()
        (result_dir / "TEST-one.xml").write_text(
            '<testsuite tests="5" failures="1" errors="0" skipped="2"/>'
        )
        self.assertEqual(
            {"tests": 5, "failures": 1, "errors": 0, "skipped": 2, "passed": 2},
            HARNESS.count_android_results(result_dir),
        )

    def test_owned_children_are_stopped(self):
        child = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(60)"],
            start_new_session=True,
        )
        active = {child}
        HARNESS.stop_children(active)
        self.assertIsNotNone(child.poll())
        self.assertEqual(set(), active)

    def test_interrupted_live_child_remains_registered_for_shutdown(self):
        class InterruptedOutput:
            def read(self, _size):
                raise KeyboardInterrupt

        process = mock.Mock()
        process.stdout = InterruptedOutput()
        process.poll.return_value = None
        active = set()
        paths = HARNESS.create_run_paths(self.root / "runs")
        with mock.patch.object(HARNESS.subprocess, "Popen", return_value=process):
            with self.assertRaises(KeyboardInterrupt):
                HARNESS.run_command(
                    ["owned-child"],
                    cwd=self.root,
                    environment=os.environ.copy(),
                    log_path=paths.reports / "interrupted.log",
                    active=active,
                )
        self.assertEqual({process}, active)

    def test_cleanup_is_bounded_and_external_sentinel_survives(self):
        parent = self.root / "runs"
        paths = HARNESS.create_run_paths(parent)
        sentinel = self.root / "sentinel"
        sentinel.write_text("outside")
        HARNESS.safe_cleanup(paths.root, parent)
        self.assertFalse(paths.root.exists())
        self.assertEqual("outside", sentinel.read_text())
        with self.assertRaisesRegex(HARNESS.HarnessError, "cible extérieure"):
            HARNESS.safe_cleanup(self.root, parent)

    def test_cleanup_refuses_symlink(self):
        parent = self.root / "runs"
        paths = HARNESS.create_run_paths(parent)
        outside = self.root / "outside"
        outside.write_text("intact")
        link = paths.root / "external-link"
        link.symlink_to(outside)
        with self.assertRaisesRegex(HARNESS.HarnessError, "symlink"):
            HARNESS.safe_cleanup(paths.root, parent)
        self.assertEqual("intact", outside.read_text())
        link.unlink()
        shutil.rmtree(paths.root)

    def test_default_modes_never_start_real_transports(self):
        commands = HARNESS.commands_for_suite(ROOT, "full")
        flattened = [argument for _, command, _ in commands for argument in command]
        forbidden = ("trainlog-sync-once", "trainlog_syncd.py", "rclone", "adb")
        for name in forbidden:
            self.assertFalse(any(name in argument for argument in flattened), name)
        self.assertNotIn("connectedDebugAndroidTest", flattened)


if __name__ == "__main__":
    unittest.main()
