// SPDX-License-Identifier: GPL-2.0
/*
 * scale.cpp  -  see include/oa_scale.h.
 *
 * ScaleInteger<T>() is an implementation detail (not itself a real OA.ko
 * symbol) that captures the one algorithm ScaleLong/Short/Word/Byte/Char all
 * share -- C++'s own integer-promotion rules for the narrower types
 * reproduce the movzx/movsx-to-32-bit widening the real disassembly does
 * before its imul/idiv, so a single template is a faithful, not merely
 * convenient, way to express all five.
 */

#include "oa_scale.h"

template <typename T>
static inline T ScaleInteger(T value, T inMin, T inMax, T outMin, T outMax)
{
	if (inMin == inMax)
		return (T)(outMin + (outMax - outMin) * (value - inMin));
	return (T)(outMin + (long)(value - inMin) * (long)(outMax - outMin) / (long)(inMax - inMin));
}

long ScaleLong(long value, long inMin, long inMax, long outMin, long outMax)
{
	return ScaleInteger<long>(value, inMin, inMax, outMin, outMax);
}

short ScaleShort(short value, short inMin, short inMax, short outMin, short outMax)
{
	return ScaleInteger<short>(value, inMin, inMax, outMin, outMax);
}

unsigned short ScaleWord(unsigned short value, unsigned short inMin, unsigned short inMax,
                          unsigned short outMin, unsigned short outMax)
{
	return ScaleInteger<unsigned short>(value, inMin, inMax, outMin, outMax);
}

unsigned char ScaleByte(unsigned char value, unsigned char inMin, unsigned char inMax,
                         unsigned char outMin, unsigned char outMax)
{
	return ScaleInteger<unsigned char>(value, inMin, inMax, outMin, outMax);
}

char ScaleChar(char value, char inMin, char inMax, char outMin, char outMax)
{
	return ScaleInteger<char>(value, inMin, inMax, outMin, outMax);
}

long ScaleLongDouble(long value, long inMin, long inMax, long outMin, long outMax)
{
	double numerator = (double)(outMax - outMin) * (double)(value - inMin);

	if (inMin != inMax)
		numerator = numerator / (double)(inMax - inMin);

	return outMin + (long)numerator;
}
