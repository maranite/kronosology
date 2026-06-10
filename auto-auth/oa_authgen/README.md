# oa_authgen

Kernel module to communicate with the ATMEL security chip and calculate 
authorization strings for EXs options.

## Building

Requires the Kronos kernel source tree (patched 2.6.32 from
[cgudrian/linux-kronos](https://github.com/cgudrian/linux-kronos)) and an
i386-capable compiler:

```bash
cd auto-auth/oa_authgen
make KDIR=/tmp/linux-kronos        # host must be i386/i686, or have 32-bit multilib
# Produces: oa_authgen.ko  (~24 KB)
```

> **Kernel tree prerequisite:** run `make ARCH=i386 oldconfig` and
> `make ARCH=i386 prepare scripts` in the kernel tree once before building
> external modules against it.
