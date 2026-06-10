# auto-auth — Automatic EXs Authorisation for Korg Kronos

Authorises all installed EX libraries without writing anything to the Kronos
internal disk. It generates device-specific authorization strings for [EXs options](https://korg.shop/sound-libraries/kronos-nautilus.html) that will be accepted by unmodified units, so future Korg OS updates keep working.

A kernel module is loaded from the USB stick, generates the
auth strings, writes them to `/korg/rw/Startup/AuthorizationStrings`, then
unloads itself.  Nothing else changes.

Best for: a stock Kronos that has EX libraries already installed and just needs the auth strings populated.

## Usage

Run the builder to create the required output directory:

```bash
python3 build_auto_auth.py
```

Produces this output directory:

```
output/auto-auth/
├── install.info           ← signed package metadata
├── auth.tar.gz            ← stamp payload (just drops /tmp/auth.stamp)
├── pretar.sh              ← pre-flight check
├── posttar.sh             ← loads ko, writes auth strings, rmmod
└── DisplayUpdaterMessage  ← on-screen progress binary (if built)
```

Format a USB stick using the Kronos disk utility, thrn copy the contents to the root of the USB stick:

```sh
cp -r output/auto-auth/* /media/your-usb-stick/
sync
```

Then insert into the Kronos, trigger **Global → OS Update**.  
Voiala!.. all options will show as authorized.


## How it works

The zero-footprint USB mode bypasses OA.ko entirely and writes directly to
`/korg/rw/Startup/AuthorizationStrings`, which OA.ko reads at the next normal boot.

## Power-cycle warning

After using the USB authoriser, **power-cycle** the Kronos (full power-off ≥ 60 s,
then on). See
[`../docs/modules/OmapNKS4Module.ko_chip_wedge.md`](../docs/modules/OmapNKS4Module.ko_chip_wedge.md).
