# Set Lists & Templates — `STLS.BIN`, `STMPP.BIN`, `STMPU.BIN`

The Kronos's **Set List** mode is a "live-performance lookup table" — 128 slots per set
list, where each slot points at a Program or a Combi (or another set list) and includes
its own display name, "comments" field, transpose, and KARMA settings. Set Lists are
the Kronos's answer to traditional performance-mode banks.

| Property | Value |
|---|---|
| File | `STLS.BIN` — the set-list store |
| File size | 8 885 268 bytes |
| Header magic | `PSTL` |
| Set lists | **128** (header `record_count = 0x80`) |
| Record size | **69 416 bytes** per set list (header `record_size = 0x10F28`) |
| Container | See [container_format.md](container_format.md) |

---

## Why each set list is 69 416 bytes

A single set list has 128 slots. Each slot can override the referenced program's name,
transpose, tempo, KARMA latch state, EQ, comments — and the *comments* field alone can
hold ~512 bytes of free text. So a per-slot record is ≈ 542 bytes, and 128 × 542 plus
common parameters ≈ 69 416 bytes total.

```
Offset       Size       Field
-------------------------------------------------------------------
0x000        24         Set-list name (ASCII, null-padded)
0x018       ~16         Common: default EQ, default font size, default tempo, etc.
0x028     128 × 542     Per-slot entry, each:
                          • slot type (Program / Combi / Set-List)
                          • bank + index
                          • display name override (24 chars)
                          • transpose
                          • tempo override
                          • KARMA latch
                          • comments (~480 chars)
                          • per-slot EQ
0x10F00     ~40         Reserved / padding
-------------------------------------------------------------------
Total: 69 416 bytes
```

---

## Templates — `STMPP.BIN` (preset) and `STMPU.BIN` (user)

| File | Size | Magic | Records | Record size |
|---|---|---|---|---|
| `STMPP.BIN` | 235 460 | `TMPU` | 18 | 13 080 |
| `STMPU.BIN` | 209 091 | *none — starts directly with first template name* | — | — |

A **set-list template** is a saved style of slot formatting — colours, comment-block
layout, default tempo behaviour, KARMA defaults. Korg ships 18 templates (`STMPP.BIN`)
and lets the user define more (`STMPU.BIN`).

`STMPP.BIN` follows the standard P-magic container despite reading `TMPU` not `PXXX` —
it's a tagged exception. `STMPU.BIN` does not follow the container format at all, just
starts with the first template's data (the first 24 bytes of which is the template name
"Breakbeat" in the observed file).

---

## Loader

`CSetListBank::Initialize`, called from `CSTGGlobal::Initialize` (`0x00008340`), loads
`STLS.BIN`. The set-list bank object is at `this->pPad_0x33ce + 0x293037e` in the
`CSTGGlobal` memory area (consistent with how programs and combis are stored at
high-offset slots).

Eva handles the templates (`STMPP.BIN`, `STMPU.BIN`) — they're never accessed directly
by the audio engine.

---

## Write path

Eva's `CFormDlogSetListWrite` form handles the **WRITE** action on a set list. It
serializes the in-edit set-list buffer into the corresponding 69 416-byte record at
`STLS.BIN` offset `20 + index × 69 416`.

---

## Set List vs Combi — what's the difference?

| | Set List | Combi |
|---|---|---|
| Slots | 128 per set list, sequential | 16 timbres, layered/split |
| Purpose | Live performance — quick patch change | Performance — split / layered single sound |
| Per-slot routing | Single program reference + overrides | 16 timbres play together |
| Tempo lock | Per-slot can override | One tempo per combi |
| Setlist-of-setlists | Yes — slot can reference another set list | No |
| Use case | Gigs — one set list per song | Sound design — one combi per multitimbral sound |

---

## See also

- [container_format.md](container_format.md)
- [program_banks.md](program_banks.md), [combi_banks.md](combi_banks.md) — what slots reference
- [extension_points.md](extension_points.md) — could there be more than 128 set lists?
- `KRONOS_Op_Guide_E10.pdf`, ch. on Set List mode — concept reference
- `KRONOS_Param_Guide_E10.pdf`, p. 270+ — set-list parameter reference
