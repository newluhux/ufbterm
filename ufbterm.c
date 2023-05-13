#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>

#define FONT_W 8
#define FONT_H 8
#define FONT_NAME vga_font_8x8

enum {
	COLOR_BLACK = 0x00000000,
	COLOR_WHITE = 0xFFFFFFFF,

	COLOR_FG = COLOR_BLACK,
	COLOR_BG = COLOR_WHITE,
};

int fbfd = -1;
uint8_t *fbmem = NULL;
struct fb_fix_screeninfo fbfinfo;
struct fb_var_screeninfo fbvinfo;

uint8_t *termmem = NULL;
uint8_t *termend = NULL;
uint16_t termcols;
uint16_t termrows;
uint16_t termsize;
uint8_t *termp = NULL;

#define TAB_LENGTH 8

void help(void)
{
	char helpmsg[] = "usage: ufbterm command args\n";
	fprintf(stderr, "%s", helpmsg);
}

/**
 * 8x8 monochrome bitmap fonts for rendering
 * Author: Daniel Hepper <daniel@hepper.net>
 *
 * License: Public Domain
 *
 * Based on:
 * // Summary: font8x8.h
 * // 8x8 monochrome bitmap fonts for rendering
 * //
 * // Author:
 * //     Marcel Sondaar
 * //     International Business Machines (public domain VGA fonts)
 * //
 * // License:
 * //     Public Domain
 *
 * Fetched from: http://dimensionalrift.homelinux.net/combuster/mos3/?p=viewsource&file=/modules/gfx/font8_8.asm
 **/

