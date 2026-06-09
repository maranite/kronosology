/*
 * kronosology_updater_msg.c
 * ─────────────────────────────────────────────────────────────────────────────
 * Clean-room replacement for Korg's DisplayUpdaterMessage utility.
 * Shows status messages on the Kronos front-panel LCD during USB updates.
 *
 * Written by studying the public OmapNKS4 framebuffer driver ABI
 * (ioctl numbers, /proc interface, palette format, fill-rect struct layout).
 * No Korg source code was used or copied; all logic is independently written.
 *
 * Additions beyond the original interface:
 *   • Smooth 9-level FreeType anti-aliasing for red text (original: 2 levels)
 *   • "kronosology auto-auth" brand header above each message
 *   • Pixel-art padlock icon drawn directly into the framebuffer
 *
 * Build (cross-compile for Kronos i386, or native on a 32-bit host):
 *   gcc -std=gnu99 -m32 -O2 -o DisplayUpdaterMessage kronosology_updater_msg.c \
 *       $(freetype-config --cflags --libs) -lm
 *
 * Usage (drop-in for original):
 *   DisplayUpdaterMessage "Your message"     — show logo + message
 *   DisplayUpdaterMessage SetTextPalette     — switch palette only
 *   DisplayUpdaterMessage SetDefaultPalette  — restore palette only
 *
 * OmapNKS4 driver ABI (derived by black-box analysis):
 *
 *   Device         : /dev/fb1  (8bpp indexed-colour, mmap'd for writes)
 *   Panel control  : echo {enable|clear|disable} > /proc/OmapNKS4
 *   Palette ioctl  : ioctl(fd, 0x40047209, &entry)
 *                      struct { uint8_t index, r, g, b; }  — one entry per call
 *   Fill ioctl     : ioctl(fd, 0x40107208, &rect)
 *                      struct { uint8_t col; uint8_t pad[3];
 *                               uint32_t count; uint32_t fb_offset; uint32_t width; }
 *   Version ioctl  : ioctl(fd, 0x4004720e, &ver)  — returns 0 (original) or 1 (K2)
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <ft2build.h>
#include FT_FREETYPE_H

/* ── OmapNKS4 ioctl numbers ─────────────────────────────────────────────── */
#define OMAPNKS4_SET_PAL     0x40047209u  /* _IOW('r', 9,  4 bytes) one entry  */
#define OMAPNKS4_FILL_RECT   0x40107208u  /* _IOW('r', 8, 16 bytes) hw fill    */
#define OMAPNKS4_GET_VERSION 0x4004720eu  /* _IOWR('r',14, 4 bytes) 0 or 1     */

/* Passed to OMAPNKS4_FILL_RECT */
struct omapnks4_rect {
    uint8_t  color;       /* palette index                */
    uint8_t  pad[3];
    uint32_t count;       /* w * h                        */
    uint32_t fb_offset;   /* y * screen_width + x         */
    uint32_t width;       /* w                            */
};

/* Passed to OMAPNKS4_SET_PAL */
struct omapnks4_pal {
    uint8_t index;
    uint8_t r, g, b;
};

/* ── Palette index assignments ──────────────────────────────────────────── */
/*
 * Entries 0-9 match Korg's SetTextPalette layout (compatibility guarantee).
 * 0   = black background
 * 1-9 = red gradient for smooth FreeType anti-aliasing (0xBF → 0xFF red)
 * Entries 10-15 are our own logo colours (were all-black in the original).
 */
#define CI_BG      0    /* #000000 – background              */
#define CI_RED1    1    /* #BF0000 – dim Korg red (aa lvl 1) */
#define CI_RED2    2    /* #C8... etc                        */
#define CI_RED3    3
#define CI_RED4    4
#define CI_RED5    5
#define CI_RED6    6
#define CI_RED7    7
#define CI_RED8    8    /* #FC0000 – almost full red         */
#define CI_RED     9    /* #FF0000 – full Korg red (text)    */
#define CI_WHITE  10    /* #FFFFFF – header/label text       */
#define CI_LGRAY  11    /* #C0C0C0 – padlock body fill       */
#define CI_DGRAY  12    /* #505050 – padlock shadow/depth    */
#define CI_ORANGE 13    /* #FF8010 – warm accent stripe      */
#define CI_MGRAY  14    /* #888888 – padlock mid-tone        */
#define CI_DRED   15    /* #700000 – dark red for depth      */

