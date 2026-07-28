/*
 * list.h  -  CObject / CAbstList / CUsrList / CList / CStaticList / CListIter, Eva's
 * foundational intrusive circular doubly-linked-list container family. Pervasively used
 * (e.g. `CIconList`, `CPatchJack`, `COverGraffiti` in the Peg UI layer, `CLoadedItemInfoArray`
 * elsewhere) -- this batch reconstructs the family itself, not any of its callers.
 *
 * FOUND 2026-07-28, fresh `nm -C` class-inventory sweep for the next dense, mechanical,
 * previously-100%-untouched cluster (following CSeqPat/CPatternDataHolder). One candidate
 * traced and REJECTED before settling on this one:
 *   - `CWaveformTemplate` (28 methods, LFO waveform-shape generator: `EquationTriangle`/
 *     `EquationSaw`/.../`EquationRandomSH1..3`/`EquationExpSawUp`/etc, `MakeShapeTable`,
 *     `DrawWave`) -- genuinely self-contained (zero external subsystem dependency: pure
 *     integer/x87-FPU math plus one leaf vtable call into a global Peg screen singleton in
 *     `DrawWave` alone), but the per-equation bodies are dense GCC divide-by-constant-via-
 *     multiply idioms and multi-branch quantization thresholds that would need much deeper
 *     manual register tracing to reproduce with confidence than this pass's budget allowed.
 *     Left untouched, not marked reconstructed -- a real, valid future candidate, just not
 *     this pass's pick.
 *
 * REACHABILITY: traced via a full `objdump -d` xref sweep over the whole family's address
 * range (.text+0x08bd0440..0x08bd1b90). External call targets: `CObject::~CObject()` (this
 * family's own base, reconstructed here), `operator new`/`operator new[]`/`operator delete`,
 * and `_Unwind_Resume@plt` (standard C++ exception-unwind landing pad, already an
 * established pattern elsewhere in this project) -- nothing else. Zero dependency on any
 * unmodeled subsystem.
 *
 * SListNode -- the intrusive node type, confirmed by every `makenode_sub`/`operator new`
 * call site (`malloc(0xc)`/`new SListNode` sized exactly 12 bytes) and every node field
 * access throughout the family:
 *   +0x00  mPrev   SListNode*
 *   +0x04  mNext   SListNode*  (CStaticList's pool allocator reuses this same field as its
 *                   singly-linked free-node-chain pointer while a node sits on the free list)
 *   +0x08  mItem   CObject*
 *
 * CObject -- minimal common base, confirmed by both list-node destructors (0x08bd1b90 D1:
 * reset vptr, return; 0x08bd1ba0 D0: reset vptr, `operator delete(this)`) matching exactly
 * what a plain `virtual ~CObject() {}` compiles to under the Itanium C++ ABI -- no other
 * real method exists on it anywhere in `nm -C`'s output for this binary.
 *
 * CAbstList -- abstract circular-doubly-linked-list base, confirmed layout (0xc bytes):
 *   +0x00  vtbl
 *   +0x04  mFirst  SListNode*, the list's front/head node (`firstitem()` returns
 *                   `mFirst->mItem` directly; NULL when empty). The ring is circular, so
 *                   `mFirst->mPrev` is the LAST node (`lastitem()` returns
 *                   `mFirst->mPrev->mItem`) -- `append()` splices new nodes in between the
 *                   current last node and `mFirst` WITHOUT moving `mFirst` (so newly
 *                   appended items become the new last element); `prepend()` does the
 *                   identical splice but ALSO reassigns `mFirst` to the new node (so it
 *                   becomes the new front). Confirmed by direct comparison of `append()`
 *                   (.text+0x08bd0b00) vs `prepend()` (.text+0x08bd0b60) -- byte-identical
 *                   splice sequence, `prepend()` has exactly one extra `mFirst = newNode`
 *                   store `append()` lacks.
 *   +0x08  mCount  int, number of items
 * `makenode_sub(CObject*)`/`deletenode_sub(SListNode*)` are real, PURE virtual (CAbstList
 * itself defines neither; only `CUsrList`/`CList`/`CStaticList` do) -- confirmed real
 * Itanium-ABI vtable slots 2/3 (`call *0x8(%eax)`/`call *0xc(%eax)` throughout), slots 0/1
 * being the usual two destructor variants. All "walk N nodes forward/backward from a known
 * position" methods (`insertat`, `nthitem`, `findnode(long)`, `CListIter`'s positional
 * ctor/`init`) are transcribed here as plain loops -- real ground truth is a GCC
 * Duff's-device 8-way-unrolled walk of the exact same circular-list steps, collapsed per
 * this project's usual "reproduce behavior, not compiler artifacts" convention.
 *
 * `~CAbstList()` itself (.text+0x08bd04e0/0x08bd0500) does NOT walk/free any remaining
 * nodes -- it is a bare vptr-reset-then-chain-to-`~CObject()`. Each concrete subclass's own
 * destructor performs the walk instead, and every one of them (`CUsrList`, `CList`,
 * `CStaticList`) reproduces the EXACT SAME "unsplice + `deletenode_sub()` per node, virtual"
 * loop already present as `removeallnode()`'s own body -- almost certainly GCC inlining a
 * real `{ removeallnode(); }` dtor body 3 separate times rather than 3 independently-written
 * loops (`CStaticList`'s dtor additionally frees its pool array afterward). Reproduced here
 * as an explicit `removeallnode()` call from each subclass dtor -- behaviorally identical,
 * avoids tripling the loop in source.
 *
 * `dispose(CObject*)` vs `remove(CObject*)`: `remove()` only unsplices matching node(s) and
 * frees the NODE via the virtual `deletenode_sub()` (leaves the item itself alone).
 * `dispose()` does the identical unsplice-and-free-node loop, then ADDITIONALLY invokes the
 * removed item's own real virtual deleting destructor directly (`(*(*item)[1])(item)` --
 * i.e. `delete item`), confirmed by the direct tail-call to `*(*item vtbl)[1]` at the very
 * end of .text+0x08bd0f60. Both `remove()`/`dispose()` loop over ALL matching nodes, not
 * just the first (confirmed: the real disassembly re-tests the search loop after every
 * splice rather than returning after one match).
 *
 * CUsrList / CList -- both own-node-allocating, item-agnostic subclasses; `makenode_sub`/
 * `deletenode_sub` bodies are byte-identical between them (`new SListNode`/
 * `operator delete(node)`, no item disposal) -- the real distinction between the two
 * (`CUsrList` intended for `dispose()`-driven item ownership vs `CList` for plain
 * reference/iteration use, going by the two names and Peg-layer call sites) is a usage
 * convention this reconstruction does not need to enforce; both are modeled faithfully as
 * mechanically-identical concrete classes, matching real ground truth even though it reads
 * as redundant.
 *
 * CStaticList -- fixed-capacity variant backed by a single `new SListNode[capacity]` pool
 * (ctor at .text+0x08bd1540) instead of per-node heap allocation:
 *   +0x0c  mPool       SListNode*  (`operator new[]`'d array, freed by `~CStaticList()`)
 *   +0x10  mFreeHead   SListNode*  head of a singly-linked (via each node's own mNext
 *                       field) free-node chain
 *   +0x14  mFreeTail   SListNode*  tail of the same free chain
 *   +0x18  mCapacity   int
 * `makenode_sub()` pops the free-chain head (returns NULL, no allocation, if
 * `mCount >= mCapacity`); `deletenode_sub()` pushes the node back onto the free-chain tail.
 * `~CStaticList()` calls `removeallnode()` then frees `mPool` (`operator delete[]`).
 *
 * CListIter -- external (non-intrusive) iterator/cursor over a `const CAbstList&`, 4 fields
 * confirmed by every ctor/`init`/`operator++`/`operator--`/`operator()` body:
 *   +0x00  mList     const CAbstList*
 *   +0x04  mCurNode  SListNode*
 *   +0x08  mAtTail   bool-as-int, set once `operator++()` walks past the last real node
 *                     back to `mList->mFirst` (forward wrap boundary)
 *   +0x0c  mAtTop    bool-as-int, set by the `kIterHead`-style (EIteratorPos!=0) and
 *                     "index==0" positional ctors/`init` overloads to mean "conceptually
 *                     positioned just before the first element" -- the corresponding
 *                     `operator--()` clamp/`operator++()` extra-step-consumption logic is
 *                     the exact mirror of `mAtTail`'s. `operator()()` (dereference) returns
 *                     NULL whenever EITHER boundary flag is set, matching the real
 *                     disassembly's two sequential flag tests.
 * Positional ctor/`init(list, EIteratorPos)`: `pos == 0` positions at `mList->mFirst`
 * directly (`mAtTop` stays 0); `pos != 0` positions at `mList->mFirst->mPrev` (the real
 * LAST node) -- i.e. `EIteratorPos` selects front-vs-back start position, consistent with
 * how `firstitem()`/`lastitem()` read the same ring.
 */

