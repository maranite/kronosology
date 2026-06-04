#!/usr/bin/env python3
"""
Offline decryption tool for Korg Kronos encrypted loop images:
  - /korg/ro/Mod.img         (kernel modules: OA.ko, KorgUsbAudioDriver.ko, USBMidiAccessory.ko)
  - /korg/ro/Eva.img         (Eva userspace binary + supporting tools)
  - /korg/ro/WaveMotion.img  (EP-1 Rhodes/Wurlitzer physical-model .wmms data)

These are encrypted by Korg's loadmod.ko via Linux cryptoloop:
  Cipher  : AES-256-CBC (lo_crypt_name = "aes" → kernel resolves to cbc(aes))
  Key     : 32 bytes (lo_encrypt_key_size = 32). Korg's HexEncode produces
            31 printable ASCII hex chars + a trailing null byte as the 32nd byte.
            All 32 bytes (including the null) form the AES-256 key.
  IV mode : "plain" — IV[0:4] = sector_num as LE32, IV[4:16] = 0
  Sector  : 512 bytes

Key extraction history: extracted on 2026-06-04 from live Kronos at 192.168.0.3
via LOOP_GET_STATUS64 ioctl (getloopkey binary). Keys are universal across all
Kronos units (proven: identical Mod.img ciphertext between update package and
on-device dump across 2014/3.2.1/3.2.2).

Usage:
  python3 decrypt_kronos_img.py <input.img> [output.img]
  python3 decrypt_kronos_img.py <input.img> --check
  python3 decrypt_kronos_img.py --decrypt-all <input_dir> <output_dir>

The image type is auto-detected from filename. If the name doesn't match,
pass --type mod|eva|wm to force one explicitly.

Examples:
  # Check whether a key is valid for an image (fast, only reads sector 2):
  python3 decrypt_kronos_img.py KRONOS_Update_3_2_2/mnt/korg/ro/Mod.img --check

  # Decrypt one image:
  python3 decrypt_kronos_img.py KRONOS_Update_3_2_2/mnt/korg/ro/Eva.img Eva_322_plain.img

  # Decrypt all three from a directory:
  python3 decrypt_kronos_img.py --decrypt-all KRONOS_Update_3_2_2/mnt/korg/ro ./decrypted_322

After decryption, browse without mounting via debugfs:
  debugfs -R 'ls -l /' Mod_322_plain.img
  debugfs -R 'dump /OA.ko ./OA_322.ko' Mod_322_plain.img

Or mount (needs root):
  sudo mount -o ro,loop Mod_322_plain.img /mnt/mod
"""
import argparse
import os
import struct
import sys

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
except ImportError:
    sys.exit("pip install cryptography  (or: apt install python3-cryptography)")


SECTOR = 512
EXT2_MAGIC = 0xEF53

# All three keys, indexed by image type. Each is exactly 32 bytes:
# 31 printable ASCII hex chars + one trailing null byte (Korg HexEncode quirk).
KEYS = {
    "mod": bytes.fromhex("6133333661313563643834316563383932366239396537633338383465616100"),
    "eva": bytes.fromhex("3334326565353964353439633764333239643833353533376265303534306400"),
    "wm":  bytes.fromhex("3365373263306535396663303137613965623764376531313638613463646200"),
}

TYPE_FROM_FILENAME = {
    "mod.img":         "mod",
    "eva.img":         "eva",
    "wavemotion.img":  "wm",
}


def auto_detect_type(path: str) -> str:
    name = os.path.basename(path).lower()
    if name in TYPE_FROM_FILENAME:
        return TYPE_FROM_FILENAME[name]
    raise SystemExit(
        f"Cannot auto-detect image type from filename {name!r}. "
        f"Use --type {{mod|eva|wm}} to force."
    )


def decrypt_sector(key: bytes, sector_num: int, ct: bytes) -> bytes:
    iv = struct.pack("<I", sector_num & 0xFFFFFFFF) + b"\x00" * 12
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv), backend=default_backend())
    return cipher.decryptor().update(ct)


def read_superblock_magic(sector_bytes: bytes) -> int:
    return struct.unpack_from("<H", sector_bytes, 56)[0]


