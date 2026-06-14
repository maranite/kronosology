// SPDX-License-Identifier: GPL-2.0
/*
 * omapnks4_internal.h  -  module-private declarations: the STG/RTAI framework
 * layer, kernel imports, singletons and the cross-file helpers.
 *
 * Symbols prefixed stg_ / rtwrap_ are the shared "STG" real-time abstraction layer
 * (a thin veneer over RTAI + the Linux kernel) that every Korg STG module links.
 * They are imported here, not redefined.  CSTGThread likewise comes from that layer.
 */

#ifndef OMAPNKS4_INTERNAL_H
#define OMAPNKS4_INTERNAL_H

#include "omapnks4.h"

/* 1e6 ns/ms numerator; flNanosPerCycle = NANOS_PER_MS / cpu_khz  (DAT_0000af4c). */
#define OMAPNKS4_NANOS_PER_MS 1000000.0f

/* TSC read (the binary inlines RDTSC). */
static inline unsigned long long omapnks4_rdtsc(void)
{
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)hi << 32) | lo;
}

extern "C" {

/* ---- STG kernel/heap/timing veneer ------------------------------------- */
unsigned int stg_get_cpu_khz(void);
void        *stg_kmalloc(unsigned int size);
void         stg_kfree(void *p);
unsigned int stg_ksize(void *p);
void         stg_msleep(unsigned int ms);
int          stg_is_linux_context(void);
unsigned int stg_hweight32(unsigned int x);
unsigned int stg_num_online_cpus(void);

/* ---- RTAI veneer (rtwrap_*) -------------------------------------------- */
unsigned long long rtwrap_nano2count(long long nanos);
void rtwrap_sleep(unsigned long long count);
void rtwrap_pend_linux_srq(int srq);
/* (full pthread/mutex/cond/irq wrapper set lives in the STG layer) */

/* ---- USB / kernel imports used across files ---------------------------- */
int  printk(const char *fmt, ...);

}  /* extern "C" */

/* ---- C++ runtime (provided by the STG cpp-support shim) ---------------- */
void *operator new(unsigned int size);
void  operator delete(void *p);

/* ---- CSTGThread: STG real-time thread base ----------------------------- */
typedef void *(*stg_thread_fn)(void *arg);
struct CSTGThread {
	int CreateRealTimeWithCPUAffinity(stg_thread_fn fn, void *arg,
					  int priority, int stack, void *cpumask);
	void Delete(void);
	void Wait(void);
	static int GetMaxRealTimePriority(void);
};

/* ========================================================================= *
 *  OmapNKS4 module globals / cross-file helpers
 * ========================================================================= */

/* the singleton panel-driver and video-API instances (mangled COmapNKS4*::sInstance) */
extern struct COmapNKS4Driver    COmapNKS4Driver_sInstance;
extern struct COmapNKS4VideoAPI  g_video;	/* == COmapNKS4VideoAPI::sInstance */

/* host->panel output path (submit.c).  A "command word" is opcode<<24 | reg<<16 |
 * dataHi<<8 | dataLo and is passed to the submit/wait helpers in EAX. */
int  SubmitNKS4CommandMultipleWriteNONBLOCKING(unsigned int *cmds, unsigned int nInts);
int  SubmitNKS4CommandWrite(unsigned int cmd);
int  SubmitOmapNKS4CmdBulkWrite(unsigned char command, unsigned char *data, unsigned int nBytes);
int  SubmitOmapNKS4BulkWrite(unsigned int *data, unsigned int nBytes);
int  WaitForNKS4CommandWrite(unsigned int cmd);	/* send cmd, block for write-complete */
int  WaitForNKS4ReadEvent(unsigned int *resp);	/* block for one response word         */
int  OmapNKS4WriteQueueNotFull(void);

/* event delivery (procfs.c) */
void OmapNKS4ProcAddEvent(unsigned char ev);
void SendNKS4EventToLinuxReader(unsigned int cmd);

/* signals (submit.c / threads) */
void SignalAtmelReadComplete(void);
void SignalVideoMessageProcessor(void);
void SignalShutdownSSD(void);
void SetShutdownDelay(int delay);
void WaitOnAtmelRead(void);
int  SubmitOmapNKS4VideoWrite(unsigned int *data, unsigned int nBytes);

#endif /* OMAPNKS4_INTERNAL_H */
