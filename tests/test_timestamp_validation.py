#!/usr/bin/env python3
"""Focused frozen timestamp grammar and exact chronology regressions."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from validate_json import TrainlogSemanticError, parse_timestamp  # noqa: E402
import validate_json as validation  # noqa: E402

class TimestampValidationTest(unittest.TestCase):
    def test_document_admission_does_not_depend_on_optional_format_checker(self):
        schema = validation.load_json(validation.SCHEMA_PATH)
        # Simulate missing, stricter RFC-only, and permissive optional checkers.
        # Exercise the actual document path, including schema checks before semantics.
        for mode in ("missing", "strict", "permissive"):
            checker = validation.jsonschema.FormatChecker()
            checker.checkers.pop("date-time", None)
            if mode != "missing":
                checker.checks("date-time")(
                    lambda value, strict=mode == "strict": not strict
                    or not isinstance(value, str)
                    or len(value) > 19 and value[16] == ":"
                )
            original_checks = checker.checkers.copy()
            validator = validation.jsonschema.Draft202012Validator(schema, format_checker=checker)
            with self.subTest(mode=mode):
                self.assertEqual([], validation.validate_document(
                    validator, validation.VALID_FIXTURE_DIR / "temporal-extended-forms.json"))
                self.assertTrue(validation.validate_document(
                    validator, validation.INVALID_FIXTURE_DIR / "timestamp-space-separator.json"))
                self.assertEqual(original_checks, checker.checkers)

    def test_selected_grammar(self):
        for value in ("0001-01-01T00:00Z", "2000-02-29t23:59:59.00000000000000000001z",
                      "2026-09-05T18:34+23:59", "2026-09-05T18:34:12-00:00",
                      "9999-12-31T23:59:59.9-23:59"):
            with self.subTest(value=value): self.assertIsNotNone(parse_timestamp(value, "probe"))

    def test_platform_only_forms_rejected(self):
        for value in ("0000-01-01T00:00:00Z", "2026-02-29T00:00:00Z",
                      "2026-09-05 18:34:12+02:00", "20260905T183412+0200",
                      "2026-W36-5T18:34:12+02:00", "2026-09-05T18:34:12,5+02:00",
                      "2026-09-05T18:34:12+0200", "2026-09-05T18:34:12+02",
                      "2026-09-05T18:34:12.5", "2026-09-05T18:34.5Z",
                      "2026-09-05T18:34:60Z", "2026-09-05T18:34:12+24:00"):
            with self.subTest(value=value), self.assertRaises(TrainlogSemanticError):
                parse_timestamp(value, "probe")

    def test_exact_fraction_and_rollover(self):
        earlier = parse_timestamp("2026-01-01T00:00:00.12345678901234567890Z", "probe")
        later = parse_timestamp("2026-01-01T00:00:00.12345678901234567891Z", "probe")
        equal = parse_timestamp("2026-01-01T00:00:00.1234567890123456789000Z", "probe")
        self.assertLess(earlier, later); self.assertEqual(earlier, equal)
        self.assertLess(parse_timestamp("2025-12-31T09:14:59Z", "probe"),
                        parse_timestamp("2026-01-01T00:15:00+15:00", "probe"))

if __name__ == "__main__": unittest.main()
