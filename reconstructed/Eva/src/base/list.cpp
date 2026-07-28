/*
 * list.cpp  -  see include/list.h.
 *
 * Transcribed from the CObject/CAbstList/CUsrList/CList/CStaticList/CListIter family at
 * .text+0x08bd0440..0x08bd1b90.
 */

#include "list.h"

#include <new>

/* ---- CAbstList ---- */

CAbstList::CAbstList() : mFirst(0), mCount(0)
{
}

CAbstList::~CAbstList()
{
	/* Real ground truth: bare vptr-reset-then-chain-to-~CObject(), no node walk here --
	 * see file header. */
}

SListNode *CAbstList::makenode(CObject *item)
{
	SListNode *node = makenode_sub(item);
	if (node)
		++mCount;
	return node;
}

void CAbstList::deletenode(SListNode *node)
{
	if (node) {
		deletenode_sub(node);
		--mCount;
	}
}

void CAbstList::deleteallnode()
{
	SListNode *node = mFirst;
	while (node) {
		SListNode *next = node->mNext;
		mFirst = (next == node) ? 0 : next;
		node->mPrev->mNext = node->mNext;
		node->mNext->mPrev = node->mPrev;
		deletenode_sub(node);
		--mCount;
		node = mFirst;
	}
	mCount = 0;
}

SListNode *CAbstList::findnode(CObject *item) const
{
	SListNode *node = mFirst;
	if (!node)
		return 0;
	do {
		if (node->mItem == item)
			return node;
		node = node->mNext;
	} while (node != mFirst);
	return 0;
}

SListNode *CAbstList::findnode(long index) const
{
	SListNode *node = mFirst;
	if (!node || index < 0)
		return 0;
	if (index == 0)
		return node;
	for (long i = 0; i < index; ++i) {
		node = node->mNext;
		if (node == mFirst)
			return 0;
	}
	return node;
}

void CAbstList::remove(SListNode *node)
{
	if (node == mFirst)
		mFirst = (node->mNext == node) ? 0 : node->mNext;
	node->mPrev->mNext = node->mNext;
	node->mNext->mPrev = node->mPrev;
}

bool CAbstList::append(CObject *item)
{
	SListNode *node = makenode_sub(item);
	if (!node)
		return false;
	++mCount;

	SListNode *head = mFirst;
	if (!head) {
		node->mNext = node;
		node->mPrev = node;
		mFirst = node;
		return true;
	}

	SListNode *tail = head->mPrev;
	node->mNext = head;
	node->mPrev = tail;
	head->mPrev = node;
	tail->mNext = node;
	return true;
}

bool CAbstList::prepend(CObject *item)
{
	SListNode *node = makenode_sub(item);
	if (!node)
		return false;
	++mCount;

	SListNode *head = mFirst;
	if (!head) {
		node->mNext = node;
		node->mPrev = node;
		mFirst = node;
		return true;
	}

	SListNode *tail = head->mPrev;
	node->mNext = head;
	node->mPrev = tail;
	head->mPrev = node;
	tail->mNext = node;
	mFirst = node;
	return true;
}

bool CAbstList::insertafter(CObject *item, CObject *afterItem)
{
	SListNode *after = 0;
	SListNode *node = mFirst;
	if (node) {
		do {
			if (node->mItem == afterItem) {
				after = node;
				break;
			}
			node = node->mNext;
		} while (node != mFirst);
	}

	SListNode *newNode = makenode_sub(item);
	if (!newNode)
		return false;
	++mCount;

	if (!after) {
		/* No match -- real ground truth still makes the node but leaves it unlinked
		 * (matches .text+0x08bd0bee..0x08bd0c1d: the splice block is skipped
		 * entirely when `after` stays NULL). */
		return true;
	}

	newNode->mPrev = after;
	newNode->mNext = after->mNext;
	after->mNext->mPrev = newNode;
	after->mNext = newNode;
	return true;
}

bool CAbstList::insertat(CObject *item, long index)
{
	SListNode *before = 0; /* node to insert BEFORE */
	bool atFront = false;

	if (index == 0) {
		atFront = true;
	} else if (mFirst) {
		SListNode *node = mFirst;
		if (index == 1) {
			before = node;
		} else {
			long steps = index - 1;
			for (long i = 0; i < steps; ++i) {
				node = node->mNext;
				if (node == mFirst) {
					before = 0;
					goto splice;
				}
			}
			before = node;
		}
	}

splice:
	SListNode *newNode = makenode_sub(item);
	if (!newNode)
		return false;
	++mCount;

	if (atFront) {
		SListNode *head = mFirst;
		if (!head) {
			newNode->mNext = newNode;
			newNode->mPrev = newNode;
			mFirst = newNode;
			return true;
		}
		SListNode *tail = head->mPrev;
		newNode->mNext = head;
		newNode->mPrev = tail;
		head->mPrev = newNode;
		tail->mNext = newNode;
		mFirst = newNode;
		return true;
	}

	if (!before) {
		/* Real ground truth: node still made (and mCount already bumped above) but
		 * left unlinked when the requested position couldn't be found -- same
		 * "make first, splice conditionally" shape as insertafter(). */
		return true;
	}

	newNode->mPrev = before;
	newNode->mNext = before->mNext;
	before->mNext->mPrev = newNode;
	before->mNext = newNode;
	return true;
}

