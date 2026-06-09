#!/usr/bin/env python3
"""
Korg Kronos EX authorisation string generator — offline reference implementation.

Implements the same algorithm as OA.ko::ParseAuth / VerifyAuthorizationString
and the proposed oa_authgen.ko kernel module.  Use this to:
  1. Verify the algorithm offline before building the kernel module.
  2. Generate auth strings on a host machine given a captured chip secret.
  3. Round-trip test: generate → decode → verify.

Algorithm spec: kronosology/docs/crypto/auth_string_algorithm.md

Requires: pycryptodome  (pip install pycryptodome)

Usage examples
--------------
# Generate an auth string given the chip secret as a hex string:
python3 auth_gen.py gen S285 /path/to/Sxxx_option_file \
        --chip-secret <48-hex-chars>

# Round-trip self-test with a synthetic chip secret:
python3 auth_gen.py selftest

# Verify an existing auth string (e.g. from AuthorizationStrings):
python3 auth_gen.py verify XFGH9ZQD0RC65GZT7UNQAHTL \
        --chip-secret <48-hex-chars> --option-file /path/to/Sxxx
"""

import sys
import argparse
import hashlib
import struct

try:
    from Crypto.Cipher import Blowfish
except ImportError:
    sys.exit("pycryptodome is required: pip install pycryptodome")

# ---------------------------------------------------------------------------
# Custom base32
# Encode alphabet (no B, O, I, S):
ALPHA = "0123456789ACDEFGHJKLMNPQRTUVWXYZ"
assert len(ALPHA) == 32

# Decode remap: visually ambiguous chars accepted as their digit lookalikes
_REMAP = {'B': '8', 'O': '0', 'I': '1', 'S': '5'}

def _b32_encode_5(chunk: bytes) -> str:
    """Encode exactly 5 bytes as 8 base32 characters."""
    assert len(chunk) == 5
    val = int.from_bytes(chunk, 'big')  # 40-bit integer
    chars = []
    for _ in range(8):
        chars.append(ALPHA[val & 0x1F])
        val >>= 5
    return ''.join(reversed(chars))

def base32_encode(data: bytes) -> str:
    """Encode 15 bytes → 24 base32 chars (3 × 5-byte chunks)."""
    assert len(data) == 15
    return _b32_encode_5(data[0:5]) + _b32_encode_5(data[5:10]) + _b32_encode_5(data[10:15])

def _b32_decode_5(s: str) -> bytes:
    """Decode 8 base32 characters to exactly 5 bytes."""
    assert len(s) == 8
    val = 0
    for c in s:
        c = _REMAP.get(c.upper(), c.upper())
        idx = ALPHA.index(c)
        val = (val << 5) | idx
    return val.to_bytes(5, 'big')

def base32_decode(s: str) -> bytes:
    """Decode 24 base32 chars → 15 bytes."""
    assert len(s) == 24
    return _b32_decode_5(s[0:8]) + _b32_decode_5(s[8:16]) + _b32_decode_5(s[16:24])

# ---------------------------------------------------------------------------
# Blowfish-CFB-8

def blowfish_cfb8_encrypt(key: bytes, iv: bytes, data: bytes) -> bytes:
    """Encrypt data byte-by-byte in CFB-8 mode using Blowfish.

    key: 4–56 bytes (we use 24)
    iv:  8 bytes (1 Blowfish block = 64 bits)
    """
    # pycryptodome's Blowfish supports CFB mode but with segment_size in bits.
    # CFB-8 = segment_size=8.
    cipher = Blowfish.new(key, Blowfish.MODE_CFB, iv=iv, segment_size=8)
    return cipher.encrypt(data)

def blowfish_cfb8_decrypt(key: bytes, iv: bytes, data: bytes) -> bytes:
    cipher = Blowfish.new(key, Blowfish.MODE_CFB, iv=iv, segment_size=8)
    return cipher.decrypt(data)

# ---------------------------------------------------------------------------
# Auth string generation

def generate_auth_string(chip_secret: bytes,
                         option_id: str,
                         option_file_bytes: bytes,
                         salt: bytes = b'\x00' * 8) -> str:
    """Generate the 24-char authorisation string for a given EX.

    chip_secret:       24 bytes from Atmel NV2AC (addresses 0x10, 0x18, 0x20)
    option_id:         4-char string, e.g. "S285"
    option_file_bytes: raw content of /korg/rw/Options/<option_id>
    salt:              8 bytes; use zeros for reproducibility (issuer-chosen)
    """
    assert len(chip_secret) == 24
    assert len(option_id) == 4
    assert len(salt) == 8

    # Step 1 & 2: Build plain_12 and compute MD5 fingerprint
    opt_bytes = option_id.encode('ascii')
    plain_12 = salt + opt_bytes
    assert len(plain_12) == 12

    digest = hashlib.md5(plain_12 + option_file_bytes).digest()
    fp = bytes([digest[3], digest[7], digest[11]])

    # Step 3: Assemble plaintext_15
    plaintext = plain_12 + fp
    assert len(plaintext) == 15

    # Step 4: Blowfish-CFB-8 encrypt
    iv = chip_secret[16:24]
    ciphertext = blowfish_cfb8_encrypt(chip_secret, iv, plaintext)
    assert len(ciphertext) == 15

    # Step 5: Custom base32 encode
    return base32_encode(ciphertext)


