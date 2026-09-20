import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "sync_drive_transport", ROOT / "tools/sync_drive_transport.py"
)
drive = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(drive)


class MemoryDrive:
    def __init__(self):
        self.objects = {}
        self.uploads = []
        self.downloads = []

    def upload_verified(self, source, relative):
        self.uploads.append(relative)
        self.objects[relative] = Path(source).read_bytes()

    def download(self, relative, destination, optional=False):
        self.downloads.append(relative)
        if relative not in self.objects:
            if optional:
                return False
            raise drive.DriveTransportError("missing object")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(self.objects[relative])
        return True


class DriveTransportTest(unittest.TestCase):
    def generation(self, root):
        generation = root / "desktop-objects/generations/gen_one"
        generation.mkdir(parents=True)
        artifact = generation / "history.json"
        artifact.write_bytes(b"{}\n")
        manifest = {
            "generation_id": "gen_one",
            "artifacts": [
                {
                    "filename": "generations/gen_one/history.json",
                    "sha256": drive.digest(artifact),
                }
            ],
        }
        (generation / "manifest.json").write_text(json.dumps(manifest))
        (root / "desktop-generation-v1.json").write_text(
            json.dumps(
                {
                    "generation_id": "gen_one",
                    "relative_path": "desktop-objects/generations/gen_one",
                }
            )
        )

    def test_manifest_is_uploaded_after_artifacts_and_reference_is_last(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            self.generation(root)
            remote = MemoryDrive()
            drive.push(remote, root)
            self.assertLess(
                remote.uploads.index("desktop-objects/generations/gen_one/history.json"),
                remote.uploads.index("desktop-objects/generations/gen_one/manifest.json"),
            )
            self.assertLess(
                remote.uploads.index("desktop-objects/generations/gen_one/manifest.json"),
                remote.uploads.index("desktop-generation-v1.json"),
            )

    def test_generation_reference_accepts_its_producer_namespace(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            self.generation(root)
            self.assertEqual(
                drive.generation_from_reference(root, "desktop-generation-v1.json"),
                "desktop-objects/generations/gen_one",
            )

            value = json.loads((root / "desktop-generation-v1.json").read_text())
            value["relative_path"] = "android-objects/generations/gen_one"
            (root / "desktop-generation-v1.json").write_text(json.dumps(value))
            with self.assertRaises(drive.DriveTransportError):
                drive.generation_from_reference(root, "desktop-generation-v1.json")

    def test_partial_generation_never_becomes_visible_locally(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            remote = MemoryDrive()
            remote.objects["android-generation-v1.json"] = json.dumps(
                {
                    "generation_id": "gen_one",
                    "relative_path": "android-objects/generations/gen_one",
                }
            ).encode()
            remote.objects["android-objects/generations/gen_one/manifest.json"] = json.dumps(
                {
                    "generation_id": "gen_one",
                    "artifacts": [
                        {"filename": "generations/gen_one/missing.json"}
                    ],
                }
            ).encode()
            with self.assertRaises(drive.DriveTransportError):
                drive.pull(remote, root)
            self.assertFalse((root / "android-objects/generations/gen_one").exists())

    def test_complete_generation_is_pulled_into_its_producer_namespace(self):
        with tempfile.TemporaryDirectory() as raw:
            source = Path(raw) / "source"
            source.mkdir()
            self.generation(source)
            remote = MemoryDrive()
            drive.push(remote, source)
            destination = Path(raw) / "destination"
            drive.pull(remote, destination)
            self.assertEqual(
                (destination / "desktop-objects/generations/gen_one/history.json").read_bytes(),
                b"{}\n",
            )

            remote.downloads.clear()
            drive.pull(remote, destination)
            self.assertNotIn(
                "desktop-objects/generations/gen_one/history.json",
                remote.downloads,
            )

    def test_unsafe_or_ai_namespaces_are_rejected(self):
        for remote in ("", "remote:Trainlog/AI", "remote:Trainlog/Sync/v1\nother"):
            with self.subTest(remote=remote), self.assertRaises(drive.DriveTransportError):
                drive.RcloneDrive("rclone", remote)
        for relative in ("../db", "/absolute", "a\\b"):
            with self.subTest(relative=relative), self.assertRaises(drive.DriveTransportError):
                drive.safe_relative(relative)


if __name__ == "__main__":
    unittest.main()
