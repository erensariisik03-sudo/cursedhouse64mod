#!/usr/bin/env python3
"""Create a compact, stdlib-only symbol/search report from a Unity dump.cs."""
from __future__ import annotations

import argparse
import bisect
import json
import re
from collections import defaultdict
from pathlib import Path

DECL_RE = re.compile(
    r"^\s*(?:(?:public|private|protected|internal|static|abstract|sealed|partial|unsafe|new|readonly)\s+)*"
    r"(class|struct|enum|interface)\s+([A-Za-z_][\w.`+<>]*)"
)
IMAGE_RE = re.compile(r"^\s*//\s*Image\s+\d+\s*:\s*(.*?)\s*-\s*(\d+)\s*$")
TYPE_INDEX_RE = re.compile(r"TypeDefIndex:\s*(\d+)")
NS_RE = re.compile(r"^\s*//\s*Namespace:\s*(.*?)\s*$")


def load_terms(path: Path) -> list[str]:
    if not path.exists():
        return []
    out = []
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        value = raw.strip()
        if value and not value.startswith("#"):
            out.append(value)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--terms-file", type=Path, default=Path(__file__).resolve().with_name("queries.txt"))
    ap.add_argument("--query", action="append", default=[], help="Extra case-insensitive substring query; repeatable")
    ap.add_argument("--max-hits-per-term", type=int, default=8)
    ap.add_argument("--context", type=int, default=2, help="Context lines before/after each match")
    ap.add_argument("--symbols-json", type=Path, default=None)
    args = ap.parse_args()

    if not args.input.is_file():
        ap.error(f"Input not found: {args.input}. Copy dump.cs to reference/dump.cs first.")

    terms = list(dict.fromkeys(load_terms(args.terms_file) + args.query))
    if not terms:
        ap.error("No search terms. Add terms to tools/queries.txt or pass --query.")

    terms_lc = [(t, t.casefold()) for t in terms]
    hits: dict[str, list[dict]] = defaultdict(list)
    symbols: list[dict] = []
    active_image = "?"
    active_ns = ""
    current_symbol = "(outside class)"
    line_count = 0

    # The header maps assembly names to starting TypeDefIndex values.
    image_starts: list[tuple[int, str]] = []
    with args.input.open("r", encoding="utf-8", errors="replace") as stream:
        for _ in range(256):
            raw = stream.readline()
            if not raw:
                break
            m = IMAGE_RE.match(raw.rstrip("\n"))
            if m:
                image_starts.append((int(m.group(2)), m.group(1).strip()))
    image_starts.sort()
    image_indices = [x[0] for x in image_starts]

    # Pass 1: index declarations and collect bounded search hits.
    with args.input.open("r", encoding="utf-8", errors="replace") as stream:
        for line_no, raw in enumerate(stream, 1):
            line_count = line_no
            line = raw.rstrip("\n")
            m = NS_RE.match(line)
            if m:
                active_ns = m.group(1).strip()
            m = DECL_RE.match(line)
            if m:
                current_symbol = f"{m.group(1)} {m.group(2)}"
                type_index = TYPE_INDEX_RE.search(line)
                if type_index and image_starts:
                    idx = int(type_index.group(1))
                    pos = bisect.bisect_right(image_indices, idx) - 1
                    if pos >= 0:
                        active_image = image_starts[pos][1]
                symbols.append({
                    "line": line_no,
                    "kind": m.group(1),
                    "name": m.group(2),
                    "namespace": active_ns,
                    "image": active_image,
                    "declaration": line.strip(),
                })
            folded = line.casefold()
            for term, needle in terms_lc:
                if needle in folded and len(hits[term]) < max(args.max_hits_per_term, 1):
                    hits[term].append({
                        "line": line_no,
                        "text": line.strip(),
                        "image": active_image,
                        "namespace": active_ns,
                        "symbol": current_symbol,
                    })

    # Pass 2: fetch small line windows only for selected hits.
    selected = set()
    for term_hits in hits.values():
        for item in term_hits:
            lo = max(1, item["line"] - max(args.context, 0))
            hi = min(line_count, item["line"] + max(args.context, 0))
            selected.update(range(lo, hi + 1))
    context_lines: dict[int, str] = {}
    if selected:
        with args.input.open("r", encoding="utf-8", errors="replace") as stream:
            for line_no, raw in enumerate(stream, 1):
                if line_no in selected:
                    context_lines[line_no] = raw.rstrip("\n")

    # Put game-owned symbols first where possible.
    for term in hits:
        hits[term].sort(key=lambda h: ("assembly-csharp" not in h["image"].casefold(), h["line"]))

    out = [
        "# dump.cs compact index",
        "",
        f"- File: `{args.input}`",
        f"- Lines scanned: **{line_count:,}**",
        f"- Type declarations indexed: **{len(symbols):,}**",
        "- Search is case-insensitive substring matching; hit counts below are capped.",
        "- Assembly ownership is inferred from the dump header TypeDefIndex ranges.",
        "- `Assembly-CSharp.dll` matches are displayed first when present.",
        "",
        "## Type declarations (first 100)",
        "",
        "| Line | Assembly | Namespace | Declaration |",
        "|---:|---|---|---|",
    ]
    for s in symbols[:100]:
        clean = s["declaration"].replace("|", "\\|")
        out.append(f"| {s['line']} | {s['image']} | {s['namespace'] or '(global)'} | `{clean}` |")

    out.extend(["", "## Search results", ""])
    for term, _ in terms_lc:
        term_hits = hits.get(term, [])
        out.append(f"### `{term}` — {len(term_hits)} shown (cap {args.max_hits_per_term})")
        if not term_hits:
            out.extend(["", "No matches.", ""])
            continue
        out.append("")
        for item in term_hits:
            out.append(f"**Line {item['line']}** · `{item['image']}` · `{item['namespace'] or '(global)'}` · `{item['symbol']}`")
            start = max(1, item["line"] - max(args.context, 0))
            end = min(line_count, item["line"] + max(args.context, 0))
            out.append("```csharp")
            for n in range(start, end + 1):
                marker = ">" if n == item["line"] else " "
                out.append(f"{marker}{n}: {context_lines.get(n, '')}")
            out.append("```")
            out.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(out), encoding="utf-8")
    if args.symbols_json:
        args.symbols_json.parent.mkdir(parents=True, exist_ok=True)
        args.symbols_json.write_text(json.dumps(symbols, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Wrote {args.out} ({line_count:,} lines scanned; {len(symbols):,} declarations; {len(terms)} terms).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
