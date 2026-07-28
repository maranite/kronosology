/*
 * korg_riff.h  -  CKorgRiff, a generic RIFF-style chunked-file reader/writer
 * base, and its nested CNameChunk value type.
 *
 * FOUND 2026-07-28, same `nm -C` class-inventory sweep that landed CKorgFile
 * (korg_file.h) -- see that header's own "shared root + siblings" writeup for
 * the rejected candidates from this pass (CDirectoryElem/CFileKge/CAsyn+
 * CSubBuff). CKorgFile's own header documents ONE sibling family it
 * deliberately deferred (CKorgFileKMP/CKorgFileKSC/CKorgFileKSF, all needing
 * the project-wide-out-of-scope `CSTGMultisampleBank`). CKorgRiff is a
 * COMPLETELY SEPARATE, not-yet-mentioned second family sharing the CKorgFile
 * root: `nm -C "typeinfo for ..."` + a direct `.rodata` byte read of
 * `CKorgRiff`'s own typeinfo record (`.rodata+0x08f79d10`) confirms its single
 * base is `CKorgFile` (base-type pointer == `typeinfo for CKorgFile` @
 * 0x08f79ae8), and its own siblings-of-siblings `CKorgKmp`/`CKorgKsf` in turn
 * derive from `CKorgRiff` itself (their own typeinfo records' base-type
 * pointer == `typeinfo for CKorgRiff` @ 0x08f79d10) -- confirmed via a direct
 * `.rodata` byte read of all 3 typeinfo/vtable records, not inferred from
 * naming. `CKorgKsc` (a 4th, differently-shaped sibling) derives directly from
 * `CKorgFile` instead (15-dword vtable, matching CKorgFile's own shape
 * exactly, no extra slots) -- NOT part of this family.
 *
 * A full `objdump -d -C` call-target sweep of CKorgRiff's entire .text range
 * (0x089d1770..0x089da4a0, all 12 real methods) shows ZERO calls into any
 * other Eva class -- only libc (`fseek`/`feof`/`fread`/`fwrite`/`strncpy`)
 * and its own virtual dispatch (`IsBigEndian()`/`ReadChunk()`, through the
 * REAL runtime object's vtable -- i.e. a `CKorgKmp`/`CKorgKsf` instance's own
 * override, if this is actually one of those, same polymorphic-dispatch-
 * through-base-pointer idiom CKorgFile::Read()/Write() already established
 * for ImportToBank()/LoadChunk()). CKorgKmp/CKorgKsc/CKorgKsf/CKorgProgram
 * themselves are NOT reconstructed this pass -- CKorgRiff alone is already a
 * complete, self-contained, well-defined unit (this project's "clean value/
 * utility class" precedent, same as CKorgFile itself), and the 3 concrete
 * siblings pull in real additional depth (`CKorgKmp::AddSample()` takes a
 * `CKorgKsf const*`; `CKorgProgram`'s own `COscillator`/`CVelocitySplit`
 * nested classes manage arrays of `CKorgKmp*`) genuinely disproportionate to
 * this batch -- documented here as the natural next lead, not fabricated.
 * REAL USAGE (not dead code): `CKorgKsc` objects (a sibling of THIS class'
 * own root, `CKorgFile`) are constructed and populated by
 * `UKontaktToKorgConvert::Create()`/`::Convert()` (Kontakt-to-Korg bank
 * conversion, already-reconstructed `CKontaktXml`/`CKontaktParameter`
 * family's own natural next step) -- confirming this whole `CKorgFile`-rooted
 * hierarchy is genuinely live ground-truth code, not vestigial.
 *
 * VTABLE SHAPE (`.rodata+0x08f79cc0`, direct byte dump, 17 dwords: offset-to-
 * top, RTTI, then 15 vfunc slots -- 2 more than CKorgFile's own 13, matching
 * the +2 real methods below):
 *   slot 0-1:  ~CKorgRiff() D1/D0 (own real dtor -- OVERRIDE)
 *   slot 2-9:  Read/Write/TransferFromBegin/TransferToBegin/TransferFrom/
 *              TransferTo/TransferFromEnd/TransferToEnd -- all still
 *              CKorgFile's own bodies (INHERITED, not overridden)
 *   slot 10:   SetPath -- also inherited
 *   slot 11:   ReadFile(FILE*)  -- OVERRIDE of CKorgFile's ImportToBank slot
 *   slot 12:   WriteFile(FILE*) -- OVERRIDE of CKorgFile's LoadChunk slot
 *   slot 13:   IsBigEndian() const -- NEW virtual slot (CKorgRiff's own base
 *              body always `return false`; CKorgKmp/CKorgKsf override it)
 *   slot 14:   ReadChunk(unsigned int, unsigned int, FILE*) -- NEW virtual
 *              slot (CKorgRiff's own base body: `fseek(file, len, SEEK_CUR)`,
 *              i.e. "skip any chunk this class doesn't specifically
 *              recognize"; CKorgKmp/CKorgKsf override it to dispatch on the
 *              real chunk tag)
 *
 * OBJECT LAYOUT: CKorgFile base sub-object (0x110 bytes) immediately followed
 * by `CNameChunk mChunkName` at +0x110 (0x19/25 bytes) -- confirmed by
 * `ReadFile()`'s own `this+0x110` address computation and `WriteFile()`'s own
 * matching `lea ebx,[ebx+0x110]` before its second `fwrite`. `CNameChunk`
 * itself is a raw, not-necessarily-NUL-terminated 25-byte char buffer (see
 * `SetName()`/`GetName()` below) -- `sizeof(CNameChunk) == 0x19`.
 *
 * TAG NORMALIZATION (`ReadFile()`/`WriteHeader()`): the on-disk 4-byte chunk
 * tag is always physically stored in natural ASCII byte order (`'N','A',
 * 'M','E'`), but the in-memory 32-bit *comparison* value ReadFile() builds
 * (and WriteHeader() consumes) is BYTE-SWAPPED relative to a raw little-
 * endian load -- confirmed by the real `movbe` instruction (hardware byte-
 * swap-on-load, real Atom-class ISA support) at both call sites, and by the
 * real immediate `CMP EAX, 0x4e414d45` matching `bswap32(load_le("NAME"))`
 * exactly. Reproduced here with a portable `Bswap32()` helper rather than
 * emitting `movbe` directly (same "plain C equivalent, not literal opcode
 * transcription" convention already used project-wide, e.g. korg_file.h's
 * own `BoundedNameLen`). The chunk LENGTH field, by contrast, is only
 * conditionally swapped -- iff `IsBigEndian()` is true at that call site
 * (base class: never) -- matching a real numeric-value big/little-endian
 * conversion, not a fixed tag-byte-order normalization.
 *
 * `ReadFile()`'s own real "NAME" special-case: any chunk whose (normalized)
 * tag equals `'NAME'` is NOT dispatched to the virtual `ReadChunk()` -- its
 * 24-byte payload is read directly into `mChunkName` instead (matching the
 * `+0x18`/24-byte size `WriteFile()`'s own second `fwrite` uses). Every other
 * chunk tag goes through `ReadChunk(tag, len, file)`, dispatched via the REAL
 * runtime object's own vtable slot 14 (base: skip via fseek; overrides:
 * real per-format chunk handling, out of scope here).
 *
 * `WriteFile()`'s own trailing `strncpy(scratchHeaderBuf, &mChunkName, 0x19)`
 * (after both real `fwrite` calls) copies `mChunkName` into a LOCAL stack
 * buffer that is never subsequently read or written anywhere -- a real,
 * observably inert side effect in ground truth (bounded read of `mChunkName`,
 * result discarded), preserved faithfully rather than dropped as dead code,
 * same "preserve real ground-truth quirks verbatim" precedent already
 * established project-wide (e.g. `heap.h`'s own uninitialized-slot note).
 *
 * `SwapFile(T&)` (3 overloads: short/unsigned short/unsigned int) all share
 * one real shape: call `this->IsBigEndian()`; if true, byte-swap the
 * referenced value in place (16-bit: `rol 8`; 32-bit: real hardware
 * byte-swap). `SwapLittleEndian(T&)` (3 overloads) are literal empty `ret`
 * bodies -- true no-ops, confirmed byte-for-byte (the host/target is x86,
 * genuinely little-endian, so "convert a little-endian value to host order"
 * is nothing). `SwapBigEndian(T&)` and `Swap(T&)` (3 overloads each) are
 * BYTE-IDENTICAL unconditional-swap bodies to each other -- two distinctly-
 * named ground-truth symbols with the same real machine code, transcribed as
 * two separate (but implementation-sharing) methods, not collapsed into one,
 * matching this project's "distinct ground-truth symbols stay distinct"
 * convention even when bit-identical (e.g. CKorgFile::MakeNameRight/Left's
 * own thin-wrapper precedent, one level up in genuinely-distinct-symbol
 * terms).
 */

