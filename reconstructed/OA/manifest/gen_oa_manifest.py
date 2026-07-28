#!/usr/bin/env python3
"""
gen_oa_manifest.py - regenerate manifest/oa_functions.csv from the Ghidra static
export for OA.ko, tracking pending -> reconstructed status per real ground-truth
function. This is OA.ko's equivalent of reconstructed/Eva/manifest/gen_manifest.py
(read that script first if this one is confusing -- same shape, adapted source of
truth and adapted status heuristic).

Source of truth: /home/share/Decomp/oa_export/{functions,symbols}.csv -- an
existing Ghidra static export (functions.csv: entry/size/calling_convention/
signature per function; symbols.csv: address -> demangled namespace+name). Do NOT
re-run Ghidra analysis here, this script only reads the existing export.

UNLIKE Eva, OA.ko had NO function-level tracking system before this script (no
hand-maintained literal "RECONSTRUCTED" address set to draw from -- see
PROJECT_BRAIN status.md 2026-07-27). Status is instead inferred with two
heuristics applied to the CURRENT reconstructed/OA/{src,include} tree every time
this script runs (so re-running it after new work naturally advances the count --
no manual bookkeeping list to maintain, unlike Eva's script):

  1. ADDRESS heuristic (high confidence). OA.ko's source comments cite real
     ground-truth addresses as ".text+0xXXXXXX" extensively (established project
     convention -- see e.g. src/auth/cuuid_convert.cpp's ".text+0x46570, 1425
     bytes" comment for CUUID::ConvertFromText, cross-checked byte-exact against
     this export's functions.csv entry=00056570/size_bytes=1425 row). Ghidra
     assigned image base 0x00010000 to this unrelocated .ko object's .text
     section, so real_address = 0x10000 + comment_offset. Every such address
     found anywhere under src/ or include/ is treated as reconstructed.

  2. NAME heuristic (lower confidence, broader recall). OA.ko is much more
     C-flavored than Eva and its reconstructed source frequently keeps ground
     truth's OWN function names verbatim -- both the clean ones (InitCdromSupport,
     ParseAuth) and the deliberately-obfuscated ones (bzzzzzzzzzzzt12,
     fFfFfFfFfFfF11, moancjsd82, cm_SetUserZone) -- so plain name matching carries
     real signal here in a way it wouldn't for a heavily-templated C++ binary.
     This script scans every .cpp/.h/.c file under src/ and include/ for
     function-DEFINITION-shaped text (name(...) { or name(...) : init-list {,
     deliberately NOT just any call or forward declaration) and matches the
     captured name against functions.csv's raw `name` column (plain C functions)
     or symbols.csv's demangled `namespace::name` (C++ methods, e.g.
     "CUUID::ConvertFromText"). This is a syntactic regex, not a compiler or an
     AST -- it can miss multi-line signatures and can in principle false-positive
     on control-flow-shaped text (guarded against for the obvious keywords, see
     _KEYWORDS below, but not exhaustively). Treat NAME-only matches as a
     starting backlog for prioritization, not verified ground truth -- spot-check
     before relying on one. ADDRESS matches are strong evidence (a human
     transcriber cited the real address); NAME matches are not.

2026-07-28 fix: inline-in-class method bodies now qualified and matched.
  Originally the NAME heuristic only recognized out-of-line definitions shaped
  `ClassName::Method(...) { ... }` -- a method body written directly inside a
  class declaration (`ReturnType Method(...) { ... }` with no `ClassName::`
  prefix) was captured by DEF_RE as a bare unqualified name, which the
  by_name check's collision guard then refuses to trust against any
  class-qualified ground-truth row (see the guard's own comment below). Real
  investigation (widget family, CKG*/CSK* cluster, 2026-07-28) found the
  majority of this family's "pending" manifest rows were exactly this: real,
  correct, already-committed inline bodies getting zero NAME credit. Fixed by
  tracking class-body context: a lightweight brace-depth stack (fed by a
  CLASS_RE scan for `class X ... {` / `struct X ... {` headers) is walked
  alongside every DEF_RE match; a bare (unqualified) captured name is
  prefixed with the innermost enclosing class at its match position before
  being added to name_set, e.g. `void Foo() { ... }` inside `class CKGFoo`
  is now recorded as `CKGFoo::Foo`, not bare `Foo`. Verified against the full
  383-class-header sweep of src/+include/ with no false positives (spot-
  checked template declarations like `template <class T> struct Foo { ... }`,
  which could in principle mis-capture the template parameter `T` as a class
  name -- the tightened CLASS_RE's restricted post-name charset makes that
  specific shape fail to match at all, correctly leaving the real `struct Foo`
  header as the next candidate instead).

  A second, more consequential bug was found and fixed alongside it while
  validating the above: DEF_RE (unchanged) is a plain regex over RAW file
  text, so a preceding comment containing an English parenthetical --
  e.g. "... real base default (weak/inline, no address cited) ..." -- can
  itself look like a zero-arg call `default(...)`, and because
  `[^;{}]*` doesn't exclude `)`, that bogus "()" pair's contents can
  greedily swallow everything up to and including the NEXT real function's
  own signature and opening brace, consuming it as one match whose captured
  name is the comment word (`default`, `ops`, `arg`, `truth`, ...) -- the
  real function directly after it is then never seen as its own match at
  all (finditer resumes scanning only after the bogus match's end). This
  silently ate real, already-correct methods including
  `CKGKarmaAssignableSw::GetId()` (the exact method `fb691d3` fixed a missing
  override for) purely because its own preceding doc-comment happened to
  contain "ground truth (`_ZN20CKGKarmaAssignableSw5GetIdEv` exists ...)".
  Fixed by running DEF_RE/CLASS_RE/the brace-stack walk over a
  comment-and-string-blanked copy of the file (see `_strip_comments_and_strings`)
  instead of the raw text -- `/* ... */` and `// ...` spans (and the contents
  of `"..."`/`'...'` literals, so a `/*`-looking sequence inside a string
  can't desync the scanner) are replaced with same-length whitespace so line
  numbers/positions stay stable for debugging, but no comment prose can ever
  masquerade as code again. ADDR_RE still runs over the ORIGINAL raw text
  (unchanged) since ground-truth addresses are deliberately cited INSIDE
  comments -- blanking them there would break the address heuristic entirely.

Known limitations (documented per the task's request, not fixed here):
  - A function can be matched by name coincidentally if the same identifier is
    reused for an unrelated helper (rare given how distinctive most ground-truth
    names are, but possible for short/common names).
  - Static/anonymous-namespace helpers with a project-invented name that happens
    to collide with an unrelated ground-truth symbol will false-positive. Not
    filtered out here.
  - The regex requires the opening `{` to appear before the next `;`, `{`, or `}`
    after the parameter list closes, which pure forward declarations (ending in
    `;`) correctly fail -- but a multi-line signature where trailing attributes or
    exception specifiers appear before the brace works only if they don't contain
    `;`/`{`/`}` themselves.
  - This does not distinguish "reconstructed" from "reconstructed but stubbed" --
    same caveat Eva's Tier A/B convention exists for and this script does not
    attempt to replicate (no per-function tier data exists for OA.ko yet).
  - CLASS_RE requires a plain `class`/`struct` header with only identifiers,
    `public`/`private`/`protected`/`virtual`, commas, whitespace, and `<...>`
    template-argument-shaped text between the name and the opening brace --
    a base-class list using anything wilder (macros, unusual attributes) won't
    be recognized as a class-open brace, so inline methods inside it fall back
    to the pre-fix bare-name behavior (guarded, not mismatched) rather than
    being qualified. Checked empirically: 0 such cases in the current tree
    (see docstring note above, 383/383 class headers matched cleanly).

Usage: python3 gen_oa_manifest.py
(writes oa_functions.csv next to this script; prints the pending/reconstructed
count and a breakdown by match method)
"""
import csv
import os
import re

