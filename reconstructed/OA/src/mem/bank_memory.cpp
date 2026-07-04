// SPDX-License-Identifier: GPL-2.0
/*
 * bank_memory.cpp  -  see include/oa_bank_memory.h.
 * Ground-truthed offsets: .text+0x232b0 / 0x232d0 / 0x232e0.
 */

#include "oa_bank_memory.h"

unsigned char *CSTGBankMemory::sOurMemoryBase;
unsigned long  CSTGBankMemory::sCurrentAllocationOffset;
unsigned long  CSTGBankMemory::sTotalMemoryAvailable;

void CSTGBankMemory::Initialize(unsigned char *base, unsigned long size)
{
	sCurrentAllocationOffset = 0;
	sOurMemoryBase = (unsigned char *)(((unsigned long)base + 0xfUL) & ~0xfUL);
	sTotalMemoryAvailable = size & ~0xfUL;
}

void CSTGBankMemory::SetTotalBytesToManage(unsigned long size)
{
	sTotalMemoryAvailable = size;
}

unsigned char *CSTGBankMemory::AllocAligned(unsigned int size, unsigned int alignment)
{
	unsigned long pos = (unsigned long)sOurMemoryBase + sCurrentAllocationOffset;
	unsigned long aligned = (pos + (alignment - 1)) & ~(unsigned long)(alignment - 1);

	sCurrentAllocationOffset = (aligned - (unsigned long)sOurMemoryBase) + size;

	return (unsigned char *)aligned;
}