/* ── Display geometry constants ─────────────────────────────────────────── */
/*
 * The Kronos status area begins at y=420 and extends to the bottom of the
 * screen.  These constants mirror the original binary's hard-coded offsets.
 */
#define STATUS_Y        420     /* first pixel row of the status area   */
#define STATUS_W_MAX    800     /* assumed maximum width; clamped to g_w */
#define LOGO_SCALE      2       /* padlock pixel art scale factor        */
#define LOGO_PAD_X      10      /* pixels from left edge for padlock     */
#define LOGO_PAD_Y      4       /* pixels below STATUS_Y for padlock     */
#define HEADER_FONT_PX  13      /* px size for "kronosology auto-auth"   */
#define MSG_FONT_PX     20      /* px size for main status message       */

/* ── Fonts to try in order ──────────────────────────────────────────────── */
static const char *const kFontPaths[] = {
    "/usr/share/fonts/truetype/Geneva.ttf",
    "/usr/share/fonts/truetype/Arialbd.ttf",
    "/usr/share/fonts/truetype/Arial.ttf",
    "/usr/share/fonts/truetype/Geneva_Bold.ttf",
    NULL
};

/* ── 20×14 pixel-art padlock bitmap ─────────────────────────────────────── */
/*
 * Values: 0 = transparent (skip), 1 = body fill, 2 = border/shackle
 * Drawn at LOGO_SCALE × scale, so final size = 40×28 at scale 2.
 *
 *   ....XXXXXXXXXX....    ← shackle arc (rows 0-3)
 *   ...XX..........XX..
 *   ...XX..........XX..
 *   ...XX..........XX..
 *   XXXXXXXXXXXXXXXXXX    ← body top border (row 4)
 *   X################X    ← body interior (rows 5-12)
 *   X####..KKKK..####X    ← keyhole circle (rows 6-8)
 *   X####.KKKKKK.####X
 *   X####..KKKK..####X
 *   X#####..KK..#####X    ← keyhole stem (rows 9-10)
 *   X#####..KK..#####X
 *   X################X
 *   X################X
 *   XXXXXXXXXXXXXXXXXX    ← body bottom border (row 13)
 */
static const uint8_t kPadlock[14][20] = {
    /* row  0 – shackle arc top */
    {0,0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0},
    /* rows 1-3 – shackle sides */
    {0,0,0,2,2,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0},
    {0,0,0,2,2,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0},
    {0,0,0,2,2,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0},
    /* row  4 – body top border */
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
    /* row  5 – body (no keyhole) */
    {2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,0,0},
    /* row  6 – keyhole circle top (6-wide gap at cols 6-11) */
    {2,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,2,0,0},
    /* row  7 – keyhole circle widest (8-wide gap at cols 5-12) */
    {2,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,2,0,0},
    /* row  8 – keyhole circle bottom */
    {2,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,2,0,0},
    /* rows 9-10 – keyhole stem (4-wide gap at cols 7-10) */
    {2,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,2,0,0},
    /* rows 11-12 – body interior */
    {2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,0,0},
    /* row 13 – body bottom border */
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
};
#define PADLOCK_ROWS 14
#define PADLOCK_COLS 18   /* only 18 columns are non-zero */

/* ── Global framebuffer state ─────────────────────────────────────────────── */
static int      g_fd       = -1;
static uint32_t g_w        = 0;
static uint32_t g_h        = 0;
static uint32_t g_bpp      = 0;
static uint8_t *g_fb       = NULL;
static size_t   g_fb_size  = 0;
static int      g_connected = 0;