EXPORT_DIR = "/home/share/Decomp/oa_export"
SRC_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # reconstructed/OA
OUT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "oa_functions.csv")
TEXT_BASE = 0x10000  # Ghidra's assigned base for OA.ko's unrelocated .text section

ADDR_RE = re.compile(r"\.text\+0x([0-9a-fA-F]{4,8})")

# Function-definition-shaped: NAME( ...params... ) [const] [: init-list] {
# Deliberately excludes ';', '{', '}' from both the param list and the optional
# trailing const/init-list span so plain declarations/prototypes don't match.
DEF_RE = re.compile(
    r"(?<![A-Za-z0-9_])([A-Za-z_~][A-Za-z0-9_]*(?:::~?[A-Za-z0-9_]+)?)\s*"
    r"\(([^;{}]*)\)\s*(?:const\s*)?(?::\s*[^;{}]*)?\{"
)
# `class X ... {` / `struct X ... {` header, used only to seed the brace-depth
# class-context stack in scan_reconstructed_tree()/qualify_inline_defs() below
# -- NOT trusted as a general C++ class parser. The restricted charset between
# the name and the opening brace (identifiers/`::`/commas/whitespace/`<>`/
# `public`/`private`/`protected`/`virtual`/pointer-or-ref sigils, plus one
# optional `__attribute__((...))`) is deliberate: it's just permissive enough
# for every real base-class list in this tree (spot-checked, see docstring)
# while still failing closed on `template <class T>\nstruct Foo { ... }` --
# after capturing name="T" there is a stray '>' before any brace/colon, which
# this charset can't swallow, so the match fails at that position and the
# real `struct Foo {` is found on its own next attempt instead of being
# consumed as part of a bogus "class T" match.
CLASS_RE = re.compile(
    r"\b(?:class|struct)\s+([A-Za-z_]\w*)\s*"
    r"(?:__attribute__\s*\(\([^()]*\)\)\s*)?"
    r"(?::\s*[A-Za-z_][\w\s:,<>*&]*)?"
    r"\{"
)
BRACE_RE = re.compile(r"[{}]")
_KEYWORDS = {"if", "for", "while", "switch", "catch", "return", "sizeof", "else"}
_SCAN_EXTS = (".cpp", ".h", ".c")
_SKIP_DIR_PARTS = {".tmp_versions", "objs", "manifest", "verify"}


