#!/usr/bin/env python3
"""Small user-session daemon for Android-triggered Trainlog sync requests."""

from __future__ import annotations

import argparse
import signal
import subprocess
import time
from pathlib import Path


STOP = False


def request_stop(
    _signum: int,
    _frame: object,
) -> None:
    global STOP
    STOP = True


def append_log(
    text: str,
) -> None:
    state_home = Path.home() / ".local" / "state" / "trainlog"
    state_home.mkdir(
        parents=True,
        exist_ok=True,
    )

    with (
        state_home / "syncd.log"
    ).open(
        "a",
        encoding="utf-8",
    ) as handle:
        handle.write(
            time.strftime(
                "%Y-%m-%d %H:%M:%S "
            )
        )

        handle.write(text.rstrip())
        handle.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--sync-once",
        required=True,
        type=Path,
    )

    parser.add_argument(
        "--interval",
        type=float,
        default=3.0,
    )

    args = parser.parse_args()

    if not args.sync_once.exists():
        raise SystemExit(
            f"sync executable missing: {args.sync_once}"
        )

    signal.signal(
        signal.SIGTERM,
        request_stop,
    )

    signal.signal(
        signal.SIGINT,
        request_stop,
    )

    append_log(
        "trainlog-syncd started"
    )

    while not STOP:
        result = subprocess.run(
            [
                str(args.sync_once),
                "--request-only",
                "--trigger",
                "android",
            ],
            capture_output=True,
            text=True,
            check=False,
        )

        if result.returncode == 0:
            append_log(
                result.stdout
            )
        elif result.returncode not in (
            3,
        ):
            detail = (
                result.stderr.strip()
                or result.stdout.strip()
                or f"exit={result.returncode}"
            )

            append_log(
                detail
            )

        deadline = (
            time.monotonic()
            + max(
                args.interval,
                1.0,
            )
        )

        while (
            not STOP
            and time.monotonic()
            < deadline
        ):
            time.sleep(0.2)

    append_log(
        "trainlog-syncd stopped"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