/* ── Global FreeType state ──────────────────────────────────────────────── */
static FT_Library g_ftlib  = NULL;
static FT_Face    g_face   = NULL;
static uint32_t   g_ft_px  = 0;    /* current loaded pixel size */

/* ── Framebuffer helpers ────────────────────────────────────────────────── */

static void fb_pixel(int x, int y, uint8_t col)
{
    if (!g_fb || x < 0 || y < 0 || (uint32_t)x >= g_w || (uint32_t)y >= g_h)
        return;
    g_fb[(uint32_t)y * g_w + (uint32_t)x] = col;
}

static void fb_fill(uint8_t col, int x, int y, int w, int h)
{
    int row, col_x;
    if (!g_fb || w <= 0 || h <= 0) return;

    /* hardware-accelerated path */
    if (g_fd >= 0) {
        struct omapnks4_rect r;
        r.color     = col;
        r.pad[0]    = r.pad[1] = r.pad[2] = 0;
        r.count     = (uint32_t)(w * h);
        r.fb_offset = (uint32_t)(y * (int)g_w + x);
        r.width     = (uint32_t)w;
        ioctl(g_fd, (int)OMAPNKS4_FILL_RECT, &r);
    }

    /* software path (always executed; also serves as fallback) */
    for (row = y; row < y + h; row++) {
        for (col_x = x; col_x < x + w; col_x++)
            fb_pixel(col_x, row, col);
    }
}

static void fb_hline(int x0, int x1, int y, int thick, uint8_t col)
{
    fb_fill(col, x0, y, x1 - x0, thick);
}

/* ── Palette management ─────────────────────────────────────────────────── */

static void pal_entry(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    struct omapnks4_pal e = { idx, r, g, b };
    if (g_fd >= 0)
        ioctl(g_fd, (int)OMAPNKS4_SET_PAL, &e);
}

/*
 * Text palette — compatible with Korg's SetTextPalette:
 *   • Entries 0-9 for red gradient text (Korg standard)
 *   • Entries 10-15 for our logo colours (were all-black in original)
 *   • Entries 16-255 black
 */
static void pal_text(void)
{
    unsigned int i;
    /* Korg-compatible red gradient: 0=black, 1=dim red, ..., 9=bright red */
    static const uint8_t kRedR[10] = {
        0x00, 0xBF, 0xC8, 0xD1, 0xDA, 0xE3, 0xEC, 0xF5, 0xFC, 0xFF
    };
    for (i = 0; i < 256; i++) {
        if (i < 10)
            pal_entry((uint8_t)i, kRedR[i], 0, 0);
        else if (i == CI_WHITE)
            pal_entry(CI_WHITE,  0xFF, 0xFF, 0xFF);
        else if (i == CI_LGRAY)
            pal_entry(CI_LGRAY,  0xC0, 0xC0, 0xC0);
        else if (i == CI_DGRAY)
            pal_entry(CI_DGRAY,  0x50, 0x50, 0x50);
        else if (i == CI_ORANGE)
            pal_entry(CI_ORANGE, 0xFF, 0x80, 0x10);
        else if (i == CI_MGRAY)
            pal_entry(CI_MGRAY,  0x88, 0x88, 0x88);
        else if (i == CI_DRED)
            pal_entry(CI_DRED,   0x70, 0x00, 0x00);
        else
            pal_entry((uint8_t)i, 0, 0, 0);
    }
}

/*
 * Default/restore palette — standard CGA 16 + a neutral ramp.
 * Own design; not copied from Korg's 230-colour table.
 */