#ifndef LIST_H
#define LIST_H

/* CObject -- minimal common base for every list-held item; see file header. */
class CObject {
public:
	virtual ~CObject() {}
};

/* Intrusive node -- see file header. */
struct SListNode {
	SListNode *mPrev;
	SListNode *mNext;
	CObject *mItem;
};

/* Positional constant used by CListIter's positional ctor/init overloads -- real ground
 * truth only distinguishes "zero" (front) from "nonzero" (back); the exact enumerator
 * names/values beyond that are not confirmed. */
enum EIteratorPos {
	kIterFront = 0,
	kIterBack = 1,
};

/* CAbstList -- abstract circular doubly-linked-list base; see file header. */
class CAbstList : public CObject {
	/* CListIter reads mFirst directly, matching the real disassembly's direct
	 * offset+4 field access (no accessor call). */
	friend class CListIter;

public:
	CAbstList();
	virtual ~CAbstList();

	virtual SListNode *makenode_sub(CObject *item) = 0;
	virtual void deletenode_sub(SListNode *node) = 0;

	SListNode *makenode(CObject *item);
	void deletenode(SListNode *node);
	void deleteallnode();

	SListNode *findnode(CObject *item) const;
	SListNode *findnode(long index) const;
	/* Lower-level helper: unsplices an already-located node (updating mFirst if it was
	 * the head) WITHOUT touching mCount -- distinct from remove(CObject*) below, which
	 * searches by value and does decrement mCount. Real ground truth (.text+0x08bd0ab0)
	 * has no currently-traced caller within this batch's own address range; reproduced
	 * as-is since it is a real exported method. */
	void remove(SListNode *node);

