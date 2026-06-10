# auto-auth — Automatic EXs Authorisation for Korg Kronos
Authorises all installed EX libraries without writing anything to the Kronos
internal disk. It generates device-specific authorization strings for [EXs options](https://korg.shop/sound-libraries/kronos-nautilus.html) that will be accepted by unmodified units, so future Korg OS updates keep working.

A kernel module is loaded from the USB stick, generates the
auth strings, writes them to `/korg/rw/Startup/AuthorizationStrings`, then
unloads itself.  Nothing else changes.

Best for: a stock Kronos that has EX libraries already installed and just needs the auth strings populated.

## Published EXs Option download URLs
- [EXs12 SGX-1 Austrian Piano](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs12/K2_EXs12.zip)
- [EXs21 SGX-2 Italian F Piano](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs21/K2_EXs21.zip)
- [EXs22 SGX-2 Italian F Piano LE](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs22/K2_EXs22.zip)
- [EXs23 2 Church Pianos](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs23/K2_EXs23.zip)

- [EXs59 Stage Piano](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs59/K2_EXs59.zip)
- [EXs60 Electric Grand](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs60/K2_EXs60.zip)
- [EXs67 Upright Piano](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs67/EXs67.zip)

- [EXs147 Acousticsamples C7 Grand Close](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs147/K2_EXs147.zip)
- [EXs148 Acousticsamples C7 Grand Player](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs148/K2_EXs148.zip">)
- [EXs150 Acousticsamples B-Piano](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs150/K2_EXs150.zip)
- [EXs157 MKS-EP](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs157/K2_EXs157.zip)
- [EXs165 Modern Grand](https://storage.korg.com/korgms/sound_libraries/Kronos_Nautilus/EXs165/EXs165.zip)

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


