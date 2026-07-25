/*
 * editable.h  -  CEditable, the thin per-sub-object "descriptor registration" mixin
 * `CAlphaKeybIfcTask` (alpha_keyb_ifc_task.h) embeds at its own +0x7c -- reconstructed
 * as part of the same Stage 6 breadth-sweep batch that closed out CEditor's own
 * Setup() fan-out (2026-07-25). Not itself CEditor-family: this is a genuinely
 * separate, top-level `CEditable` class (confirmed via symbols.csv's own unqualified
 * `_ZN9CEditableC1E...` / `_ZTI9CEditable` mangling, no enclosing-class prefix) used
 * wherever some other CTask-derived class wants to register its own SDescriptor table
 * against an existing CEditServer's CDataHandler without itself being a full
 * CModule+CEditServer pair.
 *
 * Real layout (0x04 bytes total -- a single pointer, no vtable of its own; every read
 * site treats `this` as a bare `CEditServer**`, confirmed from CEditable@0806e310.c /
 * AddDescriptorsMap@0806e320.c):
 *   +0x00  mEditServer   the ctor's own CEditServer* argument, stored verbatim
 *
 * `CEditable(CEditServer*)` (.text+0x0806e310, 11 bytes) is Tier A -- trivial pointer
 * store, no vtable install (CEditable is not itself polymorphic; it only ever appears
 * as a plain embedded sub-object, e.g. `CAlphaKeybIfcTask::mEditable` at +0x7c).
 *
 * `AddDescriptorsMap(CObjectBase*, SDescriptor*, bool)` (.text+0x0806e320, 196 bytes)
 * is Tier A. Real body: scans forward through `descriptors` counting entries until one
 * whose `group` field (SDescriptor's own +0x34 byte, edit_server.h) equals the 0xff
 * sentinel -- the real disassembly is an 8-way Duff's-device-unrolled version of this
 * scan (checking `descriptors[0..7].group` per iteration before advancing by 8 whole
 * SDescriptor elements, i.e. exactly `sizeof(SDescriptor)*8 = 0x1c0` bytes -- confirmed
 * against the real byte offsets 0x34/0x6c/0xa4/0xdc/0x114/0x14c/0x184/0x1bc, each
 * exactly one more `sizeof(SDescriptor)=0x38` apart), collapsed here to the equivalent
 * plain `while` loop, same license as every other unrolled scan in this project. Once
 * the sentinel-terminated count is known, forwards to
 * `mEditServer`'s own embedded `CDataHandler` (located at `mEditServer + 4`, matching
 * edit_server.h's own "+0x04..0x40024 mData" layout comment -- CDataHandler is never
 * separately allocated) via the already-real `CDataHandler::AddDescriptors()`
 * (edit_server.h).
 *
 * Only real, confirmed caller in the ground-truth binary is
 * `CAlphaKeybIfcTask::CAlphaKeybIfcTask(CEditor const&)` -- NOT wired into this
 * reconstruction's own call graph yet (see alpha_keyb_ifc_task.h's own header comment
 * for why: `CEditor::Setup()`'s "ALPHAKEYBOARD=Yes" branch that would construct it is
 * left deliberately un-built by the concurrent CEditor batch, and this pass
 * intentionally does not touch editor.cpp/editor.h to avoid colliding with that
 * concurrent work). `AddDescriptorsMap()` itself has no other callers in the export.
 */

#ifndef EDITABLE_H
#define EDITABLE_H

class CEditServer;
class CObjectBase;
struct SDescriptor;

class CEditable {
public:
	/* .text+0x0806e310, 11 bytes. Trivial pointer store, see header comment. */
	explicit CEditable(CEditServer *editServer);

	/* .text+0x0806e320, 196 bytes. See header comment. `descriptors` must be a
	 * sentinel-terminated array (a row with `.group == 0xff` marks the end,
	 * never itself passed to CDataHandler::AddDescriptors).
	 */
	void AddDescriptorsMap(CObjectBase *owner, SDescriptor *descriptors,
	                       bool alreadyRegistered);

private:
	CEditServer *mEditServer;
};

#endif /* EDITABLE_H */