def check_image(img_path: str, key: bytes) -> tuple[bool, int]:
    with open(img_path, "rb") as f:
        f.seek(2 * SECTOR)
        ct = f.read(SECTOR)
    pt = decrypt_sector(key, 2, ct)
    magic = read_superblock_magic(pt)
    return magic == EXT2_MAGIC, magic


def decrypt_image(img_path: str, out_path: str, key: bytes, quiet: bool = False) -> None:
    size = os.path.getsize(img_path)
    total_sectors = size // SECTOR

    if not quiet:
        print(f"  {os.path.basename(img_path):20s}  {size//(1024*1024):>5} MB  {total_sectors:>7} sectors → {out_path}")

    with open(img_path, "rb") as fin, open(out_path, "wb") as fout:
        for i in range(total_sectors):
            if not quiet and i and (i % 16384) == 0:
                pct = i * 100 // total_sectors
                print(f"    {pct:3d}%  sector {i}/{total_sectors}", end="\r", flush=True)
            ct = fin.read(SECTOR)
            fout.write(decrypt_sector(key, i, ct))
        tail = fin.read()
        if tail:
            fout.write(tail)
    if not quiet:
        print(" " * 60, end="\r")

    with open(out_path, "rb") as f:
        f.seek(2 * SECTOR)
        magic = read_superblock_magic(f.read(SECTOR))
    if magic != EXT2_MAGIC:
        sys.exit(f"ERROR: decrypted {out_path} does not have ext2 magic (got 0x{magic:04X})")


def cmd_check(args: argparse.Namespace) -> int:
    img_type = args.type or auto_detect_type(args.image)
    ok, magic = check_image(args.image, KEYS[img_type])
    print(f"  {os.path.basename(args.image):20s}  type={img_type}  "
          f"magic=0x{magic:04X}  {'OK (ext2)' if ok else 'FAIL — wrong key'}")
    return 0 if ok else 2


def cmd_one(args: argparse.Namespace) -> int:
    img_type = args.type or auto_detect_type(args.image)
    out = args.output or (os.path.splitext(args.image)[0] + "_plain.img")
    decrypt_image(args.image, out, KEYS[img_type])
    print(f"  OK → {out}")
    return 0


def cmd_all(args: argparse.Namespace) -> int:
    os.makedirs(args.output_dir, exist_ok=True)
    for fname, typ in TYPE_FROM_FILENAME.items():
        # Find the file case-insensitively
        candidates = [f for f in os.listdir(args.input_dir) if f.lower() == fname]
        if not candidates:
            print(f"  SKIP   {fname}  (not found in {args.input_dir})")
            continue
        src = os.path.join(args.input_dir, candidates[0])
        dst = os.path.join(args.output_dir, candidates[0].replace(".img", "_plain.img"))
        decrypt_image(src, dst, KEYS[typ])
        print(f"  OK → {dst}")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(
        description="Offline decryptor for Kronos Mod/Eva/WaveMotion .img files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("--type", choices=["mod", "eva", "wm"],
                   help="Force image type (otherwise auto-detected from filename)")
    p.add_argument("--check", action="store_true",
                   help="Only validate the key against sector 2 (fast)")
    p.add_argument("--decrypt-all", action="store_true",
                   help="Treat args as input_dir output_dir; decrypt all 3 known images")
    p.add_argument("paths", nargs="+", help="image path(s) — see usage")

    args = p.parse_args()

    if args.decrypt_all:
        if len(args.paths) != 2:
            p.error("--decrypt-all requires <input_dir> <output_dir>")
        ns = argparse.Namespace(input_dir=args.paths[0], output_dir=args.paths[1])
        return cmd_all(ns)

    if args.check:
        if len(args.paths) != 1:
            p.error("--check takes exactly one image path")
        ns = argparse.Namespace(image=args.paths[0], type=args.type)
        return cmd_check(ns)

    if len(args.paths) not in (1, 2):
        p.error("Provide <input.img> [output.img]")
    ns = argparse.Namespace(
        image=args.paths[0],
        output=args.paths[1] if len(args.paths) == 2 else None,
        type=args.type,
    )
    return cmd_one(ns)


if __name__ == "__main__":
    sys.exit(main())
