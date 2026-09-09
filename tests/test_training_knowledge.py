import copy
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_training_knowledge", ROOT / "tools" / "validate_training_knowledge.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class TrainingKnowledgeValidationTest(unittest.TestCase):
    files = (
        "body-zones-v1.json", "equipment-v1.json", "science-references-v1.json", "muscles-v1.json",
        "joint-actions-v1.json", "movement-patterns-v1.json",
        "exercise-knowledge-v1.json", "equipment-knowledge-v1.json",
        "training-knowledge-audit-v1.json",
    )

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.catalog = Path(self.temp.name)
        for filename in self.files:
            (self.catalog / filename).write_bytes((ROOT / "catalog" / filename).read_bytes())

    def tearDown(self):
        self.temp.cleanup()

    def mutate(self, filename, operation):
        path = self.catalog / filename
        value = json.loads(path.read_text(encoding="utf-8"))
        operation(value)
        path.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")

    def assert_invalid(self):
        with self.assertRaises(MODULE.ValidationError):
            MODULE.validate(self.catalog)

    def test_valid_and_deterministic_generation(self):
        MODULE.validate(self.catalog)
        first = self.catalog / "first.c"
        second = self.catalog / "second.c"
        command = [sys.executable, str(ROOT / "tools" / "generate_training_knowledge.py"),
                   str(self.catalog)]
        subprocess.run(command + [str(first)], check=True)
        subprocess.run(command + [str(second)], check=True)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        audit = self.catalog / "regenerated-audit.json"
        subprocess.run([sys.executable, str(ROOT / "tools" / "generate_training_knowledge_audit.py"),
                        str(self.catalog), str(audit)], check=True)
        self.assertEqual(audit.read_bytes(), (self.catalog / "training-knowledge-audit-v1.json").read_bytes())
        markdown = self.catalog / "audit.md"
        subprocess.run([sys.executable, str(ROOT / "tools" / "generate_training_knowledge_audit.py"),
                        str(self.catalog), str(markdown), "--markdown"], check=True)
        self.assertEqual(markdown.read_bytes(), (ROOT / "docs/domain/knowledge_audit.md").read_bytes())

    def test_unknown_nested_keys_and_types_are_validation_errors(self):
        mutations = [
            lambda root: root["equipment"][0].__setitem__("unexpected_review_key", True),
            lambda root: root["equipment"][0]["capabilities"][0].__setitem__("requirements", None),
            lambda root: root["equipment"][0].__setitem__("requires_actual_exercise", 1),
        ]
        original = (self.catalog / "equipment-knowledge-v1.json").read_bytes()
        for mutation in mutations:
            (self.catalog / "equipment-knowledge-v1.json").write_bytes(original)
            self.mutate("equipment-knowledge-v1.json", mutation)
            self.assert_invalid()

    def test_additive_non_runtime_reference_is_valid(self):
        def add(root):
            row = copy.deepcopy(root["references"][-1])
            row["ref_id"] = "zz_additive_provenance"
            row["title"] = "Additional provenance record"
            root["references"].append(row)
        self.mutate("science-references-v1.json", add)
        MODULE.validate(self.catalog)

    def test_stale_generated_audit_is_rejected(self):
        self.mutate("training-knowledge-audit-v1.json",
                    lambda root: root["rows"][0].__setitem__("rationale", "stale generated value"))
        self.assert_invalid()

    def test_duplicate_object_key_rejected(self):
        path = self.catalog / "science-references-v1.json"
        text = path.read_text(encoding="utf-8")
        path.write_text(text.replace('"version": 1', '"version": 1, "version": 1', 1), encoding="utf-8")
        self.assert_invalid()

    def test_version_order_enum_role_and_cross_reference_mutations(self):
        cases = [
            ("muscles-v1.json", lambda root: root.__setitem__("version", 2)),
            ("muscles-v1.json", lambda root: root["muscles"].reverse()),
            ("muscles-v1.json", lambda root: root["muscles"][0]["joint_action_ids"].reverse()),
            ("muscles-v1.json", lambda root: root["muscles"][0].__setitem__("confidence", "limited")),
            ("exercise-knowledge-v1.json", lambda root: root["exercises"][0]["interpretation"].
             __setitem__("primary_muscle_ids", ["missing_muscle"])),
            ("exercise-knowledge-v1.json", lambda root: root["exercises"][0]["interpretation"].
             __setitem__("secondary_muscle_ids", root["exercises"][0]["interpretation"]["primary_muscle_ids"])),
            ("equipment-knowledge-v1.json", lambda root: root["equipment"][0]["capabilities"][0].
             __setitem__("exercise_ids", ["ex_00000000-0000-4000-8000-000000000000"])),
        ]
        originals = {filename: (self.catalog / filename).read_bytes() for filename in self.files}
        for filename, mutation in cases:
            for restore, contents in originals.items():
                (self.catalog / restore).write_bytes(contents)
            self.mutate(filename, mutation)
            self.assert_invalid()

    def test_conditional_candidate_cannot_become_resolved_data(self):
        def leak(root):
            row = next(item for item in root["exercises"] if item["resolution_status"] == "conditional")
            row["interpretation"] = copy.deepcopy(row["conditional_interpretation"])
        self.mutate("exercise-knowledge-v1.json", leak)
        self.assert_invalid()

    def test_manufacturer_only_high_mapping_rejected(self):
        def mutation(root):
            root["equipment"][0]["confidence"] = "high"
            root["equipment"][0]["evidence_type"] = "manufacturer_statement"
        self.mutate("equipment-knowledge-v1.json", mutation)
        self.assert_invalid()


if __name__ == "__main__":
    unittest.main()