	bool append(CObject *item);
	bool prepend(CObject *item);
	/* Argument order confirmed by direct disassembly (.text+0x08bd0bc0): the search for
	 * the splice point matches the SECOND parameter (afterItem); makenode_sub() is
	 * called with the FIRST (item) -- i.e. `insertafter(item, afterItem)`, not the
	 * other way round. */
	bool insertafter(CObject *item, CObject *afterItem);
	bool insertat(CObject *item, long index);

	void remove(CObject *item);
	CObject *removefirst();
	CObject *removelast();
	void dispose(CObject *item);
	void disposeall();
	void removeallnode();

	/* Not const in real ground truth (nm -C shows no trailing "const" on any of these,
	 * unlike the two findnode() overloads above). */
	CObject *prev(CObject *item);
	CObject *next(CObject *item);

	void bringfront(CObject *item);
	void sendback(CObject *item);
	void moveup(CObject *item);
	void movedown(CObject *item);

	long getnumitems() { return mCount; }
	CObject *firstitem();
	CObject *lastitem();
	CObject *nthitem(long index);
	long findindex(CObject *item);
	bool includes(CObject *item);

protected:
	SListNode *mFirst;
	long mCount;
};

/* CUsrList / CList -- see file header (mechanically identical concrete node-owning
 * subclasses). */
class CUsrList : public CAbstList {
public:
	CUsrList();
	virtual ~CUsrList();

	virtual SListNode *makenode_sub(CObject *item);
	virtual void deletenode_sub(SListNode *node);
};

class CList : public CAbstList {
public:
	CList();
	virtual ~CList();

	virtual SListNode *makenode_sub(CObject *item);
	virtual void deletenode_sub(SListNode *node);
};

/* CStaticList -- fixed-capacity pool-backed subclass; see file header. */
class CStaticList : public CAbstList {
public:
	explicit CStaticList(int capacity);
	virtual ~CStaticList();

	virtual SListNode *makenode_sub(CObject *item);
	virtual void deletenode_sub(SListNode *node);

private:
	SListNode *mPool;
	SListNode *mFreeHead;
	SListNode *mFreeTail;
	long mCapacity;
};

/* CListIter -- non-owning external cursor over a CAbstList; see file header. */
class CListIter {
public:
	CListIter();
	CListIter(const CAbstList &list, EIteratorPos pos);
	CListIter(const CAbstList &list, CObject *atItem);
	CListIter(const CAbstList &list, long index);
	~CListIter();

	void init(const CAbstList &list, EIteratorPos pos);
	void init(const CAbstList &list, CObject *atItem);
	void init(const CAbstList &list, long index);

	void totop();
	void totail();

	CObject *operator()() const;
	void operator++();
	void operator--();

private:
	void inititer(const CAbstList *list);

	const CAbstList *mList;
	SListNode *mCurNode;
	bool mAtTail;
	bool mAtTop;
};

#endif /* LIST_H */