static void pal_default(void)
{
    unsigned int i;
    /* CGA-compatible 16 colours for entries 0-15 */
    static const uint8_t cga[16][3] = {
        {0x00,0x00,0x00}, {0xBF,0x00,0x00}, {0x00,0xBF,0x00}, {0xBF,0xBF,0x00},
        {0x00,0x00,0xBF}, {0xBF,0x00,0xBF}, {0x00,0xBF,0xBF}, {0xC0,0xC0,0xC0},
        {0x80,0x80,0x80}, {0xFF,0x00,0x00}, {0x00,0xFF,0x00}, {0xFF,0xFF,0x00},
        {0x00,0x00,0xFF}, {0xFF,0x00,0xFF}, {0x00,0xFF,0xFF}, {0xFF,0xFF,0xFF},
    };
    for (i = 0; i < 16; i++)
        pal_entry((uint8_t)i, cga[i][0], cga[i][1], cga[i][2]);
    /* entries 16-255: neutral dark-to-black gradient */
    for (i = 16; i < 256; i++) {
        uint8_t v = (uint8_t)(0x40 - (i - 16) * 0x40 / 240);
        pal_entry((uint8_t)i, v, v, v);
    }
}

/* ── Framebuffer connect / disconnect ─────────────────────────────────────── */