#ifndef KORG_RIFF_H
#define KORG_RIFF_H

#include "korg_file.h"

class CKorgRiff : public CKorgFile {
public:
	/* Raw 25-byte chunk-name buffer -- NOT guaranteed NUL-terminated by
	 * SetName() alone (see .cpp); GetName() defensively force-terminates
	 * the CALLER's own destination buffer.
	 */
	class CNameChunk {
	public:
		/* .text+0x089d19d0, __cdecl (this passed as the strncpy source
		 * pointer directly, no vtable/thiscall machinery -- a plain
		 * value-type accessor). strncpy(dest, this, 0x19); dest[0x18]=0.
		 */
		void GetName(char *dest, unsigned int maxLen) const;

		/* .text+0x089d1a00. strncpy(this, name, 0x18) -- writes at most
		 * 24 data bytes, byte 24 (mName[0x18]) left whatever it was.
		 */
		void SetName(const char *name);

	private:
		char mName[0x19];

		friend class CKorgRiff;
	};

	/* .text+0x089d1990, 52 bytes. Chains CKorgFile::CKorgFile(name, ext),
	 * installs CKorgRiff's own vtable, clears mChunkName's first byte
	 * (mName[0] = 0) -- the ONLY field this ctor itself initializes past
	 * the base-class chain.
	 */
	CKorgRiff(const char *name, const char *ext);

