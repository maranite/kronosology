# DisplayUpdaterMessage

A drop-in replacement for Korg's DisplayUpdaterMessage binary, distributed 
as part of firmware update packages. Pretty much just allows pretar.sh and posttar.sh
to display messages to the user during an OS update. 

## Usage

DisplayUpdaterMessage "SetTextPalette"      <- Prepares update screen for custom text messages 
DisplayUpdaterMessage "[custom message]"    <- displays your message on the Update screen
DisplayUpdaterMessage "SetDefaultPalette"   <- Reverts update screen to default text

## Building

This source requires `gcc-multilib` and `libfreetype6-dev:i386`.
Install them with `apt-get install -y gcc-multilib ibfreetype6-dev:i386`

To build on Debian/Ubuntu:
```bash
cd auto-auth/updater_msg
make
# Produces: DisplayUpdaterMessage  (i386 ELF, GLIBC 2.0 only, ~18 KB stripped)
```

The binary requires only `GLIBC_2.0`, making it compatible with the Kronos's
older glibc.  Compiler flags `-fno-stack-protector -D_FORTIFY_SOURCE=0 -no-pie`
and a `--wrap=__libc_start_main` shim in `glibc_compat.c` prevent modern
toolchain defaults from pulling in newer symbol versions.