def _strip_comments_and_strings(text):
    """Return `text` with /* */ and // comment spans, and the contents of
    "..."/'...' literals, replaced by same-length whitespace (newlines kept
    as newlines). Used ONLY to feed CLASS_RE/DEF_RE -- ADDR_RE still scans
    the original raw text since ground-truth addresses are deliberately
    cited inside comments. See module docstring, "2026-07-28 fix" section,
    for why this is needed (a comment-prose parenthetical can otherwise
    masquerade as a zero-arg function call and swallow the real function
    that follows it)."""
    out = []
    i = 0
    n = len(text)
    in_str = in_chr = False
    while i < n:
        c = text[i]
        if in_str or in_chr:
            out.append(c if c == "\n" else " ")
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1] if text[i + 1] == "\n" else " ")
                i += 2
                continue
            if (in_str and c == '"') or (in_chr and c == "'"):
                in_str = in_chr = False
            i += 1
            continue
        if c == '"':
            in_str = True
            out.append(" ")
            i += 1
            continue
        if c == "'":
            in_chr = True
            out.append(" ")
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n - 2 if j == -1 else j
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j + 2]))
            i = j + 2
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i + 2)
            j = n if j == -1 else j
            out.append(" " * (j - i))
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out)


def qualify_inline_defs(text):
    """Scan one file's (comment/string-stripped) text and return the set of
    function-definition names found, qualifying bare in-class inline method
    names (`Foo() { ... }` written directly inside `class X { ... }`) as
    `X::Foo` via a brace-depth class-context stack. Out-of-line definitions
    that already spell `ClassName::Method` in the source are left as-is."""
    code = _strip_comments_and_strings(text)
    class_open_brace = {m.end() - 1: m.group(1) for m in CLASS_RE.finditer(code)}

    events = []
    for m in BRACE_RE.finditer(code):
        events.append((m.start(), 0 if code[m.start()] == "{" else 1, None))
    for m in DEF_RE.finditer(code):
        events.append((m.start(), 2, m))
    events.sort(key=lambda e: (e[0], e[1]))  # opens(0) before defs(2) before closes(1)

    stack = []
    names = set()
    for pos, kind, m in events:
        if kind == 0:
            stack.append(class_open_brace.get(pos))
        elif kind == 1:
            if stack:
                stack.pop()
        else:
            name = m.group(1)
            base_name = name.split("::")[-1]
            if base_name in _KEYWORDS or name in _KEYWORDS:
                continue
            if "::" not in name and stack and stack[-1]:
                name = f"{stack[-1]}::{name}"
            names.add(name)
    return names


