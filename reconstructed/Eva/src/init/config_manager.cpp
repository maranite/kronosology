/*
 * config_manager.cpp  -  see include/config_manager.h.
 *
 * AssignEditServerIDs() transcribed from AssignEditServerIDs@080562f0.c (334 bytes).
 * The other 9 CConfigManager methods (CKernel::InitUserLayer()'s own bring-up
 * sequence) are Tier-B link-stubs, not individually looked up in the decompile export
 * -- genuinely out of scope for this pass (each is presumably its own substantial
 * per-subsystem bring-up routine, matching the scale of CModuleManager's own methods).
 */

#include "config_manager.h"

/* CEditApiInstance::AssignScope()/EditApiInstance are Tier-B: real signature/global
 * confirmed from the decompile, but with this pass's own zero-initialized
 * sm_ptEditServerInfo placeholder (config_info.cpp), AssignEditServerIDs()'s loop
 * body that would call this is real but unreachable (see below) -- so an empty body
 * here is not a behavioral gap for anything this pass's data can exercise.
 */
class CEditApiInstance {
public:
	void AssignScope(const char *name, unsigned char scope);
};

void CEditApiInstance::AssignScope(const char * /*name*/, unsigned char /*scope*/)
{
	/* Tier-B link-stub. */
}

/* Real global, shared with mains.cpp's own MMainEditMan() (the same EditApiInstance
 * the real binary registers via CSysApiInstance::RegisterApi(), Api's vtable slot
 * +0xa4) -- defined once there, not redefined here. CORRECTED 2026-07-23: this is a
 * real ~1028-byte object (byte buffer, not a pointer) -- see mains.cpp's own
 * declaration comment; array-to-pointer decay keeps every cast below unchanged.
 */
extern unsigned char EditApiInstance[];

void CConfigManager::AssignEditServerIDs()
{
	if (sm_ptCreateInfo == 0)
		return;

	/* Real per-entry table: 7 packed {name, scope} pairs per 0x10-dword row,
	 * terminated by a null name. With this pass's own zero-initialized
	 * sm_ptEditServerInfo placeholder (config_info.cpp), the first name is already
	 * null, so this loop body is real but never executes -- transcribed faithfully
	 * anyway (same license as USTGAPILCDControl::LoadStoredSettings()'s dead
	 * `local_10` read).
	 */
	unsigned *row = (unsigned *)sm_ptEditServerInfo;
	char *name = (char *)row[0];
	while (name != 0) {
		((CEditApiInstance *)EditApiInstance)->AssignScope(name, (unsigned char)row[1]);
		if ((char *)row[2] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[2], (unsigned char)row[3]);
		if ((char *)row[4] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[4], (unsigned char)row[5]);
		if ((char *)row[6] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[6], (unsigned char)row[7]);
		if ((char *)row[8] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[8], (unsigned char)row[9]);
		if ((char *)row[10] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[10], (unsigned char)row[11]);
		if ((char *)row[12] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[12], (unsigned char)row[13]);
		if ((char *)row[14] == 0) return;
		unsigned char scope15 = (unsigned char)row[15];
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[14], scope15);
		row += 16;
		name = (char *)row[0];
	}
}

void CConfigManager::ConfigureSeqTimer() {}
void CConfigManager::CreateResourceFamilies() {}
void CConfigManager::CreateUserModules() {}
void CConfigManager::CreateFMDrivers() {}
void CConfigManager::SetupRouting() {}
void CConfigManager::LinkRTRouterTracks() {}
void CConfigManager::SetupSysex() {}
void CConfigManager::MakeConnections() {}
void CConfigManager::RegisterChunkServer() {}
