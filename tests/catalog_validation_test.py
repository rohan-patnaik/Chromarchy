#!/usr/bin/env python3
"""Negative contract tests for the offline capability catalog validator."""

from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "offline_parity", ROOT / "scripts" / "generate_offline_parity.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class CatalogValidationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.catalog = json.loads(MODULE.CATALOG.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        candidate = copy.deepcopy(self.catalog)
        mutate(candidate)
        with self.assertRaises(ValueError):
            MODULE.validate(candidate)

    @staticmethod
    def history() -> dict:
        return json.loads(MODULE.ID_HISTORY.read_text(encoding="utf-8"))

    def test_unknown_root_field_is_rejected(self) -> None:
        self.assertRejected(lambda catalog: catalog.__setitem__("surprise", True))

    def test_unknown_capability_field_is_rejected(self) -> None:
        self.assertRejected(
            lambda catalog: catalog["capabilities"][0].__setitem__("score", 10)
        )

    def test_excluded_phase_relation_is_enforced(self) -> None:
        self.assertRejected(
            lambda catalog: catalog["capabilities"][0].__setitem__("phase", "Excluded")
        )

    def test_parent_traversal_evidence_is_rejected(self) -> None:
        def mutate(catalog):
            row = next(item for item in catalog["capabilities"] if item["evidence"])
            row["evidence"] = ["../README.md"]
        self.assertRejected(mutate)

    def test_unstructured_test_anchor_is_rejected(self) -> None:
        def mutate(catalog):
            row = next(item for item in catalog["capabilities"] if item["evidence"])
            row["evidence"] = ["tests/MetadataTest.cpp#not a test"]
        self.assertRejected(mutate)

    def test_arbitrary_test_substring_is_not_a_definition(self) -> None:
        def mutate(catalog):
            row = next(item for item in catalog["capabilities"] if item["evidence"])
            row["evidence"] = ["tests/MetadataTest.cpp#QVERIFY"]
        self.assertRejected(mutate)

    def test_complete_requires_defined_test_evidence(self) -> None:
        def mutate(catalog):
            row = next(
                item for item in catalog["capabilities"]
                if item["status"] == "Complete"
            )
            row["evidence"] = ["README.md"]
        self.assertRejected(mutate)

    def test_active_id_deletion_requires_retirement(self) -> None:
        self.assertRejected(lambda catalog: catalog["capabilities"].pop())

    def test_issued_id_deletion_is_rejected_against_base(self) -> None:
        candidate = copy.deepcopy(self.catalog)
        removed = candidate["capabilities"].pop()["id"]
        current_history = self.history()
        current_history["issued"].remove(removed)
        with self.assertRaisesRegex(ValueError, "issued IDs are append-only"):
            MODULE.validate(
                candidate,
                history=current_history,
                previous_history=self.history(),
            )

    def test_retired_entry_deletion_is_rejected_against_base(self) -> None:
        previous = self.history()
        previous["retired"] = [{
            "id": "historical.retired-id",
            "replacement": "none",
            "reason": "Historical test fixture",
        }]
        with self.assertRaisesRegex(ValueError, "retired IDs are append-only"):
            MODULE.validate(
                copy.deepcopy(self.catalog),
                history=self.history(),
                previous_history=previous,
            )


if __name__ == "__main__":
    unittest.main()
