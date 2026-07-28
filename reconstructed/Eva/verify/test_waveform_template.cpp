/*
 * test_waveform_template.cpp  -  host-side known-answer test for CWaveformTemplate
 * (src/editor/waveform_template.cpp). See include/waveform_template.h for full
 * ground-truth provenance.
 *
 * Expected values below were derived independently of the C++ translation: a small
 * x86-32 instruction interpreter (host-side tooling, not part of this repo) replayed the
 * REAL disassembly instruction-by-instruction for thousands of randomized (x,y,z) triples
 * per Equation* function (this file's literals are a small, hand-picked, human-checkable
 * subset of that much larger regression sweep, which stayed at 0 mismatches).
 */

#include <cstdio>
#include <cstdlib>

#include "waveform_template.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct CWaveformTemplateTestHooks {
	static void Setup(CWaveformTemplate &t, unsigned char *data, unsigned short size,
			   unsigned char *shape, unsigned short count)
	{
		t.m_pbData = data;
		t.m_wSize = size;
		t.m_pbShapeTable = shape;
		t.m_wCount = count;
	}
};

int main()
{
	printf("test_waveform_template\n");

	/* ---- EquationNone ---- */
	check("None(5,7,9) == 0", CWaveformTemplate::EquationNone(5, 7, 9) == 0);

	/* ---- EquationTriangle ---- */
	check("Triangle(0,100,8) == 0", CWaveformTemplate::EquationTriangle(0, 100, 8) == 0);
	check("Triangle(2,100,8) == 50", CWaveformTemplate::EquationTriangle(2, 100, 8) == 50);
	check("Triangle(4,100,8) == 0", CWaveformTemplate::EquationTriangle(4, 100, 8) == 0);
	check("Triangle(6,100,8) == -50", CWaveformTemplate::EquationTriangle(6, 100, 8) == -50);
	check("Triangle(7,100,8) == -25", CWaveformTemplate::EquationTriangle(7, 100, 8) == -25);
	check("Triangle(-1,100,8) == -25", CWaveformTemplate::EquationTriangle(-1, 100, 8) == -25);
	check("Triangle(3,-50,8) == -12", CWaveformTemplate::EquationTriangle(3, -50, 8) == -12);

	/* ---- EquationSaw: ((z>>1)-x)*y/z ---- */
	check("Saw(0,100,8) == 50", CWaveformTemplate::EquationSaw(0, 100, 8) == 50);
	check("Saw(4,100,8) == 0", CWaveformTemplate::EquationSaw(4, 100, 8) == 0);
	check("Saw(7,100,8) == -37", CWaveformTemplate::EquationSaw(7, 100, 8) == -37);

	/* ---- EquationSquare ---- */
	check("Square(0,10,4) == 5", CWaveformTemplate::EquationSquare(0, 10, 4) == 5);
	check("Square(3,10,4) == -5", CWaveformTemplate::EquationSquare(3, 10, 4) == -5);
	check("Square(2,10,4) == -5", CWaveformTemplate::EquationSquare(2, 10, 4) == -5);
	check("Square(-1,0,4) == 0", CWaveformTemplate::EquationSquare(-1, 0, 4) == 0);

	/* ---- EquationStepTri4 (4-step quantized triangle) ---- */
	check("StepTri4(0,100,12) == -50", CWaveformTemplate::EquationStepTri4(0, 100, 12) == -50);
	check("StepTri4(3,100,12) == 0", CWaveformTemplate::EquationStepTri4(3, 100, 12) == 0);
	check("StepTri4(6,100,12) == 50", CWaveformTemplate::EquationStepTri4(6, 100, 12) == 50);
	check("StepTri4(9,100,12) == 0", CWaveformTemplate::EquationStepTri4(9, 100, 12) == 0);
	check("StepTri4(11,100,12) == 0", CWaveformTemplate::EquationStepTri4(11, 100, 12) == 0);

	/* ---- EquationStepTri6 (6-step quantized triangle), z=60,y=100 ---- */
	check("StepTri6(0,100,60) == -50", CWaveformTemplate::EquationStepTri6(0, 100, 60) == -50);
	check("StepTri6(10,100,60) == -16", CWaveformTemplate::EquationStepTri6(10, 100, 60) == -16);
	check("StepTri6(20,100,60) == 16", CWaveformTemplate::EquationStepTri6(20, 100, 60) == 16);
	check("StepTri6(30,100,60) == 50", CWaveformTemplate::EquationStepTri6(30, 100, 60) == 50);
	check("StepTri6(40,100,60) == 16", CWaveformTemplate::EquationStepTri6(40, 100, 60) == 16);
	check("StepTri6(50,100,60) == -16", CWaveformTemplate::EquationStepTri6(50, 100, 60) == -16);

	/* ---- EquationStepSaw4 (4-step quantized ascending saw), z=60,y=100 ---- */
	check("StepSaw4(0,100,60) == 50", CWaveformTemplate::EquationStepSaw4(0, 100, 60) == 50);
	check("StepSaw4(15,100,60) == 16", CWaveformTemplate::EquationStepSaw4(15, 100, 60) == 16);
	check("StepSaw4(30,100,60) == -16", CWaveformTemplate::EquationStepSaw4(30, 100, 60) == -16);
	check("StepSaw4(45,100,60) == -50", CWaveformTemplate::EquationStepSaw4(45, 100, 60) == -50);

	/* ---- EquationStepSaw6 (6-step quantized descending saw), z=60,y=1000 ---- */
	check("StepSaw6(0,1000,60) == 500", CWaveformTemplate::EquationStepSaw6(0, 1000, 60) == 500);
	check("StepSaw6(10,1000,60) == 300", CWaveformTemplate::EquationStepSaw6(10, 1000, 60) == 300);
	check("StepSaw6(20,1000,60) == 100", CWaveformTemplate::EquationStepSaw6(20, 1000, 60) == 100);
	check("StepSaw6(30,1000,60) == -100", CWaveformTemplate::EquationStepSaw6(30, 1000, 60) == -100);
	check("StepSaw6(40,1000,60) == -300", CWaveformTemplate::EquationStepSaw6(40, 1000, 60) == -300);
	check("StepSaw6(50,1000,60) == -500", CWaveformTemplate::EquationStepSaw6(50, 1000, 60) == -500);

	/* ---- GetData: m_pbData[idx mod m_wSize], both wrap directions ---- */
	{
		unsigned char data[5] = {10, 20, 30, 40, 50};
		unsigned char *heapData = (unsigned char *)malloc(sizeof(data));
		for (unsigned i = 0; i < sizeof(data); ++i)
			heapData[i] = data[i];

		CWaveformTemplate t;
		CWaveformTemplateTestHooks::Setup(t, heapData, 5, 0, 0);

		check("GetData(0) == 10", t.GetData(0) == 10);
		check("GetData(2) == 30", t.GetData(2) == 30);
		check("GetData(5) == 10 (wraps forward)", t.GetData(5) == 10);
		check("GetData(7) == 30 (wraps forward)", t.GetData(7) == 30);
		check("GetData(-1) == 50 (wraps backward)", t.GetData(-1) == 50);
		check("GetData(-5) == 10 (wraps backward exactly)", t.GetData(-5) == 10);
		/* destructor frees heapData via free() -- matches real ground truth exactly */
	}

	/* ---- Shape: m_pbShapeTable[clamp(m_wCount/2 + param, 0, m_wCount-1)] ---- */
	{
		unsigned char shape[7] = {1, 2, 3, 4, 5, 6, 7};
		unsigned char *heapShape = (unsigned char *)malloc(sizeof(shape));
		for (unsigned i = 0; i < sizeof(shape); ++i)
			heapShape[i] = shape[i];

		CWaveformTemplate t;
		CWaveformTemplateTestHooks::Setup(t, 0, 0, heapShape, 7);

		check("Shape(0) == 4 (center)", t.Shape(0) == 4);
		check("Shape(3) == 7 (top, in range)", t.Shape(3) == 7);
		check("Shape(10) == 7 (clamped high)", t.Shape((char)10) == 7);
		check("Shape(-5) == 1 (clamped low)", t.Shape((char)-5) == 1);
		check("Shape(-3) == 1 (exactly index 0)", t.Shape((char)-3) == 1);
	}

	printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
