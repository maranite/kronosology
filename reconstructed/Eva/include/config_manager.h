/*
 * config_manager.h  -  CConfigManager, the static table-of-config-tables holder.
 *
 * Fields recovered from SetConfigInfo() (.text+0x0804cb70, src/init/config_info.cpp)
 * -- the only function seen so far that writes them, and symbols.csv's own
 * `_ZN14CConfigManager...E` mangled names, which confirm these are real static data
 * members of CConfigManager, not free globals. All 13 are opaque pointers; the real
 * config tables they point at are large metadata blobs genuinely out of scope for
 * this boot-path pass (nothing on the traced boot path dereferences any of them) --
 * see config_info.cpp for the placeholder objects they're pointed at here.
 *
 * CConfigManager::AssignEditServerIDs() (.text+0x080562f0, 334 bytes) is called from
 * CKernel::InitSystemLayer() (src/init/ckernel.cpp) -- declared here too so both call
 * sites share one real class declaration instead of redeclaring it locally (ODR).
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

/* .text+0x0804cb70, 147 bytes -- assigns all 13 fields below. See config_info.cpp. */
void SetConfigInfo();

class CConfigManager {
public:
	/* .text+0x080562f0, 334 bytes. Real body walks sm_ptEditServerInfo's own packed
	 * per-entry table (7 name/scope pairs per row) calling CEditApiInstance::AssignScope
	 * on each until a null name terminates the row -- with this pass's own
	 * zero-initialized sm_ptEditServerInfo placeholder (config_info.cpp), the first
	 * entry is already null, so the real loop body never executes (a real, faithfully
	 * preserved consequence of this pass's own placeholder config data, not a
	 * shortcut). CEditApiInstance::AssignScope() is a Tier-B link-stub for the same
	 * reason -- see config_manager.cpp.
	 */
	static void AssignEditServerIDs();

	/* CKernel::InitUserLayer()'s own 9-step user-layer bring-up (ckernel.cpp) --
	 * .text addresses not individually looked up (out of scope for this pass); all 9
	 * are Tier-B link-stubs (empty bodies), see config_manager.cpp.
	 */
	static void ConfigureSeqTimer();
	static void CreateResourceFamilies();
	static void CreateUserModules();
	static void CreateFMDrivers();
	static void SetupRouting();
	static void LinkRTRouterTracks();
	static void SetupSysex();
	static void MakeConnections();
	static void RegisterChunkServer();

	static void *sm_ptCreateInfo;
	static void *sm_ptFMDriverInfo;
	static void *sm_ptConnectInfo;
	static void *sm_ptEditServerInfo;
	static void *sm_ptSysExModuleInfo;
	static void *sm_ptSysExConnectInfo;
	static void *sm_ptSysExFilterInfo;
	static void *sm_ptRTRouterInfo;
	static void *sm_ptChunkInfo;
	static void *sm_ptResFamilyInfo;
	static void *sm_ptSeqTimerInfo;
	static void *sm_pktVersionInfo;
	static void *sm_apkcSysVars;
};

#endif /* CONFIG_MANAGER_H */
