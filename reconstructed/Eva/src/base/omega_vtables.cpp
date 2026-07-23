/*
 * omega_vtables.cpp  -  see include/omega_vtables.h.
 */

#include "omega_vtables.h"

extern "C" void EvaVTableStub()
{
	/* Real cdecl no-op. Safe as a stand-in for any of these slots' real virtual
	 * method regardless of the real method's own arg count -- cdecl callees never
	 * pop caller-pushed stack args, so a mismatched (zero) parameter count here
	 * cannot corrupt the caller's stack.
	 */
}

extern "C" {
void *PTR__CHostInterfaceBase_08e80b68[22] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CHostInterface_08e80b08[22] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__COmegaPtrArray_08e80be0[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e80bf8[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e80c10[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e80c40[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TVector_08e80c58[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CDummyMsgInput_08e80c80[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CNamedObjectBase_08e81378[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CTracer_08e81468[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CLevelManagerArray_08e80c28[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CLevelManager_08e80e50[20] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e80ea8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e80bc8[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CSysApiInstance_08e81008[94] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e811a8[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e811c0[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
}
