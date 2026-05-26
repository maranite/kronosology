# OmapNKS4 panel-chip wedge — operational note

The Korg Kronos's front-panel controller (`OmapNKS4Module.ko` driver, USB VID:PID `0944:1005`) gets stuck in a state where USB enumeration succeeds (`Probe() found: vendor 0x944, product 0x1005`, `probe success`) but the proprietary protocol comm-check times out:

```
OmapNKS4:WaitForNKS4ReadEvent: line 1029: WaitForNKS4ReadEvent() timed out
OmapNKS4:CommunicationCheck: line 208: Comm check - bad response, sent 0x00ee0000, rcvd 0x00000000
OmapNKS4:OmapNKS4Init: Problem configuring OmapNKS4 in Init
```

**Why:** Once wedged, the chip rejects the protocol-level handshake despite USB-level enumeration working. Module init fails, no front-panel = boot fails (ShowReauthScreen).

**How to apply:**
- Soft reboots (`reboot`, `reboot -f`) DO NOT clear the wedge. The chip's firmware state survives across host reboots.
- USB-level reset (sysfs `authorized` toggle, or `usb_reset_device` from a module patch) does NOT clear the wedge either.
- **Only a full power cycle of at least ~30-60 seconds with the unit unplugged reliably clears it.** Brief power switch toggle is not enough — likely capacitor-backed state.
- The wedge tends to recur after using `reboot -f` or any abnormal kernel shutdown. Normal `reboot` (clean shutdown calling module exits) may avoid the wedge.

**Detection:** After SSH-ing in:
- `cat /proc/modules | grep OmapNKS4` — if missing, chip wedged
- `cat /tmp/stgStatus` — empty (loadmod never ran) means loadoa stopped at OmapNKS4 insmod step
- `cat /korg/rw/oarc.log` (if logging was enabled in OA.clonos.rc) — shows `Insmod /sbin/OmapNKS4Module.ko failed`

**Operational guidance:**
- Don't `reboot -f` to test a patch unless you're willing to risk the wedge.
- After deploying patched files, prefer asking the user to do a full power cycle rather than SSH-reboot.
- If hot-swap testing via rmmod/insmod, beware: see `oa-ko-hot-swap-bug` — OA.ko's cleanup leaks `/proc/.shm` which causes kernel oops on next file-descriptor close (often `sh` exiting).
