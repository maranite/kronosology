/*
 * omega_interface.h  -  COmegaInterface, Eva's "app kernel" bring-up class (Stage 1).
 *
 * main() constructs a global COmegaInterface (real symbol "Omega") and calls Init()
 * on it; Init() does not return until app shutdown -- see README.md's "Boot path"
 * section for the full call sequence this is transcribed from. CKernel itself
 * (Stage 3) is forward-declared only; its real layout/methods are not yet
 * reconstructed.
 */

#ifndef OMEGA_INTERFACE_H
#define OMEGA_INTERFACE_H

class CKernel;

/* Real typedef name recovered from Ghidra's own prototype for Init()'s parameter. */
typedef int (*_func_int_char_ptr)(const char *);

class COmegaInterface {
public:
	COmegaInterface();
	~COmegaInterface();

	static void *GetSysApi();
	static void ExitRequested();

	/* The real bring-up. Idempotent (guarded on mCreated != 0). Spawns 6
	 * OmegaSchedulingThread workers, then SetConfigInfo() / CKernel::InitSystemLayer()
	 * / Mains() / one OmegaInitThread, then runs OmegaTimingThread(NULL) directly on
	 * the calling thread -- which is the real reason this call blocks until shutdown.
	 * Stage 3 (CKernel, SetConfigInfo, Mains, the OmegaXxxThread bodies) is what this
	 * function actually calls into and is not yet reconstructed -- see README.md.
	 */
	void Init(_func_int_char_ptr sendCallback);

	/* Real bodies: spinlock-protected increment/decrement of a "timing disable"
	 * refcount; both unconditionally return -1. Faithfully preserved, not "fixed"
	 * into a meaningful return value.
	 */
	static int Run();
	static int Stop();

	/* Sets the real shutdown flag (s_bRunning = 0); same body as the destructor. */
	void Close();

private:
	int mCreated;                     /* this+0x00 -- Init()'s idempotency guard */
	int mUnused04;                    /* this+0x04 -- zeroed by ctor, not yet observed written elsewhere */
	CKernel *mKernel;                 /* this+0x08 */
	_func_int_char_ptr mSendCallback; /* this+0x0c -- set by Init() */
	unsigned char mUnknown10[0x0c];   /* this+0x10..0x1b -- not yet observed touched anywhere */
	int mUnused1c;                    /* this+0x1c -- zeroed by ctor */
	int mUnused20;                    /* this+0x20 -- zeroed by ctor */
};

/* Real global instance, referenced from main() by the confirmed symbol name "Omega". */
extern COmegaInterface Omega;

#endif /* OMEGA_INTERFACE_H */