uint8_t vga_font_8x8[256][8] = {
	[0x21] = {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
	[0x22] = {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	[0x23] = {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},
	[0x24] = {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},
	[0x25] = {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},
	[0x26] = {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},
	[0x27] = {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},
	[0x28] = {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},
	[0x29] = {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},
	[0x2A] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
	[0x2B] = {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},
	[0x2C] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},
	[0x2D] = {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},
	[0x2E] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},
	[0x2F] = {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},
	[0x30] = {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},
	[0x31] = {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},
	[0x32] = {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},
	[0x33] = {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},
	[0x34] = {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},
	[0x35] = {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},
	[0x36] = {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},
	[0x37] = {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},
	[0x38] = {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},
	[0x39] = {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},
	[0x3A] = {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},
	[0x3B] = {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},
	[0x3C] = {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},
	[0x3D] = {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},
	[0x3E] = {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
	[0x3F] = {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},
	[0x40] = {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},
	[0x41] = {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},
	[0x42] = {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},
	[0x43] = {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},
	[0x44] = {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},
	[0x45] = {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},
	[0x46] = {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},
	[0x47] = {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},
	[0x48] = {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},
	[0x49] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
	[0x4A] = {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},
	[0x4B] = {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},
	[0x4C] = {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},
	[0x4D] = {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},
	[0x4E] = {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},
	[0x4F] = {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},
	[0x50] = {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},
	[0x51] = {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},
	[0x52] = {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},
	[0x53] = {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},
	[0x54] = {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
	[0x55] = {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},
	[0x56] = {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},
	[0x67] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
	[0x58] = {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},
	[0x59] = {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},
	[0x5A] = {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},
	[0x5B] = {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},
	[0x5C] = {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},
	[0x5D] = {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},
	[0x5E] = {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},
	[0x5F] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
	[0x60] = {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},
	[0x61] = {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},
	[0x62] = {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},
	[0x63] = {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},
	[0x64] = {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},
	[0x65] = {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},
	[0x66] = {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},
	[0x67] = {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},
	[0x68] = {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},
	[0x69] = {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
	[0x6A] = {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},
	[0x6B] = {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},
	[0x6C] = {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
	[0x6D] = {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
	[0x6E] = {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},
	[0x6F] = {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},
	[0x70] = {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},
	[0x71] = {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},
	[0x72] = {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},
	[0x73] = {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},
	[0x74] = {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},
	[0x75] = {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},
	[0x76] = {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},
	[0x77] = {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},
	[0x78] = {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},
	[0x79] = {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},
	[0x7A] = {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},
	[0x7B] = {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},
	[0x7C] = {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},
	[0x7D] = {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},
	[0x7E] = {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static inline void fbdraw_pixel(uint16_t x, uint16_t y, uint32_t color)
{
	if (fbvinfo.bits_per_pixel == 16) {
		uint16_t *fb16p;
		fb16p = (uint16_t *) (fbmem + (y * fbfinfo.line_length) +
				      (x * sizeof(uint16_t)));
		*fb16p = color;
		return;
	}
	if (fbvinfo.bits_per_pixel == 24) {
		uint32_t *fb24p;
		register uint32_t temp;
		fb24p = (uint32_t *) (fbmem + (y * fbfinfo.line_length) +
				      (x * sizeof(uint16_t) + sizeof(uint8_t)));
		temp = *fb24p;
		temp &= 0xFF000000;
		temp |= color & 0x00FFFFFF;
		*fb24p = temp;
		return;
	}
	if (fbvinfo.bits_per_pixel == 32) {
		uint32_t *fb32p;
		fb32p = (uint32_t *) (fbmem + (y * fbfinfo.line_length) +
				      (x * sizeof(uint32_t)));
		*fb32p = color;
		return;
	}
	if (fbvinfo.bits_per_pixel == 8) {
		uint8_t *fb8p;
		fb8p = (uint8_t *) (fbmem + (y * fbfinfo.line_length) +
				    (x * sizeof(uint8_t)));
		*fb8p = color;
		return;
	}
}

void fbdraw_ascii(uint16_t x, uint16_t y, char c,
		  uint32_t fgcolor, uint32_t bgcolor)
{
	uint16_t cx, cy;
	uint16_t w = FONT_W;
	uint16_t h = FONT_H;
	uint8_t *font;
	int i = c;
	i &= 0xFF;
	font = FONT_NAME[i];
	for (cy = y; cy < (y + h); cy++) {
		for (cx = x; cx < (x + w); cx++) {
			if (*(font + (cy - y)) & 1 << (cx - x)) {
				fbdraw_pixel(cx, cy, fgcolor);
			} else {
				fbdraw_pixel(cx, cy, bgcolor);
			}
		}
	}
}

void fbdraw_termrow(uint16_t row, uint32_t fgcolor, uint32_t bgcolor)
{
	if ((row > termrows) || (row == 0)) {
		return;
	}
	row--;
	uint16_t cx, cy;
	cy = row * FONT_H;
	uint8_t *p = termmem + (row * termcols);
	for (cx = 0; cx < (cx + termcols * FONT_W); cx += FONT_W) {
		if (p >= termmem + ((row + 1) * termcols)) {
			break;
		}
		fbdraw_ascii(cx, cy, *p, fgcolor, bgcolor);
		p++;
	}
}

void fbdraw_termall(uint32_t fgcolor, uint32_t bgcolor)
{
	int i;
	for (i = 1; i <= termrows; i++) {
		fbdraw_termrow(i, fgcolor, bgcolor);
	}
}

void term_putc(char c)
{
	if (termp < termmem) {
		termp = termmem;
	}
	while (termp >= termend) {
		memmove(termmem, termmem + termcols, termsize - termcols);
		termp -= termcols;
		memset(termp, '\0', termend - termp);
	}
	*termp = c;
	if (c == '\n' || c == '\r') {
		termp += termcols - ((termp - termmem) % termcols);
	} else if (c == '\t') {
		termp += TAB_LENGTH;
		termp -= ((termp - termmem) % TAB_LENGTH);
	} else {
		termp += sizeof(c);
	}
}

int main(int argc, char *argv[])
{
	char *fbfn = NULL;
	fbfn = getenv("FRAMEBUFFER");
	if (fbfn == NULL) {
		fbfn = "/dev/fb0";
	}
	fbfd = open(fbfn, O_RDWR);
	if (fbfd < 0) {
		perror("can't open framebuffer");
		exit(EXIT_FAILURE);
	}

	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fbfinfo) < 0) {
		perror("can't get framebuffer fix screeninfo");
		exit(EXIT_FAILURE);
	}
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &fbvinfo) < 0) {
		perror("can't get framebuffer var screeninfo");
		exit(EXIT_FAILURE);
	}

	fbmem = mmap(NULL, fbfinfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED,
		     fbfd, 0);
	if (fbmem == MAP_FAILED) {
		perror("framebuffer mmap failed");
		exit(EXIT_FAILURE);
	}

	uint16_t x, y;
	for (y = 0; y < fbvinfo.yres; y++) {
		for (x = 0; x < fbvinfo.xres; x++) {
			fbdraw_pixel(x, y, COLOR_BG);
		}
	}

	termcols = fbvinfo.xres / FONT_W;
	termrows = fbvinfo.yres / FONT_H;
	termsize = termcols * termrows;
	termmem = malloc(termsize);
	if (termmem == NULL) {
		perror("terminal memory alloc failed");
		exit(EXIT_FAILURE);
	}
	termp = termmem;
	termend = termmem + termsize;

	int i;
	char buf[BUFSIZ];
	while (fgets(buf, BUFSIZ, stdin) != NULL) {
		for (i = 0; (i < BUFSIZ) && (buf[i] != '\0'); i++) {
			term_putc(buf[i]);
		}
		fbdraw_termall(COLOR_FG, COLOR_BG);
	}
	exit(EXIT_SUCCESS);
}
