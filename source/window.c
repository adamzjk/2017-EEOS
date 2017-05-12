/* ウィンドウ関係 */
#include "bootpack.h"

int windownum = 0;
extern unsigned short table_8_565[256];

void make_window8(unsigned short *buf, int xsize, int ysize, char *title, char act)
{
	short clr = rgb2pal(37,191,247, 0, 0, 16);
	table_8_565[10] = rgb2pal(79,79,79, 0, 0, 16);
	boxfill8(buf, xsize, clr, 0, 0, xsize - 1, ysize - 1);
	make_wtitle8(buf, xsize, title, act);
	return;
}

void make_wtitle8(unsigned short *buf, int xsize, char *title, char act)
{
	static char closebtn[14][16] = {
		"OOOOOOOOOOOOOOO0",
		"OQQQQQQQQQQQQQ00",
		"OQQQQQQQQQQQQQ00",
		"OQQQ@@QQQQ@@QQ00",
		"OQQQQ@@QQ@@QQQ00",
		"OQQQQQ@@@@QQQQ00",
		"OQQQQQQ@@QQQQQ00",
		"OQQQQQ@@@@QQQQ00",
		"OQQQQ@@QQ@@QQQ00",
		"OQQQ@@QQQQ@@QQ00",
		"OQQQQQQQQQQQQQ00",
		"OQQQQQQQQQQQQQ00",
		"O$$$$$$$$$$$$$00",
		"0000000000000000"
	};
	int x, y;
	char c, tc;
	if (act != 0) {
		tc = 0;
	} else {
		tc = 7;
	}
	if(windownum == 0)
		table_8_565[10] = rgb2pal(37,191,247, 0, 0, 16);
	else if(windownum == 1)
		table_8_565[10] = rgb2pal(255,74,74, 0, 0, 16);
	else if(windownum == 2)
		table_8_565[10] = rgb2pal(187,71,245, 0, 0, 16);
	else if(windownum == 3)
		table_8_565[10] = rgb2pal(255,246,74, 0, 0, 16);
	else if(windownum == 4)
		table_8_565[10] = rgb2pal(146,247,37, 0, 0, 16);
	else
	{
		table_8_565[10] = rgb2pal(37,191,247, 0, 0, 16);
		windownum = 0;
	}
	windownum++;
	boxfill8(buf, xsize, 10, 0, 0, xsize - 1, 20);
	putfonts8_asc(buf, xsize, 6, 4, COL8_FFFFFF, title);
	for (y = 0; y < 18; y++) {
		for (x = 0; x < 18; x++) {
			c = closebtn[y][x];
			if (c == '@') {
				c = COL8_FFFFFF;
			}
			else {
				c = 10;
			}
			buf[(3 + y) * xsize + (xsize - 20 + x)] = table_8_565[(int)c];
		}
	}
	return;
}

void putfonts8_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l)
{
	struct TASK *task = task_now();
	boxfill8(sht->buf, sht->bxsize, b, x, y, x + l * 8 - 1, y + 15);
	if (task->langmode != 0 && task->langbyte1 != 0) {
		putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s);
		sheet_refresh(sht, x - 8, y, x + l * 8, y + 16);
	} else {
		putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s);
		sheet_refresh(sht, x, y, x + l * 8, y + 16);
	}
	return;
}

void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c)
{
	boxfill8(sht->buf, sht->bxsize, c, x0 + 1, y0 + 1, sx - 1, sy - 1);
	return;
}

/*void change_wtitle8(struct SHEET *sht, char act)
{
	int x, y, xsize = sht->bxsize;
	char c, tc_new, tbc_new, tc_old, tbc_old, *buf = sht->buf;
	if (act != 0) {
		tc_new  = COL8_FFFFFF;
		tbc_new = COL8_000084;
		tc_old  = COL8_C6C6C6;
		tbc_old = COL8_848484;
	} else {
		tc_new  = COL8_C6C6C6;
		tbc_new = COL8_848484;
		tc_old  = COL8_FFFFFF;
		tbc_old = COL8_000084;
	}
	for (y = 3; y <= 20; y++) {
		for (x = 3; x <= xsize - 4; x++) {
			c = buf[y * xsize + x];
			if (c == tc_old && x <= xsize - 22) {
				c = tc_new;
			} else if (c == tbc_old) {
				c = tbc_new;
			}
			buf[y * xsize + x] = c;
		}
	}
	sheet_refresh(sht, 3, 3, xsize, 21);
	return;
}*/

void change_wtitle8(struct SHEET *sht, char act)
{
	int xsize = sht->bxsize;
	int ysize = sht->bysize;
	unsigned char  tc;
	if (act != 0) {
		tc = 7;
	} else {
		tc = 0;
	}
	sheet_refresh(sht, 5, 23, xsize- 6, ysize -6);
	return;
}




