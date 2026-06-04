/* getloopkey.s  — i386 Linux, raw syscalls only, no libc
 *
 * Usage: getloopkey /dev/loopN
 *
 * Calls LOOP_GET_STATUS64 (ioctl 0x4C05) on the given loop device and prints:
 *   backing_file cipher_name encrypt_type key_size key_hex
 * to stdout, one field per line.
 *
 * struct loop_info64 layout (kernel 2.6.x, total 232 bytes):
 *   +0   lo_device        u64
 *   +8   lo_inode         u64
 *  +16   lo_rdevice       u64
 *  +24   lo_offset        u64
 *  +32   lo_sizelimit     u64
 *  +40   lo_number        u32
 *  +44   lo_encrypt_type  u32
 *  +48   lo_encrypt_key_size u32
 *  +52   lo_flags         u32
 *  +56   lo_file_name     [64]
 * +120   lo_crypt_name    [64]
 * +184   lo_encrypt_key   [32]
 * +216   lo_init[2]       u64*2
 */

.code32
.section .bss
    .lcomm buf, 232         /* loop_info64 struct */
    .lcomm hexbuf, 72       /* hex string for key (32 bytes * 2 + newline) */

.section .rodata
label_file:     .asciz "Backing file : "
label_cipher:   .asciz "Cipher       : "
label_type:     .asciz "Encrypt type : "
label_keysize:  .asciz "Key size     : "
label_key:      .asciz "Key (hex)    : "
newline:        .byte 10
digits:         .ascii "0123456789abcdef"
err_usage:      .ascii "Usage: getloopkey /dev/loopN\n"
err_usage_len = . - err_usage
err_open:       .ascii "Error: cannot open device\n"
err_open_len  = . - err_open
err_ioctl:      .ascii "Error: ioctl failed (not a loop device, or not root?)\n"
err_ioctl_len = . - err_ioctl

.section .text
.global _start

/* sys_write(1, ptr, len) */
.macro WRITE ptr, len
    movl $4, %eax
    movl $1, %ebx
    leal \ptr, %ecx
    movl $\len, %edx
    int  $0x80
.endm

/* sys_write(1, ptr, len) where len is in %edx, ptr in %ecx */
.macro WRITE_REG
    movl $4, %eax
    movl $1, %ebx
    int  $0x80
.endm

/* sys_exit(code) */
.macro EXIT code
    movl $1, %eax
    movl $\code, %ebx
    int  $0x80
.endm

_start:
    /* argc is at (%esp), argv[0] at 4(%esp), argv[1] at 8(%esp) */
    movl (%esp), %ecx           /* argc */
    cmpl $2, %ecx
    jge  .have_arg
    WRITE err_usage, err_usage_len
    EXIT 1

.have_arg:
    movl 8(%esp), %esi          /* argv[1] = device path */

    /* open(argv[1], O_RDONLY=0) */
    movl $5, %eax               /* sys_open */
    movl %esi, %ebx
    movl $0, %ecx               /* O_RDONLY */
    movl $0, %edx
    int  $0x80
    testl %eax, %eax
    js   .err_open
    movl %eax, %edi             /* fd in %edi */

    /* ioctl(fd, LOOP_GET_STATUS64, &buf) */
    movl $54, %eax              /* sys_ioctl */
    movl %edi, %ebx
    movl $0x4C05, %ecx          /* LOOP_GET_STATUS64 */
    leal buf, %edx
    int  $0x80
    testl %eax, %eax
    js   .err_ioctl

    /* close(fd) */
    movl $6, %eax
    movl %edi, %ebx
    int  $0x80

    /* Print "Backing file : " + lo_file_name (null-terminated, +56) */
    WRITE label_file, 15
    leal buf+56, %ecx
    call print_cstr_newline

    /* Print "Cipher       : " + lo_crypt_name (null-terminated, +120) */
    WRITE label_cipher, 15
    leal buf+120, %ecx
    call print_cstr_newline

    /* Print "Encrypt type : " + lo_encrypt_type as decimal (+44) */
    WRITE label_type, 15
    movl buf+44, %eax
    call print_u32_newline

    /* Print "Key size     : " + lo_encrypt_key_size as decimal (+48) */
    WRITE label_keysize, 15
    movl buf+48, %eax
    call print_u32_newline

    /* Print "Key (hex)    : " + hex of lo_encrypt_key (lo_encrypt_key_size bytes, +184) */
    WRITE label_key, 15
    movl buf+48, %ecx           /* key_size */
    testl %ecx, %ecx
    jz   .print_newline
    leal buf+184, %esi          /* src: key bytes */
    leal hexbuf, %edi           /* dst: hex string */
    leal digits, %ebp
    movl %ecx, %edx             /* save key_size */
.hexloop:
    xorl %eax, %eax
    movb (%esi), %al
    incl %esi
    movl %eax, %ebx
    shrl $4, %ebx               /* high nibble */
    movb (%ebp,%ebx), %bl
    movb %bl, (%edi)
    incl %edi
    andl $0xF, %eax             /* low nibble */
    movb (%ebp,%eax), %al
    movb %al, (%edi)
    incl %edi
    decl %ecx
    jnz  .hexloop
    movb $10, (%edi)            /* newline */
    incl %edi
    /* write hexbuf */
    movl $4, %eax
    movl $1, %ebx
    leal hexbuf, %ecx
    movl %edx, %edx             /* key_size */
    leal hexbuf, %ecx
    /* length = key_size*2 + 1 */
    movl %edx, %edx
    leal (%edx,%edx), %edx
    incl %edx
    WRITE_REG

    EXIT 0

.print_newline:
    WRITE newline, 1
    EXIT 0

.err_open:
    WRITE err_open, err_open_len
    EXIT 2

.err_ioctl:
    WRITE err_ioctl, err_ioctl_len
    EXIT 3

/* print_cstr_newline: print null-terminated string at %ecx, then newline */
print_cstr_newline:
    pushl %esi
    movl %ecx, %esi
    movl %ecx, %edx
.pcsl_len:
    cmpb $0, (%edx)
    je   .pcsl_done
    incl %edx
    jmp  .pcsl_len
.pcsl_done:
    subl %ecx, %edx             /* length */
    WRITE_REG
    WRITE newline, 1
    popl %esi
    ret

/* print_u32_newline: print %eax as decimal then newline */
print_u32_newline:
    pushl %edi
    subl $12, %esp
    movl %esp, %edi
    addl $11, %edi
    movb $10, (%edi)            /* newline at end */
    decl %edi
    movl %eax, %eax
    testl %eax, %eax
    jnz  .pu32_loop
    movb $48, (%edi)            /* "0" */
    decl %edi
    jmp  .pu32_done
.pu32_loop:
    testl %eax, %eax
    jz   .pu32_done
    movl $10, %ecx
    xorl %edx, %edx
    divl %ecx
    addb $48, %dl
    movb %dl, (%edi)
    decl %edi
    jmp  .pu32_loop
.pu32_done:
    incl %edi
    /* length = esp+12 - edi */
    movl %esp, %edx
    addl $12, %edx
    subl %edi, %edx
    movl %edi, %ecx
    WRITE_REG
    addl $12, %esp
    popl %edi
    ret
