#!/usr/bin/env python3
"""Materialize and build the non-migrating schema-v17 Android backup bridge."""

import argparse, io, os, shutil, subprocess, tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "77c15172e1401b91b8e2537c4b8548095ed50053"


def replace(path: Path, old: str, new: str):
    value = path.read_text()
    if value.count(old) != 1:
        raise RuntimeError(f"bridge patch anchor mismatch: {path}: {old[:40]}")
    path.write_text(value.replace(old, new))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--jdk", default="/usr/lib/jvm/java-17-openjdk")
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError("bridge output already exists")
    args.output.mkdir(parents=True)
    raw = subprocess.check_output(
        ["git", "archive", "--format=tar", BASE, "android", "catalog"], cwd=ROOT
    )
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        for member in archive.getmembers():
            target = (args.output / member.name).resolve()
            if args.output.resolve() not in target.parents:
                raise RuntimeError("unsafe archive member")
        archive.extractall(args.output, filter="data")
    android = args.output / "android"
    repository = android / "app/src/main/java/com/labfytools/trainlog/data/TrainlogRepository.kt"
    replace(
        repository,
        "import java.text.Normalizer",
        "import java.text.Normalizer\nimport java.io.File",
    )
    replace(
        repository,
        "    fun close() {\n        database.close()\n    }",
        '    fun close() {\n        database.close()\n    }\n\n    internal fun backupDatabaseTo(destination: File) {\n        require(!destination.exists())\n        val escaped = destination.absolutePath.replace("\'", "\'\'")\n        database.writableDatabase.execSQL("VACUUM INTO \'$escaped\'")\n    }',
    )
    shutil.copy2(
        ROOT / "android/app/src/main/java/com/labfytools/trainlog/data/AndroidBackupService.kt",
        android / "app/src/main/java/com/labfytools/trainlog/data/AndroidBackupService.kt",
    )
    shutil.copy2(
        ROOT / "tools/android_backup_bridge/BackupBridgeActivity.kt",
        android / "app/src/main/java/com/labfytools/trainlog/data/BackupBridgeActivity.kt",
    )
    shutil.copy2(
        ROOT / "tools/android_backup_bridge/BackupBridgeServiceTest.kt",
        android / "app/src/test/java/com/labfytools/trainlog/data/BackupBridgeServiceTest.kt",
    )
    manifest = android / "app/src/main/AndroidManifest.xml"
    replace(
        manifest,
        '        <activity\n            android:name=".MainActivity"',
        '        <activity android:name=".data.BackupBridgeActivity" android:exported="true" android:label="Trainlog Backup">\n            <intent-filter><action android:name="android.intent.action.MAIN"/><category android:name="android.intent.category.LAUNCHER"/></intent-filter>\n        </activity>\n        <activity\n            android:name=".MainActivity"',
    )
    gradle = android / "app/build.gradle.kts"
    replace(
        gradle,
        "versionCode = 3",
        'versionCode = providers.environmentVariable("TRAINLOG_BRIDGE_VERSION_CODE").orNull?.toInt() ?: 4',
    )
    env = os.environ.copy()
    env["JAVA_HOME"] = args.jdk
    env["TRAINLOG_BRIDGE_VERSION_CODE"] = env.get("TRAINLOG_BRIDGE_VERSION_CODE", "4")
    env["TRAINLOG_BRIDGE_BACKUP_OUTPUT"] = str(args.output / "schema17-synthetic.tlbackup")
    subprocess.run(
        [
            str(android / "gradlew"),
            "--no-daemon",
            ":app:testDebugUnitTest",
            "--tests",
            "com.labfytools.trainlog.data.BackupBridgeServiceTest",
            ":app:assembleRelease",
        ],
        cwd=android,
        env=env,
        check=True,
    )
    migration_env = os.environ.copy()
    migration_env["JAVA_HOME"] = args.jdk
    migration_env["TRAINLOG_BRIDGE_BACKUP_INPUT"] = env["TRAINLOG_BRIDGE_BACKUP_OUTPUT"]
    subprocess.run(
        [
            str(ROOT / "android/gradlew"),
            "--no-daemon",
            ":app:testDebugUnitTest",
            "--tests",
            "com.labfytools.trainlog.data.BridgeBackupMigrationTest",
            "--rerun-tasks",
        ],
        cwd=ROOT / "android",
        env=migration_env,
        check=True,
    )
    print(f"BRIDGE_BASE={BASE}")
    print(
        f"BRIDGE_RECIPE={subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()}"
    )
    print(f"BRIDGE_APK={android/'app/build/outputs/apk/release/app-release.apk'}")


if __name__ == "__main__":
    main()
