"""Validate every design cell and reference; generate one C++ struct per row.

This verifies declared configuration and its evidence, not in-game functionality.
Never promotes pending cells to verified. Runtime verification is separate.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLACEHOLDERS = {"unresolved", "unverified", "unknown", "todo", "tbd", "pending"}
CPP_RESERVED = set("auto bool break case char class const constexpr continue default delete do double else enum explicit extern false float for friend goto if inline int long namespace new nullptr operator private protected public return short signed sizeof static struct switch template this throw true try typedef typename union unsigned using virtual void volatile while".split())


def identifier(text):
    name = re.sub(r"[^A-Za-z0-9_]", "_", str(text))
    return "row_" + name if not name or name[0].isdigit() or name in CPP_RESERVED or name.startswith("_") else name


def load_sheets(names=None, root=ROOT):
    sheets = {}
    issues = []
    for path in sorted((root / "sheets").glob("*.json")):
        try:
            sheet = json.loads(path.read_text(encoding="utf-8-sig"))
        except (ValueError, OSError) as exc:
            issues.append(f"{path.name}: {exc}")
            continue
        if not isinstance(sheet, dict):
            issues.append(f"{path.name}: sheet must be an object")
            continue
        name = sheet.get("name", path.stem)
        if not isinstance(name, str) or not name.strip():
            issues.append(f"{path.name}: invalid sheet name")
            continue
        if name in sheets:
            issues.append(f"{name}: duplicate sheet name")
        sheets[name] = sheet
    if not sheets:
        issues.append("sheets: no JSON sheets found")
    if names:
        missing = set(names) - set(sheets)
        issues.extend(f"{n}: sheet not found" for n in sorted(missing))
    return sheets, issues


def scope(sheets, names=None):
    selected = set(sheets) if not names else set(names)
    pending = list(selected)
    while pending:
        sheet = sheets.get(pending.pop(), {})
        refs = sheet.get("references", []) if isinstance(sheet, dict) else []
        for ref in refs if isinstance(refs, list) else []:
            target = ref.get("sheet") if isinstance(ref, dict) else None
            if isinstance(target, str) and target not in selected:
                selected.add(target)
                pending.append(target)
    return sorted(selected)


def value_issues(value):
    if value is None or isinstance(value, str) and not value.strip():
        return ["unfilled"]
    if isinstance(value, str) and value.strip().lower() in PLACEHOLDERS:
        return ["unresolved placeholder"]
    if isinstance(value, (dict, list)):
        if not value:
            return ["empty collection"]
        if any(value_issues(child) for child in (value.values() if isinstance(value, dict) else value)):
            return ["contains unresolved or invalid values"]
    if isinstance(value, float) and not math.isfinite(value):
        return ["non-finite number"]
    if isinstance(value, int) and not isinstance(value, bool) and not -(2**63) <= value < 2**63:
        return ["integer exceeds signed 64-bit range; use a string for hashes"]
    return []


def type_issues(value, rule):
    if not isinstance(rule, dict):
        return ["type rule must be an object"]
    types = {"string": lambda v: isinstance(v, str),
             "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
             "number": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
             "boolean": lambda v: isinstance(v, bool),
             "array": lambda v: isinstance(v, list), "object": lambda v: isinstance(v, dict)}
    typ = rule.get("type")
    if typ not in types or not types[typ](value):
        return [f"expected valid type {typ}"]
    issues = []
    if "enum" in rule and (not isinstance(rule["enum"], list) or value not in rule["enum"]):
        issues.append("not an allowed value")
    if typ in ("number", "integer"):
        for key, compare in (("minimum", lambda a, b: a >= b), ("maximum", lambda a, b: a <= b)):
            if key in rule:
                bound = rule[key]
                if not isinstance(bound, (int, float)) or isinstance(bound, bool):
                    issues.append(f"{key} must be numeric")
                elif not compare(value, bound):
                    issues.append(f"violates {key}={bound}")
    if typ == "array" and "items" in rule:
        for index, item in enumerate(value):
            issues.extend(f"element {index}: {issue}" for issue in type_issues(item, rule["items"]))
    return issues


def preflight(sheets, initial_issues=(), names=None, root=ROOT):
    issues = list(initial_issues)
    cells = 0
    row_count = 0
    selected = scope(sheets, names)
    generated_sheets = set()
    for name in selected:
        sheet = sheets.get(name)
        if not isinstance(sheet, dict):
            issues.append(f"{name}: sheet missing or not an object")
            continue
        if identifier(name) in generated_sheets:
            issues.append(f"{name}: generated namespace collision")
        generated_sheets.add(identifier(name))
        if sheet.get("schemaVersion") != 1:
            issues.append(f"{name}: schemaVersion must be 1")
        if sheet.get("name") != name:
            issues.append(f"{name}: name must be explicitly declared")
        columns = sheet.get("columns", [])
        if not isinstance(columns, list) or any(not isinstance(c, str) or not c.strip() for c in columns):
            issues.append(f"{name}: columns must be a nonempty string list")
            columns = []
        if not columns or "id" not in columns or len(columns) != len(set(columns)):
            issues.append(f"{name}: columns must be unique and include id")
        if len({identifier(c) for c in columns}) != len(columns):
            issues.append(f"{name}: generated column collision")
        rows = sheet.get("rows", [])
        if not isinstance(rows, list) or not rows:
            issues.append(f"{name}: rows must be a nonempty list")
            rows = []
        checks = sheet.get("checks", {})
        if not isinstance(checks, dict):
            issues.append(f"{name}: checks must be an object")
            checks = {}
        rules = sheet.get("types", {})
        if not isinstance(rules, dict):
            issues.append(f"{name}: types must be an object")
            rules = {}
        for extra in set(rules) - set(columns):
            issues.append(f"{name}.{extra}: type rule for undeclared column")
        refs = sheet.get("references", [])
        if not isinstance(refs, list):
            issues.append(f"{name}: references must be a list")
            refs = []
        valid_refs = []
        for ref in refs:
            if not isinstance(ref, dict) or not isinstance(ref.get("sheet"), str) or not isinstance(ref.get("column"), str):
                issues.append(f"{name}: malformed reference")
                continue
            target = sheets.get(ref["sheet"], {})
            target_col = ref.get("target", "id")
            if ref["column"] not in columns or not isinstance(target, dict) or not isinstance(target_col, str) or target_col not in target.get("columns", []):
                issues.append(f"{name}: undeclared source or target reference column")
                continue
            target_rows = target.get("rows", [])
            targets = [r.get(target_col) for r in target_rows if isinstance(r, dict)] if isinstance(target_rows, list) else []
            valid_refs.append((ref, targets))
        ids = set()
        generated_names = set()
        for index, row in enumerate(rows):
            row_count += 1
            if not isinstance(row, dict):
                issues.append(f"{name}.row[{index}]: row must be an object")
                continue
            rid = row.get("id")
            if not isinstance(rid, str) or not rid.strip():
                issues.append(f"{name}.row[{index}]: id must be a nonempty string")
                rid = f"<row-{index}>"
            if rid in ids:
                issues.append(f"{name}.{rid}: duplicate row id")
            ids.add(rid)
            safe_id = identifier(rid)
            if safe_id in generated_names:
                issues.append(f"{name}.{rid}: generated identifier collision")
            generated_names.add(safe_id)
            row_checks = checks.get(rid, {})
            if not isinstance(row_checks, dict):
                issues.append(f"{name}.{rid}: checks must be an object")
                row_checks = {}
            for extra in set(row_checks) - set(columns):
                issues.append(f"{name}.{rid}.{extra}: check for undeclared column")
            for extra in set(row) - set(columns):
                issues.append(f"{name}.{rid}.{extra}: column undeclared")
            for col in columns:
                cells += 1
                value = row.get(col)
                issues.extend(f"{name}.{rid}.{col}: {issue}" for issue in value_issues(value))
                if col in rules:
                    issues.extend(f"{name}.{rid}.{col}: {issue}" for issue in type_issues(value, rules[col]))
                check = row_checks.get(col, {})
                if not isinstance(check, dict):
                    check = {}
                if check.get("status") != "verified":
                    issues.append(f"{name}.{rid}.{col}: unverified")
                if not isinstance(check.get("evidence"), str) or not check["evidence"].strip():
                    issues.append(f"{name}.{rid}.{col}: missing evidence")
                artifacts = check.get("artifacts", [])
                if not isinstance(artifacts, list):
                    issues.append(f"{name}.{rid}.{col}: artifacts must be a list")
                else:
                    for artifact in artifacts:
                        path = (root / artifact).resolve() if isinstance(artifact, str) else None
                        if path is None or not path.is_relative_to(root.resolve()) or not path.is_file():
                            issues.append(f"{name}.{rid}.{col}: missing or external evidence artifact: {artifact}")
            for ref, targets in valid_refs:
                val = row.get(ref["column"])
                values = val if isinstance(val, list) else [val]
                for value in values:
                    if value not in targets:
                        issues.append(f"{name}.{rid}.{ref['column']}: unresolved {ref['sheet']}:{value}")
        for extra in set(checks) - ids:
            issues.append(f"{name}.{extra}: checks for nonexistent row")
    return {"ok": not issues, "sheets": selected, "rows": row_count, "cells": cells, "issues": issues}


def cpp_string(value):
    return '"' + ''.join(chr(b) if 32 <= b < 127 and b not in (34, 92)
                         else {34: '\\"', 92: '\\\\', 10: '\\n', 13: '\\r', 9: '\\t'}.get(b, f"\\{b:03o}")
                         for b in value.encode("utf-8")) + '"'


def cpp_value(value):
    if isinstance(value, bool):
        return "bool", "true" if value else "false"
    if isinstance(value, int):
        return "std::int64_t", "(-9223372036854775807LL - 1LL)" if value == -(2**63) else str(value) + "LL"
    if isinstance(value, float):
        return "double", repr(value)
    if isinstance(value, str):
        return "std::string_view", cpp_string(value)
    if isinstance(value, list):
        mapped = [cpp_value(item) for item in value]
        types = {item[0] for item in mapped}
        if types == {"double", "std::int64_t"}:
            typ = "double"
            vals = [repr(float(item)) for item in value]
        elif len(types) == 1:
            typ = mapped[0][0]
            vals = [item[1] for item in mapped]
        else:
            return "std::string_view", cpp_string(json.dumps(value, separators=(",", ":")))
        return f"std::array<{typ}, {len(vals)}>", "{{" + ", ".join(vals) + "}}"
    return "std::string_view", cpp_string(json.dumps(value, sort_keys=True, separators=(",", ":")))


def generate(sheets, names=None, root=ROOT):
    report = preflight(sheets, names=names, root=root)
    if not report["ok"]:
        raise ValueError("Generation blocked by preflight: " + "; ".join(report["issues"]))
    out = root / "src" / "generated"
    out.mkdir(parents=True, exist_ok=True)
    for name, sheet in sheets.items():
        if name not in report["sheets"]:
            continue
        digest = hashlib.sha256(json.dumps(sheet, sort_keys=True).encode()).hexdigest()
        lines = ["// Generated by tools/sheets.py. Edit the JSON sheet first.", f"// Sheet SHA-256: {digest}",
                 "#pragma once", "#include <array>", "#include <cstdint>", "#include <string_view>",
                 f"namespace lsa::sheets::{identifier(name)} {{"]
        for row in sheet["rows"]:
            lines.append(f"struct {identifier(row['id'])} final {{")
            for col in sheet["columns"]:
                typ, val = cpp_value(row[col])
                lines.append(f"  inline static constexpr {typ} {identifier(col)} = {val};")
            lines.append("};")
        lines.append("}")
        path = out / f"{identifier(name)}.hpp"
        temp = path.with_suffix(".hpp.tmp")
        temp.write_text("\n".join(lines) + "\n", encoding="utf-8")
        temp.replace(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--sheets", nargs="+")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    sheets, issues = load_sheets(args.sheets)
    report = preflight(sheets, issues, args.sheets)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Preflight: {report['rows']} rows, {report['cells']} cells; " + ("PASS" if report['ok'] else "BLOCKED"))
    for issue in report["issues"]:
        print(" - " + issue)
    if not report["ok"]:
        return 1
    if args.generate:
        generate(sheets, args.sheets)
    return 0


if __name__ == "__main__":
    sys.exit(main())
