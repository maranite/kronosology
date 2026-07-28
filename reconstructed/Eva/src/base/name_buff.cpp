/*
 * name_buff.cpp  -  see include/name_buff.h.
 *
 * Every CNameBuff method is defined inline in the header (matching this
 * project's established convention for small classes, e.g. bit_mask_l.h,
 * stream_family.h). This file only provides the real ground-truth external
 * backing store CNameBuff::setup() points itself at.
 */

#include "name_buff.h"

unsigned char theDiskNameBuf[0x271000];
