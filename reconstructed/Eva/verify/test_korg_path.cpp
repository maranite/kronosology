/*
 * test_korg_path.cpp  -  host-side known-answer test for CKorgPath /
 * CKorgLinuxPath (src/init/korg_path.cpp, src/init/korg_linux_path.cpp) and
 * UKontaktOposPath (src/init/kontakt_opos_path.cpp). See include/korg_path.h
 * and include/korg_linux_path.h for full ground-truth provenance.
 *
 * Links against src/base/file_operation_stub.cpp's
 * CFileOperation::GetLinuxRemapPath() stand-in -- this test does genuine
 * round-trip work against the build host filesystem: real mkdir()'d
 * directory trees for FindRecurse()/Find(), and real OPOS<->Linux path
 * conversion through the same 8-entry mount table the stub provides.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>

#include "korg_path.h"
#include "korg_linux_path.h"
#include "kontakt_opos_path.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CKorgPath / CKorgLinuxPath known-answer test\n");
	printf("==============================================\n");

	printf("[1] Make() always produces a CKorgLinuxPath\n");
	{
		CKorgPath *p = CKorgPath::Make("/korg/rw/foo.KMP");
		check("Separator == '/'", p->Separator() == '/');
		char buf[256];
		p->GetPath(buf, sizeof(buf));
		check("GetPath == /korg/rw/foo.KMP", strcmp(buf, "/korg/rw/foo.KMP") == 0);
		delete p;
	}

	printf("[2] GetPathName / GetPathExtension / GetPathNameNoExtension\n");
	{
		CKorgLinuxPath p("/korg/rw/Sounds/Piano.KMP");
		check("GetPathName == Piano.KMP", strcmp(p.GetPathName(), "Piano.KMP") == 0);
		check("GetPathExtension == .KMP", strcmp(p.GetPathExtension(), ".KMP") == 0);

		char buf[64];
		int had = p.GetPathNameNoExtension(buf, sizeof(buf));
		check("GetPathNameNoExtension returns true", had == 1);
		check("GetPathNameNoExtension == Piano", strcmp(buf, "Piano") == 0);
	}

	printf("[3] GetPathName: no separator -> whole string\n");
	{
		CKorgLinuxPath p("bare.txt");
		check("GetPathName == bare.txt", strcmp(p.GetPathName(), "bare.txt") == 0);
	}

	printf("[4] GetFolder: returns a NEW CKorgPath truncated at the last separator\n");
	{
		CKorgLinuxPath p("/korg/rw/Sounds/Piano.KMP");
		CKorgPath *folder = p.GetFolder();
		char buf[256];
		folder->GetPath(buf, sizeof(buf));
		check("== /korg/rw/Sounds", strcmp(buf, "/korg/rw/Sounds") == 0);
		check("original unchanged", strcmp(p.GetPathName(), "Piano.KMP") == 0);
		delete folder;
	}

	printf("[5] Set: base->path + Separator() + name\n");
	{
		CKorgLinuxPath base("/korg/rw/Sounds");
		CKorgLinuxPath p((const char *)0);
		p.Set(&base, "Piano.KMP");
		check("GetPathName == Piano.KMP", strcmp(p.GetPathName(), "Piano.KMP") == 0);
		char buf[256];
		p.GetPath(buf, sizeof(buf));
		check("GetPath == /korg/rw/Sounds/Piano.KMP", strcmp(buf, "/korg/rw/Sounds/Piano.KMP") == 0);
	}

	printf("[6] static HasExtension / ValidExtension / AddExtension / RemoveExtension\n");
	{
		check("HasExtension true (case-insens)", CKorgPath::HasExtension("foo.KMP", ".kmp") == 1);
		check("HasExtension false", CKorgPath::HasExtension("foo.KSF", ".kmp") == 0);
		check("HasExtension false (no dot)", CKorgPath::HasExtension("foo", ".kmp") == 0);

		check("ValidExtension true", CKorgPath::ValidExtension(".KMP") == 1);
		check("ValidExtension false (no dot)", CKorgPath::ValidExtension("KMP") == 0);
		check("ValidExtension false (NULL)", CKorgPath::ValidExtension(0) == 0);

		char name[32];
		strcpy(name, "foo");
		CKorgPath::AddExtension(name, sizeof(name), ".KSC");
		check("AddExtension == foo.KSC", strcmp(name, "foo.KSC") == 0);

		check("RemoveExtension(char*) true", CKorgPath::RemoveExtension(name) == 1);
		check("RemoveExtension(char*) == foo", strcmp(name, "foo") == 0);
		check("RemoveExtension(char*) false (no dot)", CKorgPath::RemoveExtension(name) == 0);

		char src[32] = "bar.KMP";
		char dest[32];
		int had = CKorgPath::RemoveExtension(src, dest, sizeof(dest));
		check("RemoveExtension(3-arg) true", had == 1);
		check("RemoveExtension(3-arg): name stripped == bar", strcmp(src, "bar") == 0);
		check("RemoveExtension(3-arg): dest == extension .KMP", strcmp(dest, ".KMP") == 0);

		char noext[32] = "baz";
		char dest2[32];
		had = CKorgPath::RemoveExtension(noext, dest2, sizeof(dest2));
		check("RemoveExtension(3-arg) false (no dot)", had == 0);
		check("RemoveExtension(3-arg): dest empty on no match", dest2[0] == 0);
	}

	printf("[7] Sanitize / Capitalized\n");
	{
		char name[32];
		strcpy(name, "Foo-Bar_Baz Qux");
		CKorgPath::Sanitize(name);
		check("Sanitize drops -, _, space", strcmp(name, "FooBarBazQux") == 0);

		check("Capitalized true (all caps)", CKorgPath::Capitalized("PIANO") == 1);
		check("Capitalized true (no letters)", CKorgPath::Capitalized("123") == 1);
		check("Capitalized false (has lowercase)", CKorgPath::Capitalized("Piano") == 0);
	}

	printf("[8] MakePathFromFolder: ground truth always returns 0 (leaked/unused clone)\n");
	{
		CKorgLinuxPath p("/korg/rw/Sounds/Piano.KMP");
		check("returns 0", p.MakePathFromFolder("Extra.KSC") == 0);
	}

	printf("[9] real filesystem: FindRecurse via Find(), recursive tree search\n");
	{
		const char *root = "/tmp/eva_korgpath_test";
		char cmd[512];
		snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
		system(cmd);

		char sub1[256], sub2[256], hidden[256];
		snprintf(sub1, sizeof(sub1), "%s/subdir1", root);
		snprintf(sub2, sizeof(sub2), "%s/subdir1/subdir2", root);
		snprintf(hidden, sizeof(hidden), "%s/.hidden", root);
		mkdir(root, 0755);
		mkdir(sub1, 0755);
		mkdir(sub2, 0755);
		mkdir(hidden, 0755);

		char targetPath[256], decoyPath[256], hiddenTargetPath[256];
		snprintf(targetPath, sizeof(targetPath), "%s/target.txt", sub2);
		snprintf(decoyPath, sizeof(decoyPath), "%s/decoy.txt", sub1);
		snprintf(hiddenTargetPath, sizeof(hiddenTargetPath), "%s/target.txt", hidden);

		FILE *f;
		f = fopen(targetPath, "w"); fputs("real", f); fclose(f);
		f = fopen(decoyPath, "w"); fputs("decoy", f); fclose(f);
		f = fopen(hiddenTargetPath, "w"); fputs("hidden", f); fclose(f);

		char rootFileTemplate[256];
		snprintf(rootFileTemplate, sizeof(rootFileTemplate), "%s/dummy.ext", root);
		CKorgLinuxPath searchRoot(rootFileTemplate);
		CKorgLinuxPath searchFor("target.txt");

		CKorgPath *found = searchRoot.Find(searchFor);
		check("Find() located target.txt", found != 0);
		if (found) {
			char buf[256];
			found->GetPath(buf, sizeof(buf));
			check("found path == subdir1/subdir2/target.txt", strcmp(buf, targetPath) == 0);
			delete found;
		}

		printf("  -- direct FindRecurse(), skips hidden dirs --\n");
		CKorgPath *notFound = searchRoot.FindRecurse("nonexistent.xyz", &searchRoot);
		check("FindRecurse returns NULL for missing name", notFound == 0);

		snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
		system(cmd);
	}

	printf("[10] Find(): fast path when the literal file already exists\n");
	{
		const char *path = "/tmp/eva_korgpath_test_direct.txt";
		FILE *f = fopen(path, "w");
		fputs("x", f);
		fclose(f);

		CKorgLinuxPath other(path);
		CKorgLinuxPath dummyRoot("/tmp/dummy.ext");
		CKorgPath *found = dummyRoot.Find(other);
		check("Find() returns a copy via the fopen fast path", found != 0);
		if (found) {
			char buf[256];
			found->GetPath(buf, sizeof(buf));
			check("== original literal path", strcmp(buf, path) == 0);
			delete found;
		}
		remove(path);
	}

	printf("[11] UKontaktOposPath: real round-trip OPOS<->Linux conversion\n");
	{
		char linux1[256];
		int ok1 = UKontaktOposPath::ConvertOposToLinux("A:\\FOO.WAV", linux1, sizeof(linux1));
		check("ConvertOposToLinux returns 1", ok1 == 1);
		check("== /korg/rw/FOO.WAV", strcmp(linux1, "/korg/rw/FOO.WAV") == 0);

		char opos1[256];
		int ok2 = UKontaktOposPath::ConvertLinuxToOpos(linux1, opos1, sizeof(opos1));
		check("ConvertLinuxToOpos returns 1", ok2 == 1);
		check("round-trip == A:\\FOO.WAV", strcmp(opos1, "A:\\FOO.WAV") == 0);

		char linux2[256];
		int ok3 = UKontaktOposPath::ConvertOposToLinux("no-colon-path", linux2, sizeof(linux2));
		check("ConvertOposToLinux: no drive letter -> 0", ok3 == 0);
		check("dest left empty", linux2[0] == 0);

		char opos2[256];
		int ok4 = UKontaktOposPath::ConvertLinuxToOpos("/no/such/mount/file.wav", opos2, sizeof(opos2));
		check("ConvertLinuxToOpos: no matching mount -> 0", ok4 == 0);
		check("dest left empty", opos2[0] == 0);
	}

	printf("[12] CKorgLinuxPath::GetOposPath/SetOposPath (through the class, via ConvertXxx)\n");
	{
		CKorgLinuxPath p("/mnt/usb0/SAMPLE.WAV");
		char opos[256];
		p.GetOposPath(opos, sizeof(opos));
		check("GetOposPath == B:\\SAMPLE.WAV", strcmp(opos, "B:\\SAMPLE.WAV") == 0);

		CKorgLinuxPath p2((const char *)0);
		p2.SetOposPath("C:\\DIR\\FILE.KSC");
		check("SetOposPath -> GetPathName == FILE.KSC", strcmp(p2.GetPathName(), "FILE.KSC") == 0);
		char buf[256];
		p2.GetPath(buf, sizeof(buf));
		check("SetOposPath -> full path == /mnt/usb1/DIR/FILE.KSC", strcmp(buf, "/mnt/usb1/DIR/FILE.KSC") == 0);
	}

	printf("\n%s\n", g_fail ? "FAILED" : "ALL PASS");
	return g_fail ? 1 : 0;
}
