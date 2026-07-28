/*
 * test_korg_file.cpp  -  host-side known-answer test for CKorgFile
 * (src/init/korg_file.cpp). See include/korg_file.h for full ground-truth
 * provenance.
 *
 * CKorgFile is abstract (ImportToBank()/LoadChunk() are real __cxa_pure_virtual
 * slots in ground truth -- see header comment). A trivial concrete subclass
 * exercises everything else: ctor/SetPath extension logic, path/name/extension
 * string helpers, and the transfer (fopen/fread/fwrite/fclose) session pair.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "korg_file.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

class CTestKorgFile : public CKorgFile {
public:
	CTestKorgFile(const char *name, const char *ext) : CKorgFile(name, ext) {}
	virtual int ImportToBank() { return 42; }
	virtual int LoadChunk() { return 99; }
};

int main()
{
	printf("CKorgFile known-answer test\n");
	printf("============================\n");

	printf("[1] ctor: extension auto-appended when missing\n");
	{
		CTestKorgFile f("/tmp/foo", ".KMP");
		check("GetPathName == foo.KMP", strcmp(f.GetPathName(), "foo.KMP") == 0);
	}

	printf("[2] ctor: extension NOT duplicated when already present (case-insensitive)\n");
	{
		CTestKorgFile f("/tmp/foo.kmp", ".KMP");
		check("GetPathName == foo.kmp (unchanged)", strcmp(f.GetPathName(), "foo.kmp") == 0);
	}

	printf("[3] ctor: NULL name -> empty path\n");
	{
		CTestKorgFile f(0, ".KSF");
		check("GetPathName == \"\"", strcmp(f.GetPathName(), "") == 0);
	}

	printf("[4] SetPath: same extension-append behavior as ctor\n");
	{
		CTestKorgFile f(0, ".KSC");
		f.SetPath("/a/b/bank");
		check("GetPathName == bank.KSC", strcmp(f.GetPathName(), "bank.KSC") == 0);
		f.SetPath(0);
		check("SetPath(NULL) empties path", strcmp(f.GetPathName(), "") == 0);
	}

	printf("[5] GetPathName: strips directory\n");
	{
		CTestKorgFile f("/korg/rw/Sounds/Piano.KMP", ".KMP");
		check("GetPathName == Piano.KMP", strcmp(f.GetPathName(), "Piano.KMP") == 0);
	}

	printf("[6] GetPathNameNoExtension\n");
	{
		CTestKorgFile f("/korg/rw/Sounds/Piano.KMP", ".KMP");
		char buf[64];
		f.GetPathNameNoExtension(buf, sizeof(buf));
		check("== Piano", strcmp(buf, "Piano") == 0);
	}

	printf("[7] GetFolder: true + truncated at first '.'\n");
	{
		CTestKorgFile f("/korg/rw/Sounds/Piano.KMP", ".KMP");
		char buf[64];
		int had = f.GetFolder(buf, sizeof(buf));
		check("returns true", had == 1);
		check("== /korg/rw/Sounds/Piano", strcmp(buf, "/korg/rw/Sounds/Piano") == 0);
	}

	printf("[8] MakePathFromFolder\n");
	{
		CTestKorgFile f("/korg/rw/Sounds/Piano.KMP", ".KMP");
		char buf[64];
		int had = f.MakePathFromFolder(buf, "Extra.KSC", sizeof(buf));
		check("returns true (had extension)", had == 1);
		check("== /korg/rw/Sounds/Piano/Extra.KSC",
		      strcmp(buf, "/korg/rw/Sounds/Piano/Extra.KSC") == 0);
	}

	printf("[9] static HasExtension / ValidExtension / AddExtension / RemoveExtension\n");
	{
		check("HasExtension true (case-insens)", CKorgFile::HasExtension("foo.KMP", ".kmp") == 1);
		check("HasExtension false", CKorgFile::HasExtension("foo.KSF", ".kmp") == 0);
		check("HasExtension false (no dot)", CKorgFile::HasExtension("foo", ".kmp") == 0);

		check("ValidExtension true", CKorgFile::ValidExtension(".KMP") == 1);
		check("ValidExtension false (no leading dot)", CKorgFile::ValidExtension("KMP") == 0);
		check("ValidExtension false (NULL)", CKorgFile::ValidExtension(0) == 0);

		char name[32];
		strcpy(name, "foo");
		CKorgFile::AddExtension(name, sizeof(name), ".KSC");
		check("AddExtension == foo.KSC", strcmp(name, "foo.KSC") == 0);

		check("RemoveExtension(char*) true", CKorgFile::RemoveExtension(name) == 1);
		check("RemoveExtension(char*) == foo", strcmp(name, "foo") == 0);
		check("RemoveExtension(char*) false (no dot)", CKorgFile::RemoveExtension(name) == 0);

		char src[32] = "bar.KMP";
		char dest[32];
		int had = CKorgFile::RemoveExtension(src, dest, sizeof(dest));
		check("RemoveExtension(3-arg) true", had == 1);
		check("dest == bar", strcmp(dest, "bar") == 0);
		check("src truncated to bar", strcmp(src, "bar") == 0);
	}

	printf("[10] static ExtractName\n");
	{
		char dest[32];
		int had = CKorgFile::ExtractName("/a/b/Piano.KMP", dest, sizeof(dest));
		check("returns true", had == 1);
		check("== Piano", strcmp(dest, "Piano") == 0);

		had = CKorgFile::ExtractName("NoDir.KSF", dest, sizeof(dest));
		check("no-slash case == NoDir", had == 1 && strcmp(dest, "NoDir") == 0);
	}

	printf("[11] static Sanitize\n");
	{
		char buf[32];
		strcpy(buf, "A-B C_D");
		CKorgFile::Sanitize(buf);
		check("drops '-' and spaces, keeps rest", strcmp(buf, "ABC_D") == 0);
	}

	printf("[12] static Capitalized\n");
	{
		check("all-upper true", CKorgFile::Capitalized("FOO123") == 1);
		check("has lowercase false", CKorgFile::Capitalized("Foo") == 0);
		check("empty string true (vacuous)", CKorgFile::Capitalized("") == 1);
	}

	printf("[13] static NameLength (bounded strnlen)\n");
	{
		check("shorter than maxLen", CKorgFile::NameLength("abc", 10) == 3);
		check("exactly maxLen (no NUL within bound)", CKorgFile::NameLength("abcdefgh", 4) == 4);
		check("empty name -> 0", CKorgFile::NameLength("", 10) == 0);
		check("maxLen 0 -> 0", CKorgFile::NameLength("abc", 0) == 0);
	}

	printf("[14] static MakeName (copy + trim trailing spaces)\n");
	{
		char dest[16];
		CKorgFile::MakeName("Lead   ", dest, sizeof(dest));
		check("trailing spaces trimmed", strcmp(dest, "Lead") == 0);
	}

	printf("[15] static MakeNameStereo / MakeNameLeft / MakeNameRight\n");
	{
		char dest[10];
		CKorgFile::MakeNameStereo("Lead", dest, sizeof(dest), 'L');
		check("Lead padded + -L suffix",
		      memcmp(dest, "Lead    -L", 10) == 0 || strcmp(dest + 8, "-L") == 0);

		char dest2[10];
		CKorgFile::MakeNameRight("Lead", dest2, sizeof(dest2));
		check("MakeNameRight == '-R' suffix", dest2[8] == '-' && dest2[9] == 'R');

		char dest3[10];
		CKorgFile::MakeNameLeft("Lead", dest3, sizeof(dest3));
		check("MakeNameLeft == '-L' suffix", dest3[8] == '-' && dest3[9] == 'L');

		/* Re-running MakeNameLeft on an already-suffixed name should strip the
		 * old "-L" before re-padding/re-affixing, not double it up. Source and
		 * dest are deliberately separate buffers (ground truth's own strncpy
		 * call doesn't guarantee overlap-safety, and callers never alias them). */
		char nameBuf[11] = "Lead    -L"; /* 10 payload bytes + implicit NUL */
		char dest4[10];
		CKorgFile::MakeNameLeft(nameBuf, dest4, sizeof(dest4));
		check("existing matching suffix stripped+reapplied, not doubled",
		      dest4[8] == '-' && dest4[9] == 'L' && dest4[7] == ' ');
	}

	printf("[16] static MakeFileName\n");
	{
		char name[32];
		strcpy(name, "Piano");
		CKorgFile::MakeFileName(name, sizeof(name), ".KMP");
		check("appends extension", strcmp(name, "Piano.KMP") == 0);
	}

	printf("[17] static WriteEmptyFile: writes `count` zero bytes\n");
	{
		const char *path = "/tmp/korg_file_test_empty.bin";
		FILE *f = fopen(path, "wb");
		check("tmp file opened", f != 0);
		if (f) {
			CKorgFile::WriteEmptyFile(f, 0, 5000);
			fclose(f);

			FILE *r = fopen(path, "rb");
			unsigned char buf[5000];
			size_t n = fread(buf, 1, sizeof(buf), r);
			fclose(r);
			remove(path);

			bool allZero = true;
			for (size_t i = 0; i < n; i++)
				if (buf[i] != 0)
					allZero = false;
			check("wrote exactly `count` bytes", n == 5000);
			check("every byte is zero", allZero);
		}
	}

	printf("[18] Read()/Write() dispatch through the derived override + fopen/fclose\n");
	{
		const char *path = "/tmp/korg_file_test_rw.bin";
		CTestKorgFile f(path, "");
		int wr = f.Write();
		check("Write() returns LoadChunk()'s value (99)", wr == 99);

		int rd = f.Read();
		check("Read() returns ImportToBank()'s value (42)", rd == 42);
		remove(path);

		CTestKorgFile missing("/nonexistent_dir_xyz/nofile", "");
		check("Read() returns -1 when fopen fails", missing.Read() == -1);
	}

	printf("[19] TransferFromBegin/TransferFrom/TransferFromEnd session\n");
	{
		const char *path = "/tmp/korg_file_test_transfer.bin";
		FILE *seed = fopen(path, "wb");
		const char payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		fwrite(payload, 1, sizeof(payload), seed);
		fclose(seed);

		CTestKorgFile f(path, "");
		f.TransferFromBegin(0);
		unsigned char buf[8];
		f.TransferFrom(buf, 1, 8);
		f.TransferFromEnd();
		remove(path);

		check("read back the seeded bytes", memcmp(buf, payload, 8) == 0);
	}

	printf("\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
