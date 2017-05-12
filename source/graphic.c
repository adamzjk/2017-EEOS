#include "bootpack.h"

extern int *fat;
unsigned short table_8_565[256];

void init_palette(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	static unsigned char table_rgb[28 * 3] = {
		0x00, 0x00, 0x00,	
		0xff, 0x00, 0x00,	
		0x00, 0xff, 0x00,	
		0xff, 0xff, 0x00,	
		0x00, 0x00, 0xff,	
		0xff, 0x00, 0xff,	
		0x00, 0xff, 0xff,	
		0xff, 0xff, 0xff,	
		0xc6, 0xc6, 0xc6,	
		0x84, 0x00, 0x00,	
		0x00, 0x84, 0x00,	
		0x84, 0x84, 0x00,	
		0x00, 0x00, 0x84,	
		0x84, 0x00, 0x84,	
		0x00, 0x84, 0x84,	
		0x84, 0x84, 0x84,	
		0x33, 0x66, 0x66,
		0x3D, 0x78, 0x78,
		0x40, 0x80, 0x80,
		0x4F, 0x9D, 0x9D,
		0x5C, 0xAD, 0xAD,
		0x6F, 0xB7, 0xB7,
		0x81, 0xC0, 0xC0,
		0x95, 0xCA, 0xCA,
		0xA3, 0xD1, 0xD1,
		0xB3, 0xD9, 0xD9,
		0xC4, 0xE1, 0xE1,
		0xD1, 0xE9, 0xE9
	};
	int i, r, g, b;
	if (binfo->vmode == 8) {
		for (i = 0; i < 28; i++) {
			table_8_565[i] = i;
		}
		set_palette(0, 27, table_rgb);
	} else {
		for (i = 0; i < 28; i++) {
			r = table_rgb[i * 3 + 0];
			g = table_rgb[i * 3 + 1];
			b = table_rgb[i * 3 + 2];
			table_8_565[i] = (unsigned short) (((r << 8) & 0xf800) |
							 ((g << 3) & 0x07e0) | (b >> 3));	
		}
	}
	if (binfo->vmode == 8) {
		unsigned char table2[216 * 3];
		for (b = 0; b < 6; b++) {
			for (g = 0; g < 6; g++) {
				for (r = 0; r < 6; r++) {
					table2[(r + g * 6 + b * 36) * 3 + 0] = r * 51;
					table2[(r + g * 6 + b * 36) * 3 + 1] = g * 51;
					table2[(r + g * 6 + b * 36) * 3 + 2] = b * 51;
					table_8_565[r + g * 6 + b * 36 + 28] = r + g * 6 + b * 36 + 28;
				}
			}
		}
		set_palette(28, 243, table2);
	} else {
		for (b = 0; b < 6; b++) {
			for (g = 0; g < 6; g++) {
				for (r = 0; r < 6; r++) {
					table_8_565[r + g * 6 + b * 36 + 28] =
						(unsigned short) ((((r * 51) << 8) & 0xf800) |
						(((g * 51) << 3) & 0x07e0) | ((b * 51) >> 3));

				}
			}
		}
	}
	return;
}

void set_palette(int start, int end, unsigned char *rgb)
{
	int i, eflags;
	eflags = io_load_eflags();	
	io_cli(); 					
	io_out8(0x03c8, start);
	for (i = start; i <= end; i++) {
		io_out8(0x03c9, rgb[0] / 4);
		io_out8(0x03c9, rgb[1] / 4);
		io_out8(0x03c9, rgb[2] / 4);
		rgb += 3;
	}
	io_store_eflags(eflags);	
	return;
}

void boxfill8(unsigned short *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1)
{
	int x, y;
	for (y = y0; y <= y1; y++) {
		for (x = x0; x <= x1; x++)
			vram[y * xsize + x] = table_8_565[c];
	}
	return;
}

void init_screen8(short *vram, int x, int y, char *fname)
{
	boxfill8(vram, x, COL8_000000, 0, 0, x -  1, y - 29);
	read_picture(fat, vram, x, y, fname);
	return;
}

void putfont8(short *vram, int xsize, int x, int y, unsigned char c, char *font)
{
	int i;
	short *p;
	char d;
	for (i = 0; i < 16; i++) {
		p = vram + (y + i) * xsize + x;
		d = font[i];
		if ((d & 0x80) != 0) { p[0] = table_8_565[c]; }
		if ((d & 0x40) != 0) { p[1] = table_8_565[c]; }
		if ((d & 0x20) != 0) { p[2] = table_8_565[c]; }
		if ((d & 0x10) != 0) { p[3] = table_8_565[c]; }
		if ((d & 0x08) != 0) { p[4] = table_8_565[c]; }
		if ((d & 0x04) != 0) { p[5] = table_8_565[c]; }
		if ((d & 0x02) != 0) { p[6] = table_8_565[c]; }
		if ((d & 0x01) != 0) { p[7] = table_8_565[c]; }
	}
	return;
}

void putfonts8_asc_pre(short *vram, int xsize, int x, int y, unsigned char c, unsigned char *s)
{
	extern char hankaku[4096];
	for (; *s != 0x00; s++) {
		putfont8(vram, xsize, x, y, c, hankaku + *s * 16);
		x += 8;
	}
	return;
}