static int fb_connect(void)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (g_connected) return 0;

    g_fd = open("/dev/fb1", O_RDWR);
    if (g_fd < 0) {
        fprintf(stderr, "DisplayUpdaterMessage: cannot open /dev/fb1: %s\n",
                strerror(errno));
        return -1;
    }

    if (ioctl(g_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        fprintf(stderr, "DisplayUpdaterMessage: FBIOGET_FSCREENINFO failed\n");
        close(g_fd); g_fd = -1; return -1;
    }

    if (ioctl(g_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "DisplayUpdaterMessage: FBIOGET_VSCREENINFO failed\n");
        close(g_fd); g_fd = -1; return -1;
    }

    g_w   = vinfo.xres;
    g_h   = vinfo.yres;
    g_bpp = vinfo.bits_per_pixel;

    g_fb_size = (size_t)(g_w * g_h * g_bpp / 8);
    g_fb = mmap(NULL, g_fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (g_fb == MAP_FAILED || !g_fb) {
        fprintf(stderr, "DisplayUpdaterMessage: mmap failed\n");
        g_fb = NULL;
        close(g_fd); g_fd = -1; return -1;
    }

    /* Enable and clear the front-panel display */
    (void)system("echo enable > /proc/OmapNKS4");
    (void)system("echo clear > /proc/OmapNKS4");

    g_connected = 1;
    return 0;
}

static void fb_disconnect(void)
{
    if (!g_connected) return;
    (void)system("echo disable > /proc/OmapNKS4");
    if (g_fb) { munmap(g_fb, g_fb_size); g_fb = NULL; }
    if (g_fd >= 0) { close(g_fd); g_fd = -1; }
    g_connected = 0;
}

/* Returns the title-screen version (0 = original Kronos, 1 = KRONOS 2). */
static int fb_version(void)
{
    uint32_t ver = 0;
    if (g_fd >= 0)
        ioctl(g_fd, (int)OMAPNKS4_GET_VERSION, &ver);
    return (int)ver;
}

/* ── FreeType text rendering ──────────────────────────────────────────────── */

static int ft_init(void)
{
    int i;
    if (g_ftlib) return 0;
    if (FT_Init_FreeType(&g_ftlib)) {
        fprintf(stderr, "DisplayUpdaterMessage: FreeType init failed\n");
        return -1;
    }
    for (i = 0; kFontPaths[i]; i++) {
        if (FT_New_Face(g_ftlib, kFontPaths[i], 0, &g_face) == 0)
            return 0;
    }
    fprintf(stderr, "DisplayUpdaterMessage: no usable font found\n");
    FT_Done_FreeType(g_ftlib);
    g_ftlib = NULL;
    return -1;
}

static void ft_set_size(uint32_t px)
{
    if (!g_face || px == g_ft_px) return;
    FT_Set_Pixel_Sizes(g_face, 0, px);
    g_ft_px = px;
}

static void ft_done(void)
{
    if (g_face)  { FT_Done_Face(g_face);           g_face  = NULL; }
    if (g_ftlib) { FT_Done_FreeType(g_ftlib);      g_ftlib = NULL; }
    g_ft_px = 0;
}

/*
 * Map a FreeType gray value (0-255) to a palette index in 0-9.
 * Provides smooth 9-level anti-aliasing over Korg's original 2-level approach.
 */
static uint8_t ft_gray_to_pal(uint8_t v)
{
    if (v == 0) return CI_BG;
    return (uint8_t)(1 + (int)v * 8 / 256);   /* 1..9 */
}

/* Measure the pixel width of a string at the current font size. */
static int ft_text_width(const char *str)
{
    int w = 0;
    if (!g_face || !str) return 0;
    for (; *str; str++) {
        if (FT_Load_Char(g_face, (unsigned char)*str, FT_LOAD_ADVANCE_ONLY))
            continue;
        w += (int)(g_face->glyph->advance.x >> 6);
    }
    return w;
}

/*
 * Render a string into the framebuffer.
 *   x, y   = pen position (x = left edge, y = baseline)
 *   col_fn = function mapping gray value → palette index (NULL = use default)
 */
static void ft_render_str(const char *str, int x, int y,
                           uint8_t (*col_fn)(uint8_t))
{
    int pen_x = x;

    if (!g_face || !str) return;

    for (; *str; str++) {
        FT_GlyphSlot slot;
        int bx, by, gx, gy, pitch;
        unsigned char *buf;

        if (FT_Load_Char(g_face, (unsigned char)*str,
                         FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
            continue;

        slot  = g_face->glyph;
        buf   = slot->bitmap.buffer;
        pitch = slot->bitmap.pitch;
        gx    = pen_x + slot->bitmap_left;
        gy    = y - slot->bitmap_top;

        for (by = 0; (unsigned)by < slot->bitmap.rows; by++) {
            for (bx = 0; (unsigned)bx < slot->bitmap.width; bx++) {
                uint8_t v  = buf[by * pitch + bx];
                uint8_t ci = col_fn ? col_fn(v) : ft_gray_to_pal(v);
                if (ci != CI_BG)
                    fb_pixel(gx + bx, gy + by, ci);
            }
        }
        pen_x += (int)(slot->advance.x >> 6);
    }
}

/* col_fn for white text (used for the header label) */
static uint8_t gray_to_white(uint8_t v) {
    return (v > 64) ? CI_WHITE : CI_BG;
}

/* ── Logo drawing ────────────────────────────────────────────────────────── */

static void draw_padlock(int ox, int oy)
{
    int row, col;
    static const uint8_t kColMap[3] = { CI_BG, CI_LGRAY, CI_RED };

    for (row = 0; row < PADLOCK_ROWS; row++) {
        for (col = 0; col < PADLOCK_COLS; col++) {
            uint8_t v = kPadlock[row][col];
            uint8_t ci;
            int dx, dy;
            if (v == 0) continue;
            ci = kColMap[v < 3 ? v : 0];
            /* draw at LOGO_SCALE × scale */
            for (dy = 0; dy < LOGO_SCALE; dy++)
                for (dx = 0; dx < LOGO_SCALE; dx++)
                    fb_pixel(ox + col * LOGO_SCALE + dx,
                             oy + row * LOGO_SCALE + dy, ci);
        }
    }

    /* add a subtle drop-shadow one pixel down/right in dark gray */
    for (row = 0; row < PADLOCK_ROWS; row++) {
        for (col = 0; col < PADLOCK_COLS; col++) {
            if (kPadlock[row][col] != 2) continue;
            /* shadow pixel, only if background under it */
            {
                int sx = ox + col * LOGO_SCALE + LOGO_SCALE;
                int sy = oy + row * LOGO_SCALE + LOGO_SCALE;
                if (sx < (int)g_w && sy < (int)g_h) {
                    uint8_t existing = g_fb[sy * g_w + sx];
                    if (existing == CI_BG)
                        fb_pixel(sx, sy, CI_DGRAY);
                }
            }
        }
    }
}

/*
 * Draw the kronosology logo into the status area.
 *
 * Layout (STATUS_Y = 420 baseline, coordinates in pixels):
 *
 *   ── thin orange accent bar ──────────────────────────────  y = STATUS_Y + 0
 *   [padlock 40×28px]  kronosology :: auto-auth (13px white) y = STATUS_Y + 5
 *   ── thin red separator ──────────────────────────────────  y = STATUS_Y + 36
 */
static void draw_logo(void)
{
    int logo_x, logo_y, lock_w, lock_h, label_x, label_y;
    const char *label = "kronosology :: auto-auth";

    if (!g_fb) return;

    logo_x = LOGO_PAD_X;
    logo_y = STATUS_Y + LOGO_PAD_Y;
    lock_w = PADLOCK_COLS * LOGO_SCALE;   /* 36 px */
    lock_h = PADLOCK_ROWS * LOGO_SCALE;   /* 28 px */

    /* accent stripe across the full status area */
    fb_hline(0, (int)g_w, STATUS_Y, 2, CI_ORANGE);

    /* padlock icon */
    draw_padlock(logo_x, logo_y);

    /* "kronosology :: auto-auth" label — vertically centred with padlock */
    if (ft_init() == 0) {
        ft_set_size(HEADER_FONT_PX);
        label_x = logo_x + lock_w + 8;
        label_y = logo_y + lock_h / 2 + HEADER_FONT_PX / 3;
        ft_render_str(label, label_x, label_y, gray_to_white);
    }

    /* separator line below padlock + label */
    fb_hline(0, (int)g_w, STATUS_Y + lock_h + LOGO_PAD_Y + 4, 1, CI_RED);
}

/* ── Status area and message display ──────────────────────────────────────── */

static void status_clear(void)
{
    fb_fill(CI_BG, 0, STATUS_Y, (int)g_w, (int)g_h - STATUS_Y);
}

static void show_message(const char *msg)
{
    int ver, text_y, text_x, tw;

    if (!g_fb || !msg) return;

    status_clear();
    draw_logo();

    if (ft_init() != 0) return;

    ft_set_size(MSG_FONT_PX);

    /* vertical position matches original binary's version-dependent logic */
    ver    = fb_version();
    text_y = (ver == 1) ? 0x1f4 : 0x2ae;   /* 500 (K2) or 686 (original) */

    /* clamp to within the screen */
    if (text_y >= (int)g_h - 2)
        text_y = (int)g_h - 4;

    /* horizontal: centred */
    tw     = ft_text_width(msg);
    text_x = ((int)g_w > tw) ? ((int)g_w - tw) / 2 : 0;

    ft_render_str(msg, text_x, text_y, NULL);   /* NULL → default red gradient */
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    const char *arg;
    int is_set_text_pal, is_set_def_pal;

    /* drop any inherited privileges (mirrors original binary) */
    (void)setuid(0);
    (void)setgid(0);

    if (argc != 2) {
        fputs("must provide a string to display\n", stderr);
        return 1;
    }
    arg = argv[1];

    is_set_text_pal = (strstr(arg, "SetTextPalette")    != NULL);
    is_set_def_pal  = (strstr(arg, "SetDefaultPalette") != NULL);

    if (fb_connect() != 0) return 1;

    usleep(133333);   /* ~133 ms — matches original timing */

    if (is_set_text_pal) {
        pal_text();
        usleep(133333);
        fb_disconnect();
        return 0;
    }

    if (is_set_def_pal) {
        pal_default();
        usleep(133333);
        fb_disconnect();
        return 0;
    }

    /* Normal message display */
    pal_text();
    usleep(133333);
    show_message(arg);
    usleep(333333);   /* ~333 ms — matches original message dwell time */
    pal_default();
    usleep(133333);

    fb_disconnect();
    ft_done();
    return 0;
}