	/* .text+0x089d17a0 (D1) / 0x089d17c0 (D0). Real body: reinstall
	 * CKorgRiff's own vtable pointer (standard virtual-dtor-during-
	 * teardown idiom), then chain to CKorgFile::~CKorgFile() (D1: tail
	 * call; D0: regular call, then `operator delete(this)`).
	 */
	virtual ~CKorgRiff();

	/* .text+0x089d17f0, 231 bytes. In ground truth this OVERRIDES
	 * CKorgFile's ImportToBank vtable slot (same real slot 11) -- but
	 * CKorgFile::Read()/Write() (korg_file.cpp) already call
	 * ImportToBank()/LoadChunk() with NO arguments (that header's own
	 * documented "placeholder signature... never called by any
	 * reconstructed code" convention), so tying ReadFile()/WriteFile()
	 * to those zero-arg pure virtuals would lose the real `FILE*` this
	 * class's own logic genuinely needs. Modeled instead as an ordinary
	 * (non-override) virtual with its real signature -- the actual
	 * ground-truth ABI slot-sharing is a fact about the compiled binary,
	 * not something a clean recompilable header needs to reproduce given
	 * the base class already made that slot pair non-functional. See
	 * file header for the full "NAME"-chunk-vs-ReadChunk() dispatch
	 * loop. Returns the last dispatched ReadChunk() call's own return
	 * value (0 if the file had no non-NAME chunks, matching
	 * ReadChunk()'s own base-class `return 0`).
	 */
	virtual int ReadFile(FILE *file);

	/* .text+0x089d18e0, 167 bytes. Same "real slot 12 override, modeled
	 * as its own ordinary virtual" situation as ReadFile() above. Writes
	 * a "NAME" header (WriteHeader-shaped, but inlined here rather than
	 * calling it -- ground truth's own real body, not a refactor) then
	 * mChunkName's own 24 payload bytes. See file header for the
	 * trailing inert strncpy().
	 */
	virtual int WriteFile(FILE *file);

	/* CKorgFile's own pure virtuals -- given trivial bodies here purely
	 * so CKorgRiff is instantiable; ReadFile()/WriteFile() above are the
	 * real, callable entry points (see their own comments). Matches
	 * CKorgFile's own "never called by any reconstructed code" framing
	 * for this zero-arg slot pair.
	 */
	virtual int ImportToBank() { return -1; }
	virtual int LoadChunk() { return -1; }

	/* .text+0x089d1770, 39 bytes. NEW virtual slot -- base body:
	 * `fseek(file, (long)len, SEEK_CUR); return 0;` (skip an unrecognized
	 * chunk). `id` is accepted (matching every override's real signature)
	 * but genuinely unused by this base implementation.
	 */
	virtual int ReadChunk(unsigned int id, unsigned int len, FILE *file);

	/* .text+0x089da490, 3 bytes. NEW virtual slot -- base body:
	 * `return false;` unconditionally (x86 target is little-endian by
	 * default; real per-format overrides, e.g. CKorgKmp/CKorgKsf, flip
	 * this for big-endian on-disk formats -- not reconstructed here).
	 */
	virtual bool IsBigEndian() const;

	/* .text+0x089d1a30, 91 bytes. Writes an 8-byte {tag,len} chunk header:
	 * tag = Bswap32(id) (undo ReadFile()'s own normalization direction --
	 * see file header), len = IsBigEndian() ? Bswap32(len) : len.
	 */
	void WriteHeader(unsigned int id, unsigned int len, FILE *file);

	/* .text+0x089d1a90 (short&), 0x089d1ac0 (unsigned short&),
	 * 0x089d1af0 (unsigned int&). Conditional swap: byte-swaps `*ref` in
	 * place iff `IsBigEndian()` is true, otherwise a no-op.
	 */
	void SwapFile(short &ref);
	void SwapFile(unsigned short &ref);
	void SwapFile(unsigned int &ref);

	/* .text+0x089d1b20/0x089d1b30/0x089d1b40. Literal empty bodies --
	 * true no-ops on this (little-endian) target, see file header.
	 */
	static void SwapLittleEndian(short &ref);
	static void SwapLittleEndian(unsigned short &ref);
	static void SwapLittleEndian(unsigned int &ref);

	/* .text+0x089d1b50/0x089d1b60/0x089d1b70. Unconditional byte-swap. */
	static void SwapBigEndian(short &ref);
	static void SwapBigEndian(unsigned short &ref);
	static void SwapBigEndian(unsigned int &ref);

	/* .text+0x089d1b80/0x089d1b90/0x089d1ba0. Byte-identical bodies to
	 * SwapBigEndian() above -- distinct ground-truth symbols, kept
	 * distinct (see file header).
	 */
	static void Swap(short &ref);
	static void Swap(unsigned short &ref);
	static void Swap(unsigned int &ref);

private:
	CNameChunk mChunkName;

	friend struct KorgRiffTestHooks;
};

#endif /* KORG_RIFF_H */
