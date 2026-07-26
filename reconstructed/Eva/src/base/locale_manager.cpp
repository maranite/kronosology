/*
 * locale_manager.cpp  -  see include/locale_manager.h.
 */

#include "locale_manager.h"

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

void CLocaleManager::AddKeyboardLayout(const CKeyboardLayout * /*layout*/)
{
	/* Tier-B stub -- see header comment. */
}

void *CLocaleManager::GetKeyboardLayout(unsigned int /*type*/)
{
	/* Tier-B stub -- see header comment. */
	return 0;
}
