// SPDX-License-Identifier: GPL-2.0
/*
 * oa_authmath.h  -  the pure arithmetic of the OA copy-protection scheme.
 *
 * Kernel-free (only fixed-width ints) so it can be unit-tested on the host against
 * known-answer vectors — see verify/test_klm_auth.c.  Recovered exactly from OA_322.ko:
 *   - FNV-1a, 32-bit, with Korg's custom offset basis (the standard prime 0x01000193,
 *     a non-standard offset 0x050c5d1f).
 *   - the runtime-keyed authorization stamp  auth = (objectId + 1 + extra) * bootKey.
 */

#ifndef OA_AUTHMATH_H
#define OA_AUTHMATH_H

#define OA_FNV1A_OFFSET 0x050c5d1fu
#define OA_FNV1A_PRIME  0x01000193u

static inline unsigned int oa_fnv1a16(const unsigned char *p)
{
	unsigned int h = OA_FNV1A_OFFSET;
	int i;
	for (i = 0; i < 16; i++)
		h = (h ^ p[i]) * OA_FNV1A_PRIME;
	return h;
}

static inline unsigned int oa_auth_value(unsigned int objectId, unsigned int extra,
					 unsigned int bootKey)
{
	return (objectId + 1u + extra) * bootKey;
}

#endif /* OA_AUTHMATH_H */