void putfonts8_asc(short *vram, int xsize, int x, int y,unsigned char c, unsigned char *s)
{
	extern char hankaku[4096];
	struct TASK *task = task_now();
	char *nihongo = (char *) *((int *) 0x0fe8), *font;
	int k, t;

	if (task->langmode == 0) {
		for (; *s != 0x00; s++) {
			putfont8(vram, xsize, x, y, c, hankaku + *s * 16);
			x += 8;
		}
	}
	if (task->langmode == 1) {
		for (; *s != 0x00; s++) {
			if (task->langbyte1 == 0) {
				if ((0x81 <= *s && *s <= 0x9f) || (0xe0 <= *s && *s <= 0xfc)) {
					task->langbyte1 = *s;
				} else {
					putfont8(vram, xsize, x, y, c, nihongo + *s * 16);
				}
			} else {
				if (0x81 <= task->langbyte1 && task->langbyte1 <= 0x9f) {
					k = (task->langbyte1 - 0x81) * 2;
				} else {
					k = (task->langbyte1 - 0xe0) * 2 + 62;
				}
				if (0x40 <= *s && *s <= 0x7e) {
					t = *s - 0x40;
				} else if (0x80 <= *s && *s <= 0x9e) {
					t = *s - 0x80 + 63;
				} else {
					t = *s - 0x9f;
					k++;
				}
				task->langbyte1 = 0;
				font = nihongo + 256 * 16 + (k * 94 + t) * 32;
				putfont8(vram, xsize, x - 8, y, c, font     );	
				putfont8(vram, xsize, x    , y, c, font + 16);	
			}
			x += 8;
		}
	}
	if (task->langmode == 2) {
		for (; *s != 0x00; s++) {
			if (task->langbyte1 == 0) {
				if (0x81 <= *s && *s <= 0xfe) {
					task->langbyte1 = *s;
				} else {
					putfont8(vram, xsize, x, y, c, nihongo + *s * 16);
				}
			} else {
				k = task->langbyte1 - 0xa1;
				t = *s - 0xa1;
				task->langbyte1 = 0;
				font = nihongo + 256 * 16 + (k * 94 + t) * 32;
				putfont8(vram, xsize, x - 8, y, c, font     );	
				putfont8(vram, xsize, x    , y, c, font + 16);
			}
			x += 8;
		}
	}
	return;
}

void init_mouse_cursor8(short *mouse,unsigned char bc)
{
	static char cursor[16][16] = {
		"**..............",
		"*O*.............",
		"*OO*............",
		"*OOO*...........",
		"*OOOO*..........",
		"*OOOOO*.........",
		"*OOOOOO*........",
		"*OOOOOOO*.......",
		"*OOOOOOOO*......",
		"*OOOOO*****.....",
		"*OO*OO*.........",
		"*O*.*OO*........",
		"**..*OO*........",
		"*....*OO*.......",
		".....*OO*.......",
		"......**........",
	};
	int x, y;

	for (y = 0; y < 16; y++) {
		for (x = 0; x < 16; x++) {
			if (cursor[y][x] == '*') {
				mouse[y * 16 + x] = table_8_565[COL8_FFFFFF];
			}
			if (cursor[y][x] == 'O') {
				mouse[y * 16 + x] = table_8_565[COL8_000000];
			}
			if (cursor[y][x] == '.') {
				mouse[y * 16 + x] = table_8_565[bc];
			}
		}
	}
	return;
}

void putblock8_8(short *vram, int vxsize, int pxsize,
	int pysize, int px0, int py0, short *buf, int bxsize)
{
	int x, y;
	for (y = 0; y < pysize; y++) {
		for (x = 0; x < pxsize; x++) {
			vram[(py0 + y) * vxsize + (px0 + x)] = table_8_565[buf[y * bxsize + x]];
		}
	}
	return;
}

int read_picture(int *fat, short *vram, int x, int y, char *fname)
{
	int i, j, x0, y0, fsize, info[4];
	unsigned char *filebuf, r, g, b;
	struct RGB *picbuf;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct FILEINFO *finfo;
	struct DLL_STRPICENV *env;
	//unsigned temp;
	finfo = file_search(fname, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo == 0 || (finfo->type & 0x18) != 0) {
		return -1;
	}
	
	fsize = finfo->size;

	filebuf = (unsigned char *)memman_alloc_4k(memman, fsize);
	filebuf = file_loadfile2(finfo->clustno, &fsize, fat);

	env = (struct DLL_STRPICENV *) memman_alloc_4k(memman, sizeof(struct DLL_STRPICENV));
	info_JPEG(env, info, fsize, filebuf);
	picbuf = (struct RGB *) memman_alloc_4k(memman, info[2] * info[3] * sizeof(struct RGB));
	decode0_JPEG(env, fsize, filebuf, 4, (unsigned char *)picbuf, 0);

	x0 = (int)((x - info[2]) / 2);
	y0 = (int)((y - info[3]) / 2);
	
	for (i = 0; i < info[3]; i++) {
		for (j = 0; j < info[2]; j++) {
			r = picbuf[i * info[2] + j].r;
			g = picbuf[i * info[2] + j].g;
			b = picbuf[i * info[2] + j].b;
			
			vram[(y0 + i) * x + (x0 + j)] = rgb2pal(r, g, b, j, i, binfo->vmode);
		}
	}
	//char tm[10];

	memman_free_4k(memman, (int)filebuf, fsize);
	memman_free_4k(memman, (int)picbuf, info[2] * info[3] * sizeof(struct RGB));
	memman_free_4k(memman, (int)env, sizeof(struct DLL_STRPICENV));

	return 0;
}

unsigned short rgb2pal(int r, int g, int b, int x, int y, int cb)
{
	if (cb == 8) {
		static int table[4] = { 3, 1, 0, 2 };
		int i;
		x &= 1;
		y &= 1;
		i = table[x + y * 2];
		r = (r * 21) / 256;
		g = (g * 21) / 256;
		b = (b * 21) / 256;
		r = (r + i) / 4;
		g = (g + i) / 4;
		b = (b + i) / 4;
		return((unsigned short)(27 + r + g * 6 + b * 36));
	}
	else {
		return((unsigned short)(((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | (b >> 3)));
	}
}