void CAbstList::remove(CObject *item)
{
	SListNode *node = mFirst;
	if (!node)
		return;
	while (true) {
		SListNode *next = node->mNext;
		if (node->mItem == item) {
			if (node == mFirst)
				mFirst = (next == node) ? 0 : next;
			node->mPrev->mNext = node->mNext;
			node->mNext->mPrev = node->mPrev;
			deletenode_sub(node);
			--mCount;
			node = mFirst;
			if (!node)
				return;
			continue;
		}
		if (next == mFirst)
			return;
		node = next;
	}
}

CObject *CAbstList::removefirst()
{
	SListNode *node = mFirst;
	if (!node)
		return 0;
	CObject *item = node->mItem;
	mFirst = (node->mNext == node) ? 0 : node->mNext;
	node->mPrev->mNext = node->mNext;
	node->mNext->mPrev = node->mPrev;
	deletenode_sub(node);
	--mCount;
	return item;
}

CObject *CAbstList::removelast()
{
	if (!mFirst)
		return 0;
	SListNode *node = mFirst->mPrev;
	CObject *item = node->mItem;
	if (node == mFirst)
		mFirst = 0;
	node->mPrev->mNext = node->mNext;
	node->mNext->mPrev = node->mPrev;
	deletenode_sub(node);
	--mCount;
	return item;
}

void CAbstList::dispose(CObject *item)
{
	SListNode *node = mFirst;
	if (node) {
		while (true) {
			SListNode *next = node->mNext;
			if (node->mItem == item) {
				if (node == mFirst)
					mFirst = (next == node) ? 0 : next;
				node->mPrev->mNext = node->mNext;
				node->mNext->mPrev = node->mPrev;
				deletenode_sub(node);
				--mCount;
				node = mFirst;
				if (!node)
					break;
				continue;
			}
			if (next == mFirst)
				break;
			node = next;
		}
	}
	if (item)
		delete item;
}

void CAbstList::disposeall()
{
	SListNode *node = mFirst;
	while (node) {
		SListNode *next = node->mNext;
		mFirst = (next == node) ? 0 : next;
		node->mPrev->mNext = node->mNext;
		node->mNext->mPrev = node->mPrev;
		if (node->mItem)
			delete node->mItem;
		deletenode_sub(node);
		--mCount;
		node = mFirst;
	}
	mCount = 0;
}

void CAbstList::removeallnode()
{
	SListNode *node = mFirst;
	while (node) {
		SListNode *next = node->mNext;
		mFirst = (next == node) ? 0 : next;
		node->mPrev->mNext = node->mNext;
		node->mNext->mPrev = node->mPrev;
		deletenode_sub(node);
		--mCount;
		node = mFirst;
	}
	mCount = 0;
}

/* Real ground truth (.text+0x08bd0fe0) falls through into an unguarded `mPrev` read of
 * whatever register held the (possibly still-zero, "not found") search result -- a real
 * null-deref on a not-in-list item that this reconstruction treats defensively (return 0)
 * instead of reproducing literally, since every traced call site only ever passes an
 * already-known-member item. The one behavior that IS reproduced exactly: `prev()` of the
 * FIRST item returns 0 rather than wrapping to the real last item. */
CObject *CAbstList::prev(CObject *item)
{
	SListNode *head = mFirst;
	if (!head)
		return 0;
	SListNode *found = 0;
	SListNode *cursor = head;
	do {
		if (cursor->mItem == item) {
			found = cursor;
			break;
		}
		cursor = cursor->mNext;
	} while (cursor != head);
	if (!found || found == head)
		return 0;
	return found->mPrev->mItem;
}

/* Mirror of prev() -- see its comment. `next()` of the LAST item returns 0 rather than
 * wrapping to the real first item. */
CObject *CAbstList::next(CObject *item)
{
	SListNode *head = mFirst;
	if (!head)
		return 0;
	SListNode *found = 0;
	SListNode *cursor = head;
	do {
		if (cursor->mItem == item) {
			found = cursor;
			break;
		}
		cursor = cursor->mNext;
	} while (cursor != head);
	if (!found)
		return 0;
	SListNode *nxt = found->mNext;
	if (nxt == head)
		return 0;
	return nxt->mItem;
}