def verify_auth_string(auth_str: str,
                       chip_secret: bytes,
                       option_id: str,
                       option_file_bytes: bytes) -> bool:
    """Verify an auth string the way OA.ko::ParseAuth does.

    Returns True if valid, False otherwise.
    """
    assert len(auth_str) == 24
    assert len(chip_secret) == 24

    try:
        ct = base32_decode(auth_str)
    except (ValueError, AssertionError):
        return False

    iv = chip_secret[16:24]
    pt = blowfish_cfb8_decrypt(chip_secret, iv, ct)

    salt      = pt[0:8]
    opt_id_pt = pt[8:12].decode('ascii', errors='replace')
    fp_claimed = pt[12:15]

    if opt_id_pt.rstrip('\x00') != option_id.rstrip('\x00'):
        return False

    plain_12 = salt + pt[8:12]
    digest = hashlib.md5(plain_12 + option_file_bytes).digest()
    fp_expected = bytes([digest[3], digest[7], digest[11]])

    return fp_claimed == fp_expected


# ---------------------------------------------------------------------------
# Self-test

def selftest():
    """Round-trip test with a synthetic chip secret and option file."""
    import os
    chip = os.urandom(24)
    opt_id = "S285"
    opt_file = b"EXs285\nTest Expansion Bank\n285\n2,24,EXs285 Test Expansion Bank\n"

    auth = generate_auth_string(chip, opt_id, opt_file)
    print(f"Generated auth string: {auth}")
    assert len(auth) == 24, "auth string must be 24 chars"

    ok = verify_auth_string(auth, chip, opt_id, opt_file)
    assert ok, "verification of freshly-generated string failed"
    print("Round-trip: PASS")

    # Verify that a wrong chip secret fails
    bad_chip = bytes([b ^ 0xFF for b in chip])
    fail = verify_auth_string(auth, bad_chip, opt_id, opt_file)
    assert not fail, "wrong chip secret should have failed verification"
    print("Wrong-chip reject: PASS")

    # Verify that a wrong option file fails
    fail2 = verify_auth_string(auth, chip, opt_id, opt_file + b"extra")
    assert not fail2, "wrong option file should have failed verification"
    print("Wrong-file reject: PASS")

    print("All self-tests passed.")


# ---------------------------------------------------------------------------
# CLI

def main():
    parser = argparse.ArgumentParser(
        description="Korg Kronos EX authorisation string tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)

    sub = parser.add_subparsers(dest='cmd')

    p_gen = sub.add_parser('gen', help='Generate auth string for an EX')
    p_gen.add_argument('option_id', help='4-char option file basename, e.g. S285')
    p_gen.add_argument('option_file', help='Path to /korg/rw/Options/<option_id>')
    p_gen.add_argument('--chip-secret', required=True,
                       help='48 hex chars = 24 bytes NV2AC chip secret')
    p_gen.add_argument('--salt', default='00' * 8,
                       help='16 hex chars = 8 bytes salt (default: all zeros)')

    p_ver = sub.add_parser('verify', help='Verify an existing auth string')
    p_ver.add_argument('auth_string', help='24-char auth string to verify')
    p_ver.add_argument('--chip-secret', required=True,
                       help='48 hex chars = 24 bytes NV2AC chip secret')
    p_ver.add_argument('--option-id', required=True,
                       help='4-char option file basename, e.g. S285')
    p_ver.add_argument('--option-file', required=True,
                       help='Path to the option file')

    sub.add_parser('selftest', help='Run round-trip self-test')

    args = parser.parse_args()

    if args.cmd == 'selftest':
        selftest()

    elif args.cmd == 'gen':
        chip = bytes.fromhex(args.chip_secret)
        if len(chip) != 24:
            sys.exit("--chip-secret must be 48 hex chars (24 bytes)")
        salt = bytes.fromhex(args.salt)
        if len(salt) != 8:
            sys.exit("--salt must be 16 hex chars (8 bytes)")
        opt_id = args.option_id
        if len(opt_id) != 4:
            sys.exit("option_id must be exactly 4 characters, e.g. S285")
        with open(args.option_file, 'rb') as f:
            opt_bytes = f.read()
        auth = generate_auth_string(chip, opt_id, opt_bytes, salt)
        print(auth)

    elif args.cmd == 'verify':
        chip = bytes.fromhex(args.chip_secret)
        if len(chip) != 24:
            sys.exit("--chip-secret must be 48 hex chars (24 bytes)")
        opt_id = args.option_id
        if len(opt_id) != 4:
            sys.exit("--option-id must be exactly 4 characters, e.g. S285")
        with open(args.option_file, 'rb') as f:
            opt_bytes = f.read()
        ok = verify_auth_string(args.auth_string, chip, opt_id, opt_bytes)
        print("VALID" if ok else "INVALID")
        sys.exit(0 if ok else 1)

    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