def load_symbol_names(path):
    """address(lowercase, no 0x) -> best demangled 'Namespace::name' from symbols.csv."""
    best = {}
    with open(path, newline="", encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            addr = row.get("address", "").lower()
            if not addr or addr.startswith("external"):
                continue
            if row.get("symbol_type") != "Function":
                continue
            ns = row.get("namespace", "")
            name = row.get("name", "")
            qualified = f"{ns}::{name}" if ns and ns not in ("Global", "<EXTERNAL>") else name
            if addr not in best:
                best[addr] = qualified
    return best


def scan_reconstructed_tree():
    """Walk src/ and include/ once; return (addr_set, name_set)."""
    addr_set = set()
    name_set = set()
    for base in ("src", "include"):
        root = os.path.join(SRC_ROOT, base)
        if not os.path.isdir(root):
            continue
        for dirpath, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in _SKIP_DIR_PARTS]
            for fn in files:
                if not fn.endswith(_SCAN_EXTS):
                    continue
                path = os.path.join(dirpath, fn)
                try:
                    with open(path, encoding="utf-8", errors="replace") as f:
                        text = f.read()
                except OSError:
                    continue

                for m in ADDR_RE.finditer(text):
                    off = int(m.group(1), 16)
                    addr_set.add(f"{TEXT_BASE + off:08x}")

                name_set.update(qualify_inline_defs(text))

    return addr_set, name_set


def main():
    sym_names = load_symbol_names(os.path.join(EXPORT_DIR, "symbols.csv"))
    addr_set, name_set = scan_reconstructed_tree()

    with open(os.path.join(EXPORT_DIR, "functions.csv"), newline="", encoding="utf-8", errors="replace") as f, \
         open(OUT_PATH, "w", newline="", encoding="utf-8") as out:
        reader = csv.DictReader(f)
        writer = csv.writer(out)
        writer.writerow(["address", "name", "qualified_name", "size_bytes",
                          "calling_convention", "status", "match_method"])

        n_total = 0
        counts = {"address": 0, "name": 0, "address+name": 0, "": 0}
        for row in reader:
            addr = row["entry"].lower()
            qualified = sym_names.get(addr, row["name"])
            raw_name = row["name"]

            by_addr = addr in addr_set
            # Bare (unqualified) captured definitions are only trusted against a
            # row's raw `name` when ground truth itself has no class qualifier
            # (qualified == raw_name, i.e. a true global C function like
            # ParseAuth/cm_SetUserZone). Without this guard, an in-class inline
            # method definition like `void Initialize() { ... }` -- captured as
            # the bare name "Initialize" because it has no "ClassName::" prefix
            # in the source -- would match EVERY unrelated class's own
            # Initialize() in functions.csv (192 of them, checked empirically),
            # since Ghidra's raw `name` column often omits the class too. Fully
            # qualified matches (qualified in name_set) are unaffected and are
            # the precise, high-confidence half of this heuristic.
            by_name = (qualified in name_set) or (qualified == raw_name and raw_name in name_set)

            if by_addr and by_name:
                method = "address+name"
            elif by_addr:
                method = "address"
            elif by_name:
                method = "name"
            else:
                method = ""
            status = "reconstructed" if method else "pending"

            writer.writerow([addr, raw_name, qualified, row["size_bytes"],
                              row["calling_convention"], status, method])
            n_total += 1
            counts[method] += 1

        n_recon = n_total - counts[""]

    print(f"wrote {OUT_PATH}: {n_total} functions, {n_recon} reconstructed "
          f"({100.0 * n_recon / n_total:.3f}%)")
    print(f"  by address only:      {counts['address']}")
    print(f"  by name only:         {counts['name']}")
    print(f"  by address AND name:  {counts['address+name']}")
    print(f"  pending:              {counts['']}")


if __name__ == "__main__":
    main()
