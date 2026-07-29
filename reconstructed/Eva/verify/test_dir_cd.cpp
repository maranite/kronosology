// SPDX-License-Identifier: GPL-2.0
/*
 * test_dir_cd.cpp  -  host-side known-answer test for the 10 reconstructed
 * CDirCD methods (src/init/dir_cd.cpp). See include/dir_cd.h for full
 * ground-truth provenance and the list of deferred methods.
 *
 * CDirCD's real ctor/dtor are NOT reconstructed this pass (see header
 * comment) -- every test here uses a raw, manually-populated byte buffer
 * cast to `CDirCD*` instead of a real constructed instance, same
 * ctor-avoidance convention already established by CFileKscList's own KAT.
 */

#include <cstdio>
#include <cstring>

#include "dir_cd.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-55s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%lx)\n", want);
}

int main()
{
	printf("CDirCD known-answer test\n");
	printf("=========================\n");

	static unsigned char media[0x100];
	static unsigned char buf[0x1000];

	printf("[1] GetCurrEntry -- returns this+0xd8\n");
	{
		memset(buf, 0, sizeof(buf));
		CDirCD *d = (CDirCD *)buf;
		check_eq("GetCurrEntry() == this+0xd8",
		         (unsigned long)((unsigned char *)(void *)d->GetCurrEntry() - buf), 0xd8);
	}

	printf("[2] GetRootHandle -- dispatches on media[+0xa0] mode 0..4\n");
	{
		memset(buf, 0, sizeof(buf));
		memset(media, 0, sizeof(media));
		*(unsigned char **)(buf + 0x28) = media;
		CDirCD *d = (CDirCD *)buf;

		*(unsigned long *)(media + 0xa0) = 0;
		check_eq("mode 0 -> 0", d->GetRootHandle(), 0);

		*(unsigned long *)(media + 0xa0) = 1;
		*(unsigned long *)(buf + 0xc9c) = 0x1234;
		check_eq("mode 1 -> this+0xc9c", d->GetRootHandle(), 0x1234);

		*(unsigned long *)(media + 0xa0) = 2;
		buf[0x511] = 5;
		*(unsigned long *)(buf + 0x518) = 0x1000;
		check_eq("mode 2 -> byte[0x511] + dword[0x518]", d->GetRootHandle(), 0x1005);

		*(unsigned long *)(media + 0xa0) = 3;
		*(unsigned long *)(buf + 0xca0) = 0x5678;
		check_eq("mode 3 -> this+0xca0", d->GetRootHandle(), 0x5678);

		*(unsigned long *)(media + 0xa0) = 9;
		check_eq("mode 9 (default) -> 0", d->GetRootHandle(), 0);
	}

	printf("[3] GetClusterSizeInSect -- always 1\n");
	{
		CDirCD *d = (CDirCD *)buf;
		check_eq("always 1", d->GetClusterSizeInSect(), 1);
	}

	printf("[4] GetMaxDirEntrySize -- 0x40 if mode==4 else 0\n");
	{
		memset(media, 0, sizeof(media));
		CDirCD *d = (CDirCD *)buf;
		*(unsigned long *)(media + 0xa0) = 4;
		check_eq("mode 4 -> 0x40", d->GetMaxDirEntrySize(), 0x40);
		*(unsigned long *)(media + 0xa0) = 3;
		check_eq("mode 3 -> 0", d->GetMaxDirEntrySize(), 0);
	}

	printf("[5] GetNumAkaiPartition -- (end-begin)/24\n");
	{
		static CDirCD::SAkaiPartition parts[5];
		memset(buf, 0, sizeof(buf));
		CDirCD *d = (CDirCD *)buf;
		*(const unsigned char **)(buf + 0xca8) = (const unsigned char *)&parts[0];
		*(const unsigned char **)(buf + 0xcac) = (const unsigned char *)&parts[5];
		check_eq("5 elements", d->GetNumAkaiPartition(), 5);
		check_eq("sizeof(SAkaiPartition) == 24", sizeof(CDirCD::SAkaiPartition), 24);
	}

	printf("[6] SetError -- maps notify code to this+0x50\n");
	{
		memset(buf, 0, sizeof(buf));
		CDirCD *d = (CDirCD *)buf;
		d->SetError(0);
		check_eq("notify 0 -> 0", *(unsigned int *)(buf + 0x50), 0);
		d->SetError(2);
		check_eq("notify 2 -> 8", *(unsigned int *)(buf + 0x50), 8);
		d->SetError(1);
		check_eq("notify 1 -> 0xa", *(unsigned int *)(buf + 0x50), 0xa);
		d->SetError(5);
		check_eq("notify 5 -> 0xc", *(unsigned int *)(buf + 0x50), 0xc);
		d->SetError(7);
		check_eq("notify other -> 0xd", *(unsigned int *)(buf + 0x50), 0xd);
	}

	printf("[7] ResetBufferedEntries -- sets this+0xcb8=-1, this+0xcbc=0\n");
	{
		memset(buf, 0xaa, sizeof(buf));
		CDirCD *d = (CDirCD *)buf;
		d->ResetBufferedEntries();
		check_eq("this+0xcb8 == 0xffffffff", *(unsigned long *)(buf + 0xcb8), 0xffffffffUL);
		check_eq("this+0xcbc == 0", *(unsigned long *)(buf + 0xcbc), 0);
	}

	printf("[8] GetTotalSectors -- mode 4 raw override, else sum(value*75)\n");
	{
		memset(buf, 0, sizeof(buf));
		memset(media, 0, sizeof(media));
		*(unsigned char **)(buf + 0x28) = media;
		CDirCD *d = (CDirCD *)buf;

		*(unsigned long *)(media + 0xa0) = 4;
		*(unsigned long *)(buf + 0x144) = 0x9999;
		check_eq("mode 4 -> raw this+0x144", d->GetTotalSectors(), 0x9999);

		*(unsigned long *)(media + 0xa0) = 1;
		buf[0x14b] = 0;
		check_eq("count 0 -> 0", d->GetTotalSectors(), 0);

		buf[0x14b] = 2;
		buf[0x14c + 0 * 8 + 0] = 10; /* entry0 low byte -> value=10 */
		buf[0x14c + 0 * 8 + 3] = 0;
		buf[0x14c + 1 * 8 + 0] = 20; /* entry1 low byte -> value=20 */
		buf[0x14c + 1 * 8 + 3] = 0;
		check_eq("2 entries, (10+20)*75", d->GetTotalSectors(), (10 + 20) * 75UL);

		buf[0x14b] = 1;
		buf[0x14c + 0] = 0;
		buf[0x14c + 3] = 1; /* high byte -> value = 0x0100 = 256 */
		check_eq("high-byte assembly: value=256", d->GetTotalSectors(), 256UL * 75);
	}

	printf("[9] FindPartition -- linear scan over [0xca8,0xcac), 24-byte stride\n");
	{
		static CDirCD::SAkaiPartition parts[3];
		parts[0].id = 100;
		parts[1].id = 200;
		parts[2].id = 300;
		memset(buf, 0, sizeof(buf));
		CDirCD *d = (CDirCD *)buf;
		*(const unsigned char **)(buf + 0xca8) = (const unsigned char *)&parts[0];
		*(const unsigned char **)(buf + 0xcac) = (const unsigned char *)&parts[3];

		const CDirCD::SAkaiPartition *out = 0;
		bool found = d->FindPartition(200, out);
		check_eq("found id=200", found, true);
		check_eq("out points at parts[1]", (unsigned long)(out - parts), 1);

		found = d->FindPartition(999, out);
		check_eq("not found -> false", found, false);
		check_eq("out left at last element (parts[2])", (unsigned long)(out - parts), 2);

		const unsigned char *empty = (const unsigned char *)&parts[0];
		*(const unsigned char **)(buf + 0xca8) = empty;
		*(const unsigned char **)(buf + 0xcac) = empty;
		found = d->FindPartition(100, out);
		check_eq("empty range -> false", found, false);
		check_eq("empty range -> out == 0", (unsigned long)out, 0);
	}

	printf("[10] GetPTRecord -- base + index*0x10c, out-of-range asserts but still returns\n");
	{
		static unsigned char records[3 * 0x10c];
		memset(buf, 0, sizeof(buf));
		CDirCD *d = (CDirCD *)buf;
		*(const unsigned char **)(buf + 0xc94) = records;
		*(unsigned long *)(buf + 0xc98) = 3;

		const CDirCD::PTRecord *r1 = d->GetPTRecord(1);
		check_eq("index 1 -> records + 0x10c", (unsigned long)((const unsigned char *)r1 - records), 0x10c);

		const CDirCD::PTRecord *r5 = d->GetPTRecord(5); /* out of range, real GT still computes+returns */
		check_eq("out-of-range index still computes pointer",
		         (unsigned long)((const unsigned char *)r5 - records), 5UL * 0x10c);
		check_eq("sizeof(PTRecord) == 0x10c", sizeof(CDirCD::PTRecord), 0x10c);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
