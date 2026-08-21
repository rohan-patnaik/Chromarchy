#!/usr/bin/env python3
"""Validate the offline capability catalog and generate its Markdown view."""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "docs" / "offline-capabilities.json"
OUTPUT = ROOT / "docs" / "OFFLINE_PARITY.md"
ID_HISTORY = ROOT / "docs" / "capability-id-history.json"
STATUSES = ("Complete", "Partial", "Planned", "Excluded", "Blocked")
PHASES = ("M0", "P1", "M1", "M2", "M3", "M4", "M5", "M6", "Excluded")
ID_PATTERN = re.compile(r"^[a-z0-9]+(?:[.-][a-z0-9]+)*$")
TEST_ID_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9_]*(?:::[A-Za-z][A-Za-z0-9_]*)*$")
ROOT_FIELDS = {"schemaVersion", "scope", "statusDefinitions", "capabilities"}
CAPABILITY_FIELDS = {
    "id", "area", "title", "phase", "status", "acceptance",
    "testFixtureOwner", "dependencies", "budget", "evidence", "limits",
}
REQUIRED_STRINGS = (
    "id",
    "area",
    "title",
    "phase",
    "status",
    "acceptance",
    "testFixtureOwner",
    "budget",
    "limits",
)


def fail(message: str) -> None:
    raise ValueError(message)


