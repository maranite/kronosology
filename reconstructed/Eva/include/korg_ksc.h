/*
 * korg_ksc.h  -  CKorgKsc, the Korg .KSC sample-collection (multisample
 * database) file class. Partial reconstruction -- see korg_kmp.h's own file
 * header for the full "shared root + siblings" writeup, the corrected
 * CSTGMultisampleBank/name-collision finding, and why this whole family is a
 * deep on-disk-format leaf rather than a CSTGMultisampleBank-blocked one.
 * Same batch, same conventions. Unlike CKorgKmp/CKorgKsf (both CKorgRiff
 * siblings), CKorgKsc derives DIRECTLY from CKorgFile (confirmed via its own
 * `.rodata` vtable/typeinfo dump -- see korg_riff.h's own "SHARED ROOT +
 * SIBLINGS" note, "a 4th, differently-shaped sibling").
 *
 * WHAT IS RECONSTRUCTED HERE:
 *   - CKorgKsc(name, uuid, fieldA, fieldB) -- the REAL 4-argument ctor
 *     (.text+0x089cef70, 257 bytes). Chains CKorgFile(name, ".KSC"); stores
 *     `fieldA`/`fieldB` verbatim (real offsets 0x250/0x251, both bool per
 *     the real mangled signature -- no independently-recoverable semantic
 *     meaning, not read by any method reconstructed here); if `uuid` is
 *     non-NULL, bounded-copies it into a 64-byte `mUUID` (else clears it to
 *     empty). Ground truth's own ctor ALSO makes a second, redundant call to
 *     the (deferred, NOT reconstructed this pass) `CKorgKsc::SetPath()`
 *     override using the same `name` argument again -- not reproduced here
 *     (the base `CKorgFile` ctor it already chains to performs equivalent
 *     path/extension normalization; the override's own additional real
 *     behavior, 349 bytes, is out of scope this pass). Real ctor also
 *     zero-inits 3 `std::list<T*>` members and 1 `std::vector<CKorgProgram*>`
 *     (`mPrograms`/two `std::list<std::string*>`/`std::vector<CKorgProgram*>`
 *     confirmed via the dtor's own real `_M_clear`/`~vector` calls) -- not
 *     modeled here since nothing reconstructed in this pass touches them.
 *   - ~CKorgKsc() -- .text+0x089ceb40 (D1) / 0x089cef50 (D0). Real body
 *     empty (ground truth's own real work is exactly the member-list/vector
 *     teardown already noted above; not needed once those members simply
 *     aren't declared).
 *   - GetUUID(dest, maxLen) const / SetUUID(uuid) -- .text+0x089ce100 /
 *     0x089ce150. Bounded copy over the 64-byte `mUUID`; GetUUID is a no-op
 *     if `dest` is NULL (real, confirmed NULL check); SetUUID clears to
 *     empty if `uuid` is NULL.
 *   - MakeFolder() -- .text+0x089ce1a0. Byte-identical body to
 *     `CKorgKmp::MakeFolder()` (korg_kmp.h) -- transcribed as its own
 *     distinct symbol per this project's "distinct ground-truth symbols stay
 *     distinct" convention.
 *
 * NOT RECONSTRUCTED THIS PASS: Read()/Write()/ReadFile()/WriteFile() (the
 * real .KSC chunked-database parser -- WriteFile() alone is 1515 bytes),
 * SetPath() (349-byte real override), AddProgram()/AddMultisample() (6
 * overloads across this family)/GetMultisample() (5 overloads)/GetSample()/
 * AddSkippedSamples()/ReadLine(), and the `mPrograms`
 * (`std::vector<CKorgProgram*>`) / 2x `std::list<std::string*>` /
 * `std::list<CKorgKmp*>` members those methods manage.
 */

#ifndef KORG_KSC_H
#define KORG_KSC_H

#include "korg_file.h"

class CKorgKsc : public CKorgFile {
public:
	/* .text+0x089cef70, 257 bytes. See file header for full provenance. */
	CKorgKsc(const char *name, const char *uuid, bool fieldA, bool fieldB);

	/* .text+0x089ceb40 (D1) / 0x089cef50 (D0). Real body empty (once the
	 * deferred list/vector members aren't declared -- see file header).
	 */
	virtual ~CKorgKsc();

	/* .text+0x089ce100. No-op if dest is NULL. */
	void GetUUID(char *dest, unsigned int maxLen) const;

	/* .text+0x089ce150. Clears mUUID to empty if uuid is NULL. */
	void SetUUID(const char *uuid);

	/* .text+0x089ce1a0. GetFolder(buf,0x100); mkdir(buf,0777). */
	void MakeFolder();

	/* CKorgFile's own pure virtuals -- given trivial bodies here purely so
	 * CKorgKsc is instantiable, same convention as CKorgRiff's own
	 * ImportToBank()/LoadChunk() (korg_riff.h). CKorgKsc's real overrides
	 * (ReadFile()/WriteFile()-shaped, per korg_file.h's own documented
	 * "each sibling's own real signature differs" note) are not
	 * reconstructed this pass.
	 */
	virtual int ImportToBank() { return -1; }
	virtual int LoadChunk() { return -1; }

private:
	char mUUID[0x40];  /* real offset 0x210 */
	bool mFieldA;      /* real offset 0x250, ctor arg, purpose unknown */
	bool mFieldB;      /* real offset 0x251, ctor arg, purpose unknown */

	friend struct KorgKscTestHooks;
};

#endif /* KORG_KSC_H */
