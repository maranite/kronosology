/*
 * locale_manager.cpp  -  see include/locale_manager.h.
 */

#include "locale_manager.h"

#include <cstring>
#include <new>

CLocaleManager *CLocaleManager::sm_poInstance = 0;

CLocaleManager::CLocaleManager()
{
	/* Real: *(void***)this = &PTR__TVector_08e81c48; begin=end=cap=0. The real
	 * vtable identity is never dispatched through by either stub method below,
	 * so it is left null here rather than adding an unused omega_vtables entry.
	 */
	mVtbl = 0;
	mBegin = 0;
	mEnd = 0;
	mCap = 0;
}

CLocaleManager *CLocaleManager::GetInstance()
{
	if (sm_poInstance != 0)
		return sm_poInstance;

	void *raw = ::operator new(0x10);
	sm_poInstance = new (raw) CLocaleManager();
	return sm_poInstance;
}

void CLocaleManager::AddKeyboardLayout(const CKeyboardLayout *layout)
{
	/* Real: see header comment for the full derivation. */
	if (layout == 0)
		return;

	unsigned char **begin = reinterpret_cast<unsigned char **>(mBegin);
	unsigned char **end   = reinterpret_cast<unsigned char **>(mEnd);
	unsigned char **cap   = reinterpret_cast<unsigned char **>(mCap);

	unsigned int count    = (unsigned int)(end - begin);
	unsigned int capElems = (unsigned int)(cap - begin);

	if (count + 1 > capElems) {
		unsigned int needed = count + 1;
		unsigned int newCapElems = 32;
		if (needed > 32) {
			do {
				newCapElems *= 2;
			} while (needed > newCapElems);
		}

		void *newBlock = ::operator new(newCapElems * sizeof(void *));
		if (end != begin)
			memcpy(newBlock, begin, count * sizeof(void *));
		::operator delete(begin);

		begin = reinterpret_cast<unsigned char **>(newBlock);
		end   = begin + count;
		cap   = begin + newCapElems;
		mBegin = begin;
		mCap   = cap;
	}

	*end = reinterpret_cast<unsigned char *>(const_cast<CKeyboardLayout *>(layout));
	end++;
	mEnd = end;
}

void *CLocaleManager::GetKeyboardLayout(unsigned int type)
{
	/* Real: see header comment for the full derivation. */
	unsigned char **begin = reinterpret_cast<unsigned char **>(mBegin);
	unsigned char **end   = reinterpret_cast<unsigned char **>(mEnd);

	unsigned short key = (unsigned short)type;
	for (unsigned char **p = begin; p != end; p++) {
		unsigned char *elem = *p;
		if (*reinterpret_cast<const unsigned short *>(elem) == key)
			return elem;
	}
	return 0;
}