def validate(catalog: dict) -> list[dict]:
    if not isinstance(catalog, dict):
        fail("catalog root must be an object")
    unknown_root = set(catalog) - ROOT_FIELDS
    if unknown_root:
        fail(f"unknown catalog fields: {', '.join(sorted(unknown_root))}")
    if catalog.get("schemaVersion") != 1:
        fail("schemaVersion must be 1")
    capabilities = catalog.get("capabilities")
    if not isinstance(capabilities, list) or not capabilities:
        fail("capabilities must be a non-empty array")
    definitions = catalog.get("statusDefinitions")
    if not isinstance(definitions, dict) or set(definitions) != set(STATUSES):
        fail("statusDefinitions must contain exactly the supported statuses")

    seen: set[str] = set()
    for index, capability in enumerate(capabilities):
        label = f"capabilities[{index}]"
        if not isinstance(capability, dict):
            fail(f"{label} must be an object")
        unknown_fields = set(capability) - CAPABILITY_FIELDS
        missing_fields = CAPABILITY_FIELDS - set(capability)
        if unknown_fields:
            fail(f"{label} has unknown fields: {', '.join(sorted(unknown_fields))}")
        if missing_fields:
            fail(f"{label} is missing fields: {', '.join(sorted(missing_fields))}")
        for field in REQUIRED_STRINGS:
            value = capability.get(field)
            if not isinstance(value, str) or not value.strip():
                fail(f"{label}.{field} must be a non-empty string")
        capability_id = capability["id"]
        if not ID_PATTERN.fullmatch(capability_id):
            fail(f"{capability_id}: invalid stable ID")
        if capability_id in seen:
            fail(f"{capability_id}: duplicate stable ID")
        seen.add(capability_id)
        if capability["status"] not in STATUSES:
            fail(f"{capability_id}: unknown status {capability['status']}")
        if capability["phase"] not in PHASES:
            fail(f"{capability_id}: unknown phase {capability['phase']}")
        if (capability["status"] == "Excluded") != (capability["phase"] == "Excluded"):
            fail(f"{capability_id}: Excluded status and phase must be paired")
        for field in ("dependencies", "evidence"):
            value = capability.get(field)
            if not isinstance(value, list) or not all(
                isinstance(item, str) and item.strip() for item in value
            ):
                fail(f"{capability_id}.{field} must be an array of strings")
        if capability["status"] in ("Complete", "Partial") and not capability["evidence"]:
            fail(f"{capability_id}: {capability['status']} requires evidence")
        for evidence in capability["evidence"]:
            relative_path, separator, symbol = evidence.partition("#")
            candidate = Path(relative_path)
            if candidate.is_absolute() or ".." in candidate.parts or candidate.as_posix() != relative_path:
                fail(f"{capability_id}: evidence path is not repository-normalized: {relative_path}")
            evidence_path = ROOT / relative_path
            if not evidence_path.is_file():
                fail(f"{capability_id}: missing evidence path {relative_path}")
            if separator:
                if not symbol:
                    fail(f"{capability_id}: empty evidence symbol in {evidence}")
                if relative_path.startswith("tests/") and not TEST_ID_PATTERN.fullmatch(symbol):
                    fail(f"{capability_id}: invalid structured test ID {symbol}")
                contents = evidence_path.read_text(encoding="utf-8")
                if symbol not in contents:
                    fail(f"{capability_id}: missing evidence symbol {evidence}")
    try:
        history = json.loads(ID_HISTORY.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read capability ID history: {error}")
    if not isinstance(history, dict) or set(history) != {"schemaVersion", "retired"}:
        fail("capability ID history must contain exactly schemaVersion and retired")
    if history["schemaVersion"] != 1 or not isinstance(history["retired"], list):
        fail("invalid capability ID history schema")
    retired: set[str] = set()
    for index, entry in enumerate(history["retired"]):
        if not isinstance(entry, dict) or set(entry) != {"id", "replacement", "reason"}:
            fail(f"retired[{index}] has invalid fields")
        if not all(isinstance(entry[key], str) and entry[key].strip() for key in entry):
            fail(f"retired[{index}] fields must be non-empty strings")
        if not ID_PATTERN.fullmatch(entry["id"]) or entry["id"] in retired:
            fail(f"retired[{index}] has invalid or duplicate ID")
        retired.add(entry["id"])
    overlap = seen & retired
    if overlap:
        fail(f"retired IDs cannot be active: {', '.join(sorted(overlap))}")
    return capabilities


def cell(value: str) -> str:
    return value.replace("|", "\\|").replace("\n", " ")


def render(catalog: dict, capabilities: list[dict]) -> str:
    counts = Counter(item["status"] for item in capabilities)
    lines = [
        "<!-- Generated by scripts/generate_offline_parity.py; do not edit. -->",
        "# Offline capability catalog",
        "",
        cell(catalog["scope"]),
        "",
        "This is an evidence backlog, not a product-parity claim. Stable IDs are never",
        "reused; status changes and evidence changes must update the source catalog in the",
        "same commit.",
        "",
        "## Counts",
        "",
        "| Complete | Partial | Planned | Excluded | Blocked | Total |",
        "| ---: | ---: | ---: | ---: | ---: | ---: |",
        f"| {counts['Complete']} | {counts['Partial']} | {counts['Planned']} | "
        f"{counts['Excluded']} | {counts['Blocked']} | {len(capabilities)} |",
        "",
        "## Status rules",
        "",
    ]
    for status in STATUSES:
        lines.append(f"- **{status}** — {cell(catalog['statusDefinitions'][status])}")
    lines += [
        "",
        "## Capabilities",
        "",
        "| Stable ID | Area | Capability | Status | Phase | Acceptance recipe | Test/fixture owner | Dependencies | Budget | Evidence | Exclusions or limits |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for item in sorted(capabilities, key=lambda value: (value["area"], value["id"])):
        dependencies = ", ".join(item["dependencies"]) or "None"
        evidence = ", ".join(f"`{entry}`" for entry in item["evidence"]) or "None yet"
        lines.append(
            "| "
            + " | ".join(
                cell(value)
                for value in (
                    f"`{item['id']}`",
                    item["area"],
                    item["title"],
                    item["status"],
                    item["phase"],
                    item["acceptance"],
                    item["testFixtureOwner"],
                    dependencies,
                    item["budget"],
                    evidence,
                    item["limits"],
                )
            )
            + " |"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
        capabilities = validate(catalog)
        generated = render(catalog, capabilities)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print(f"offline parity validation failed: {error}", file=sys.stderr)
        return 1

    if args.check:
        try:
            current = OUTPUT.read_text(encoding="utf-8")
        except OSError as error:
            print(f"offline parity drift: {error}", file=sys.stderr)
            return 1
        if current != generated:
            print(
                "offline parity drift: run scripts/generate_offline_parity.py",
                file=sys.stderr,
            )
            return 1
    else:
        OUTPUT.write_text(generated, encoding="utf-8", newline="\n")

    counts = Counter(item["status"] for item in capabilities)
    print(
        "offline parity: "
        + ", ".join(f"{status}={counts[status]}" for status in STATUSES)
        + f", Total={len(capabilities)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