/* Moves item's node to the front (mFirst). Real ground truth (.text+0x08bd1060) reaches
 * an equivalent state via a convoluted "provisionally advance mFirst, unsplice, reinsert,
 * then set mFirst = node again" path when item is already first; net effect is a pure
 * no-op, reproduced here directly as an early return. */
void CAbstList::bringfront(CObject *item)
{
	SListNode *node = findnode(item);
	if (!node || node == mFirst)
		return;

	node->mPrev->mNext = node->mNext;
	node->mNext->mPrev = node->mPrev;

	SListNode *head = mFirst;
	if (!head) {
		node->mNext = node;
		node->mPrev = node;
		mFirst = node;
		return;
	}

	SListNode *tail = head->mPrev;
	node->mPrev = tail;
	tail->mNext = node;
	node->mNext = head;
	head->mPrev = node;
	mFirst = node;
}

/* Moves item's node to the back (mFirst->mPrev), WITHOUT reassigning mFirst -- unlike
 * bringfront()'s already-first case, sendback()'s already-first case is NOT a no-op:
 * .text+0x08bd10f0's found==mFirst path provisionally advances mFirst to the old front's
 * successor (or 0 if it was the only node) BEFORE the unsplice/reinsert, and that update
 * is never undone -- so sending the current front item to the back really does promote
 * its old successor to the new front. */
void CAbstList::sendback(CObject *item)
{
	SListNode *node = findnode(item);
	if (!node)
		return;

	if (node == mFirst)
		mFirst = (node->mNext == node) ? 0 : node->mNext;

	node->mPrev->mNext = node->mNext;
	node->mNext->mPrev = node->mPrev;

	SListNode *head = mFirst;
	if (!head) {
		/* node was the only element. */
		node->mNext = node;
		node->mPrev = node;
		mFirst = node;
		return;
	}

	SListNode *tail = head->mPrev;
	node->mPrev = tail;
	tail->mNext = node;
	node->mNext = head;
	head->mPrev = node;
}

/* Swaps `item`'s node with its predecessor. Real ground truth (.text+0x08bd1170) is a
 * no-op both when `item` isn't found (defensive here -- see prev()/next()'s comment on
 * the same not-found UB pattern) and when `item` is already the first element. */
void CAbstList::moveup(CObject *item)
{
	SListNode *node = findnode(item);
	if (!node || node == mFirst)
		return;

	SListNode *prevNode = node->mPrev;

	/* Unsplice node. */
	prevNode->mNext = node->mNext;
	node->mNext->mPrev = prevNode;

	/* Reinsert node immediately before prevNode. */
	SListNode *beforePrev = prevNode->mPrev;
	node->mNext = prevNode;
	node->mPrev = beforePrev;
	prevNode->mPrev = node;
	beforePrev->mNext = node;

	if (prevNode == mFirst)
		mFirst = node;
}

/* Mirror of moveup() -- swaps `item`'s node with its successor. No-op if not found or
 * already last. If `item` was mFirst, mFirst becomes its (former) successor. */
void CAbstList::movedown(CObject *item)
{
	SListNode *node = findnode(item);
	if (!node)
		return;

	SListNode *nextNode = node->mNext;
	if (nextNode == mFirst)
		return; /* already last */

	if (node == mFirst)
		mFirst = nextNode;

	SListNode *prevNode = node->mPrev;
	SListNode *afterNext = nextNode->mNext;

	nextNode->mPrev = prevNode;
	prevNode->mNext = nextNode;

	node->mPrev = nextNode;
	node->mNext = afterNext;
	afterNext->mPrev = node;
	nextNode->mNext = node;
}

CObject *CAbstList::firstitem()
{
	return mFirst ? mFirst->mItem : 0;
}

CObject *CAbstList::lastitem()
{
	return mFirst ? mFirst->mPrev->mItem : 0;
}

CObject *CAbstList::nthitem(long index)
{
	SListNode *node = findnode(index);
	return node ? node->mItem : 0;
}

long CAbstList::findindex(CObject *item)
{
	SListNode *node = mFirst;
	long index = 0;
	if (!node)
		return 0;
	if (node->mItem == item)
		return index;
	node = node->mNext;
	while (node != mFirst) {
		++index;
		if (node->mItem == item)
			return index;
		node = node->mNext;
	}
	return 0;
}

bool CAbstList::includes(CObject *item)
{
	SListNode *node = mFirst;
	if (!node)
		return false;
	do {
		if (node->mItem == item)
			return true;
		node = node->mNext;
	} while (node != mFirst);
	return false;
}

/* ---- CUsrList ---- */

CUsrList::CUsrList()
{
}

CUsrList::~CUsrList()
{
	removeallnode();
}

SListNode *CUsrList::makenode_sub(CObject *item)
{
	SListNode *node = new (std::nothrow) SListNode;
	if (node) {
		node->mPrev = 0;
		node->mNext = 0;
		node->mItem = item;
	}
	return node;
}

