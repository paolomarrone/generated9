#!/usr/bin/env python3
"""Generate SQL numeric cases on the host, or check native sqlite3 output."""
import json
import math
from pathlib import Path
import random
import sqlite3
import sys


def generate(directory):
    rng = random.Random(9)
    integers = [0, 1, -1, 2, -2, 2**31-1, -2**31, 2**32-1,
                2**53-1, 2**53, -(2**53), 2**63-1, -(2**63)]
    expressions = []
    for a in integers:
        for b in integers:
            for op in ["+", "-", "*", "/", "%", "&", "|", "<", "=", ">"]:
                expressions.append(f"({a}) {op} ({b})")
    for _ in range(300):
        a, b = rng.randrange(-2**63, 2**63), rng.randrange(-2**63, 2**63)
        expressions.append(f"({a}) {rng.choice(['+', '-', '*', '/', '%'])} ({b})")
    floats = ["0.0", "-0.0", "0.1", "1e-320", "-1e-320", "1e308",
              "-1e308", "1e999", "-1e999", "9007199254740992.0"]
    for a in floats:
        for b in floats:
            for op in ["+", "-", "*", "/", "<", "=", ">"]:
                expressions.append(f"({a}) {op} ({b})")
    db = sqlite3.connect(":memory:")
    expected, queries = [], []
    for i, expr in enumerate(expressions):
        value, kind = db.execute(f"select ({expr}), typeof({expr})").fetchone()
        expected.append([expr, kind, value])
        queries.append(f"select {i}, typeof({expr}), case typeof({expr}) "
                       f"when 'real' then printf('%!.17g',({expr})) else ({expr}) end;")
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "numeric.sql").write_text("\n".join(queries) + "\n")
    (directory / "numeric.json").write_text(json.dumps(expected))
    print(f"Generated {len(queries)} cases with host SQLite {sqlite3.sqlite_version}")


def check(directory):
    expected = json.loads((directory / "numeric.json").read_text())
    rows = (directory / "numeric.out").read_text().splitlines()
    if len(rows) != len(expected):
        raise ValueError(f"got {len(rows)} rows, expected {len(expected)}")
    for i, ((expr, kind, value), row) in enumerate(zip(expected, rows)):
        index, gotkind, text = row.split("\t")
        if int(index) != i or gotkind != kind:
            raise ValueError(f"{expr}: got {row!r}, expected {kind} {value}")
        if kind == "integer":
            equal = int(text) == value
        elif kind == "real":
            equal = math.isclose(float(text), value, rel_tol=2e-15, abs_tol=0)
        else:
            equal = kind == "null" and text == ""
        if not equal:
            raise ValueError(f"{expr}: got {text}, expected {value}")
    print(f"PASS: {len(rows)} numeric cases match host SQLite (including result types)")


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("generate", "check"):
        sys.exit("usage: compare.py generate|check directory")
    try:
        {"generate": generate, "check": check}[sys.argv[1]](Path(sys.argv[2]))
    except (ValueError, OSError) as error:
        sys.exit(str(error))