void CUsrList::deletenode_sub(SListNode *node)
{
	delete node;
}

/* ---- CList ---- */

CList::CList()
{
}

CList::~CList()
{
	removeallnode();
}

SListNode *CList::makenode_sub(CObject *item)
{
	SListNode *node = new (std::nothrow) SListNode;
	if (node) {
		node->mPrev = 0;
		node->mNext = 0;
		node->mItem = item;
	}
	return node;
}

void CList::deletenode_sub(SListNode *node)
{
	delete node;
}

/* ---- CStaticList ---- */

CStaticList::CStaticList(int capacity)
	: mPool(0), mFreeHead(0), mFreeTail(0), mCapacity(capacity)
{
	mPool = new SListNode[capacity];
	mPool[0].mPrev = 0;

	SListNode *node = mPool;
	for (long i = 0; i + 1 < capacity; ++i) {
		node->mItem = 0;
		node->mNext = &mPool[i + 1];
		node = node->mNext;
	}
	node->mItem = 0;
	node->mNext = 0;

	mFreeHead = mPool;
	mFreeTail = node;
}

CStaticList::~CStaticList()
{
	removeallnode();
	if (mPool) {
		delete[] mPool;
		mPool = 0;
	}
}

SListNode *CStaticList::makenode_sub(CObject *item)
{
	if (mCapacity <= mCount)
		return 0;

	SListNode *node = mFreeHead;
	mFreeHead = node->mNext;
	node->mPrev = 0;
	node->mNext = 0;
	node->mItem = item;
	return node;
}

void CStaticList::deletenode_sub(SListNode *node)
{
	if (!mFreeHead) {
		mFreeHead = node;
		mFreeTail = node;
	} else {
		mFreeTail->mNext = node;
		mFreeTail = node;
	}
	node->mNext = 0;
	mFreeTail->mItem = 0;
}

/* ---- CListIter ---- */

void CListIter::inititer(const CAbstList *list)
{
	mList = list;
	mAtTail = false;
	mAtTop = false;
}

CListIter::CListIter() : mList(0), mCurNode(0), mAtTail(false), mAtTop(false)
{
}

CListIter::CListIter(const CAbstList &list, EIteratorPos pos)
{
	init(list, pos);
}

CListIter::CListIter(const CAbstList &list, CObject *atItem)
{
	init(list, atItem);
}

CListIter::CListIter(const CAbstList &list, long index)
{
	init(list, index);
}

CListIter::~CListIter()
{
}

void CListIter::init(const CAbstList &list, EIteratorPos pos)
{
	inititer(&list);
	if (!list.mFirst) {
		mCurNode = 0;
		return;
	}
	mCurNode = (pos == kIterFront) ? list.mFirst : list.mFirst->mPrev;
}

void CListIter::init(const CAbstList &list, CObject *atItem)
{
	inititer(&list);
	mCurNode = 0;
	if (!list.mFirst)
		return;
	SListNode *node = list.mFirst;
	do {
		if (node->mItem == atItem) {
			mCurNode = node;
			return;
		}
		node = node->mNext;
	} while (node != list.mFirst);
}

void CListIter::init(const CAbstList &list, long index)
{
	inititer(&list);
	mCurNode = 0;
	SListNode *node = list.mFirst;
	if (!node)
		return;
	if (index == 0) {
		mCurNode = node;
		return;
	}
	for (long i = 0; i < index; ++i) {
		node = node->mNext;
		if (node == list.mFirst)
			return; /* out of range -- stays NULL */
	}
	mCurNode = node;
}

void CListIter::totop()
{
	mAtTail = false;
	mAtTop = false;
	mCurNode = mList->mFirst;
}

void CListIter::totail()
{
	mAtTail = false;
	mAtTop = false;
	mCurNode = mList->mFirst ? mList->mFirst->mPrev : 0;
}

CObject *CListIter::operator()() const
{
	if (!mCurNode)
		return 0;
	if (mAtTail || mAtTop)
		return 0;
	return mCurNode->mItem;
}

void CListIter::operator++()
{
	if (!mCurNode)
		return;
	if (mAtTail)
		return;
	if (mAtTop) {
		mCurNode = mCurNode->mNext;
		mAtTop = false;
	}
	mCurNode = mCurNode->mNext;
	if (mCurNode == mList->mFirst)
		mAtTail = true;
}

void CListIter::operator--()
{
	if (!mCurNode)
		return;
	if (mAtTop)
		return;
	if (mAtTail) {
		mCurNode = mCurNode->mPrev;
		mAtTail = false;
	}
	mCurNode = mCurNode->mPrev;
	if (mCurNode == mList->mFirst->mPrev)
		mAtTop = true;
}
