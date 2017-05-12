/* bootpack */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>

#define KEYCMD_LED		0xed

//desktop
int *fat;
#define bg_color COL8_000084
#define wd_color COL8_FFFFFF

struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
struct SHTCTL *shtctl;
struct SHEET  *sht_win;

extern int switch_mode;
unsigned int mem4con;

unsigned int memtotal;
//short clr = rgb2pal(37,191,247, 0, 0, 16);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	int show_apps = 0;

	char s[40];
	struct FIFO32 fifo, keycmd;
	int fifobuf[128], keycmd_buf[32];
	int mx, my, i, cursor_x, cursor_c;
	
	struct MOUSE_DEC mdec;
	unsigned short *buf_back, *buf_app, buf_mouse[256], *buf_win, *buf_cons;
	struct SHEET *sht_back, *sht_app, *sht_mouse, *sht_cons;
	struct TASK *task_a, *task_cons;
	struct TIMER *timer;
	static char keytable0[0x80] = {
		0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,   0,   ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
	static char keytable1[0x80] = {
		0,   0,   '!', 0x22, '#', '$', '%', '&', 0x27, '(', ')', '~', '=', '~', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*', 0,   0,   '}', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};
	int key_to = 0, key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	struct CONSOLE *cons;

	// japanese
	unsigned char *nihongo;
	struct FILEINFO *finfo;
	extern char hankaku[4096];

	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC的初始化已经完成，于是开放CPU的中断 */
	fifo32_init(&fifo, 128, fifobuf, 0);
	init_pit();
	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); /* 设定PIT和PIC1以及键盘为许可(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */
	fifo32_init(&keycmd, 32, keycmd_buf, 0);

	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	mem4con = memtotal;

	// japanese
	fat = (int *)memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char *)(ADR_DISKIMG + 0x000200));

	finfo = file_search("nihongo.fnt", (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo != 0) {
		i = finfo->size;
		nihongo = file_loadfile2(finfo->clustno, &i, fat);
	}
	else {
		nihongo = (unsigned char *)memman_alloc_4k(memman, 16 * 256 + 32 * 94 * 47);
		for (i = 0; i < 16 * 256; i++) {
			nihongo[i] = hankaku[i];
		}
		for (i = 16 * 256; i < 16 * 256 + 32 * 94 * 47; i++) {
			nihongo[i] = 0xff;
		}
	}
	*((int *)0x0fe8) = (int)nihongo;
	memman_free_4k(memman, (int)fat, 4 * 2880);

	init_palette();
	init_open(binfo->vram, binfo->scrnx, &fifo);

	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	*((int *) 0x0fe4) = (int) shtctl;

	/* sht_back */
	sht_back  = sheet_alloc(shtctl);
	buf_back  = (unsigned short *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny * 2);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1); /* 无透明色 */
	init_screen8(buf_back, binfo->scrnx, binfo->scrny, "pic.jpg");

	// sht_app
	sht_app = sheet_alloc(shtctl);
	buf_app = (unsigned short *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny * 2);
	sheet_setbuf(sht_app, buf_app, binfo->scrnx, binfo->scrny, -1); /* 无透明色 */
	init_screen8(buf_app, binfo->scrnx, binfo->scrny, "apps.jpg");

	/* sht_win */
	sht_win   = sheet_alloc(shtctl);
	buf_win   = (unsigned short *) memman_alloc_4k(memman, 256 * 56 * 2);
	sheet_setbuf(sht_win, buf_win, 256, 56, -1); /* 无透明色 */
	make_window8(buf_win, 256, 56, "editor", 1);
	make_textbox8(sht_win, 8, 28, 230, 16, COL8_000000);
	cursor_x = 8;
	cursor_c = COL8_FFFFFF;

	timer = timer_alloc();
	timer_init(timer, &fifo, 1);
	timer_settime(timer, 50);

	/* sht_mouse */
	sht_mouse = sheet_alloc(shtctl);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	init_mouse_cursor8(buf_mouse, 99);
	mx = (binfo->scrnx - 16) / 2; /* 计算坐标使其位于画面中央 */
	my = (binfo->scrny - 28 - 16) / 2;

	/* tasks */
	task_a = task_init(memman);
	strcpy(task_a->name, "kernel");
	fifo.task = task_a;
	task_run(task_a, 0, 2);

	/* sht_cons */
	sht_cons = sheet_alloc(shtctl);
	buf_cons = (unsigned short *) memman_alloc_4k(memman, 256 * 165 * 2);
	sheet_setbuf(sht_cons, buf_cons, 256, 165, -1); /* 无透明色 */
	make_window8(buf_cons, 256, 165, "console", 0);
	make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000);
	task_cons = task_alloc();
	task_cons->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 12;
	task_cons->tss.eip = (int) &console_task;
	task_cons->tss.es = 1 * 8;
	task_cons->tss.cs = 2 * 8;
	task_cons->tss.ss = 1 * 8;
	task_cons->tss.ds = 1 * 8;
	task_cons->tss.fs = 1 * 8;
	task_cons->tss.gs = 1 * 8;
	strcpy(task_cons->name, "console");
	*((int *) (task_cons->tss.esp + 4)) = (int) sht_cons;
	*((int *) (task_cons->tss.esp + 8)) = memtotal;
	task_run(task_cons, 1, 2); /* level=2, priority=2 */

	sheet_slide(sht_back,  	0,	0);
	sheet_slide(sht_mouse, 	mx, my);
	sheet_slide(sht_win,  	400,400);
	sheet_slide(sht_cons, 	32, 2);
	sheet_slide(sht_app, 	0,  0);

	sheet_updown(sht_back,  	0);
	sheet_updown(sht_mouse, 	1);
	sheet_updown(sht_win,   	-1);
	sheet_updown(sht_cons,  	-1);
	sheet_updown(sht_app, 		-1);

	/*为了避免和键盘当前状态冲突，在一开始先进行设置*/
	fifo32_put(&keycmd, KEYCMD_LED);
	fifo32_put(&keycmd, key_leds);

	switch_mode = 1;//2->linux

	for (;;) {
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) {
			/*如果存在向键盘控制器发送的数据，则发送它 */
			keycmd_wait = fifo32_get(&keycmd);
			wait_KBC_sendready();
			io_out8(PORT_KEYDAT, keycmd_wait);
		}
		io_cli();
		if (fifo32_status(&fifo) == 0) {
			task_sleep(task_a);
			io_sti();
		} else {
			i = fifo32_get(&fifo);
			io_sti();
			if (256 <= i && i <= 511) { /* 键盘数据*/
				if (i < 0x80 + 256) { /*将按键编码转换为字符编码*/
					if (key_shift == 0) {
						s[0] = keytable0[i - 256];
					} else {
						s[0] = keytable1[i - 256];
					}
				} else {
					s[0] = 0;
				}
				if ('A' <= s[0] && s[0] <= 'Z') { /*当输入字符为英文字母时*/
					if (((key_leds & 4) == 0 && key_shift == 0) ||((key_leds & 4) != 0 && key_shift != 0)) {
						s[0] += 0x20; /*将大写字母转换为小写字母*/
					}
				}
				if (s[0] != 0) { /*一般字符*/
					if (key_to == 0) { /*发送给任务A */
						if (cursor_x < 230) {
							/*显示一个字符之后将光标后移一位*/
							s[1] = 0;
							putfonts8_asc_sht(sht_win, cursor_x, 28,COL8_FFFFFF ,COL8_000000 , s, 1);
							cursor_x += 8;
						}
					} else { /*发送给命令行窗口*/
						fifo32_put(&task_cons->fifo, s[0] + 256);
					}
				}
				if (i == 256 + 0x0e) { /* 退格键 */
					if (key_to == 0) { /*发送给任务A */
						if (cursor_x > 8) {
							/*用空白擦除光标后将光标前移一位*/
							putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_FFFFFF,COL8_000000 , " ", 1);
							cursor_x -= 8;
						}
					} else { /*发送给命令行窗口*/
						fifo32_put(&task_cons->fifo, 8 + 256);
					}
				}
				if (i == 256 + 0x1c) { /*回车键*/
					if (key_to != 0) { /*发送至命令行窗口*/
						fifo32_put(&task_cons->fifo, 10 + 256);
					}
				}
				if (i == 256 + 0x0f) {	/* Tab */
					if (key_to == 0) {
						key_to = 1;
						make_wtitle8(buf_win,  sht_win->bxsize,  "editor",  0);
						make_wtitle8(buf_cons, sht_cons->bxsize, "console", 1);
						cursor_c = -1; /* 不显示光标 */
						boxfill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cursor_x, 28, cursor_x + 7, 43);
						fifo32_put(&task_cons->fifo, 2); /*命令行窗口光标ON */
					} else {
						key_to = 0;
						make_wtitle8(buf_win,  sht_win->bxsize,  "editor",  1);
						make_wtitle8(buf_cons, sht_cons->bxsize, "console", 0);
						cursor_c = COL8_000000;
						fifo32_put(&task_cons->fifo, 3); /*命令行窗口光标OFF */
					}
					sheet_refresh(sht_win,  0, 0, sht_win->bxsize,  21);
					sheet_refresh(sht_cons, 0, 0, sht_cons->bxsize, 21);
				}
				if (i == 256 + 0x2a) { /*左Shift ON */
					key_shift |= 1;
				}
				if (i == 256 + 0x36) { /*右Shift ON */
					key_shift |= 2;
				}
				if (i == 256 + 0xaa) { /*左Shift OFF */
					key_shift &= ~1;
				}
				if (i == 256 + 0xb6) { /*右Shift OFF */
					key_shift &= ~2;
				}
				if (i == 256 + 0x3a) {	/* CapsLock */
					key_leds ^= 4;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x45) {	/* NumLock */
					key_leds ^= 2;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x46) {	/* ScrollLock */
					key_leds ^= 1;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x3b && key_shift != 0 && task_cons->tss.ss0 != 0) { /* Shift+F1 */
					cons = (struct CONSOLE *) *((int *) 0x0fec);
					cons_putstr0(cons, "\nBreak(key) :\n");
					io_cli(); /*不能在改变寄存器值时切换到其他任务*/
					task_cons->tss.eax = (int) &(task_cons->tss.esp0);
					task_cons->tss.eip = (int) asm_end_app;
					io_sti();
				}
				if (i == 256 + 0xfa) { /*键盘成功接收到数据*/
					keycmd_wait = -1;
				}
				if (i == 256 + 0xfe) { /*键盘没有成功接收到数据*/
					wait_KBC_sendready();
					io_out8(PORT_KEYDAT, keycmd_wait);
				}
				/*重新显示光标*/
				if (cursor_c >= 0) {
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				}
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			} else if (512 <= i && i <= 767) { /* 鼠标数据*/
				if (mouse_decode(&mdec, i - 512) != 0) {
					/* 已经收集了3字节的数据，移动光标 */
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0) {
						mx = 0;
					}
					if (my < 0) {
						my = 0;
					}
					if (mx > binfo->scrnx - 1) {
						mx = binfo->scrnx - 1;
					}
					if (my > binfo->scrny - 1) {
						my = binfo->scrny - 1;
					}
					sheet_slide(sht_mouse, mx, my);/* 包含sheet_refresh */
					if ((mdec.btn & 0x01) != 0) { /* 按下左键 */
						if(mx < 32 && my > 730 && show_apps == 0 ){
							show_apps = 1;
							sheet_updown(sht_app, -1);
							sheet_updown(sht_app, 9);
							sheet_updown(sht_mouse, -1);
							sheet_updown(sht_mouse, 9);
						} else if(show_apps == 1 && mx > 115 && mx < 205 && my > 103 && my < 180){
							show_apps = 0;
							sheet_updown(sht_app, -1);

							sheet_updown(sht_cons,  3);
							sheet_updown(sht_mouse, -1);
							sheet_updown(sht_mouse, 4);
						} else if(show_apps == 1 && mx > 290 && mx < 374 && my > 103 && my < 180){
							show_apps = 0;
							sheet_updown(sht_app, -1);

							fifo32_put(&task_cons->fifo, (int) "p" + 256);
							fifo32_put(&task_cons->fifo, (int) "s" + 256);
						} else if(show_apps == 1 && mx > 450 && mx < 530 && my > 103 && my < 180){
							show_apps = 0;
							sheet_updown(sht_app, -1);

							sheet_updown(sht_win,  3);
							sheet_updown(sht_mouse, -1);
							sheet_updown(sht_mouse, 4);
						} 
					}
				}
			} else if (i <= 1) { /* 光标用定时器*/
				if (i != 0) {
					timer_init(timer, &fifo, 0); /* 下面设定0 */
					if (cursor_c >= 0) {
						cursor_c = COL8_000000;
					}
				} else {
					timer_init(timer, &fifo, 1); /* 下面设定1 */
					if (cursor_c >= 0) {
						cursor_c = COL8_FFFFFF;
					}
				}
				timer_settime(timer, 50);
				if (cursor_c >= 0) {
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
					sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
				}
			}
		}
	}
}


void init_open(unsigned short *vram1, int xsize1, struct FIFO32 *fifo)
{
	struct TIMER *timer3, *timer4, *timer5, *timer6, *timer7, *timer8, *timer9;
	int BG[12] = { COL8_336666,COL8_3D7878,COL8_408080,COL8_4F9D9D,COL8_5CADAD,
		COL8_6FB7B7,COL8_81C0C0,COL8_95CACA,COL8_A3D1D1,COL8_B3D9D9,COL8_C4E1E1,COL8_D1E9E9 };
	timer3 = timer_alloc();
	timer_init(timer3, fifo, 3);
	timer_settime(timer3, 300);
	timer4 = timer_alloc();
	timer_init(timer4, fifo, 4);
	timer_settime(timer4, 50);
	timer5 = timer_alloc();
	timer_init(timer5, fifo, 5);
	timer_settime(timer5, 100);
	timer6 = timer_alloc();
	timer_init(timer6, fifo, 6);
	timer_settime(timer6, 150);
	timer7 = timer_alloc();
	timer_init(timer7, fifo, 7);
	timer_settime(timer7, 200);
	timer8 = timer_alloc();
	timer_init(timer8, fifo, 8);
	timer_settime(timer8, 250);
	timer9 = timer_alloc();
	timer_init(timer9, fifo, 9);
	timer_settime(timer9, 350);

	int x, y;
	for (y = 0; y < 768; y++) {
		for (x = 0; x < 1024; x++)
			vram1[y * xsize1 + x] = BG[y / 64];
	}
	putfonts8_asc_pre(vram1, xsize1, 410, 350, wd_color, "Welcome to my OS");
	putfonts8_asc_pre(vram1, xsize1, 411, 350, COL8_000000, "Welcome to my OS");//COL8_840084
	int xi = 450;
	for (xi = 450;xi <= 490;xi += 10) {
		putfonts8_asc_pre(vram1, xsize1, xi, 380, wd_color, "*");
	}
	int outi = 0;//320 384
	for (;;) {
		if (fifo32_status(fifo) != 0) {
			outi = fifo32_get(fifo);
			if (outi == 4) {
				putfonts8_asc_pre(vram1, xsize1, 450, 380, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 450, 370, wd_color, "*");
			}
			else if (outi == 5) {
				putfonts8_asc_pre(vram1, xsize1, 450, 370, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 450, 380, wd_color, "*");
				putfonts8_asc_pre(vram1, xsize1, 460, 380, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 460, 370, wd_color, "*");
			}
			else if (outi == 6) {
				putfonts8_asc_pre(vram1, xsize1, 460, 370, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 460, 380, wd_color, "*");
				putfonts8_asc_pre(vram1, xsize1, 470, 380, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 470, 370, wd_color, "*");
			}
			else if (outi == 7) {
				putfonts8_asc_pre(vram1, xsize1, 470, 370, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 470, 380, wd_color, "*");
				putfonts8_asc_pre(vram1, xsize1, 480, 380, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 480, 370, wd_color, "*");
			}
			else if (outi == 8) {
				putfonts8_asc_pre(vram1, xsize1, 480, 370, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 480, 380, wd_color, "*");
				putfonts8_asc_pre(vram1, xsize1, 490, 380, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 490, 370, wd_color, "*");
			}
			else if (outi == 3) {
				putfonts8_asc_pre(vram1, xsize1, 490, 370, BG[5], "*");
				putfonts8_asc_pre(vram1, xsize1, 490, 380, wd_color, "*");
			}
			else if (outi == 9) {
				break;
			}
		}
	}
	timer_free(timer3);
	timer_free(timer4);
	timer_free(timer5);
	timer_free(timer6);
	timer_free(timer7);
	timer_free(timer8);
	timer_free(timer9);

}

void wait(volatile struct xhl* m, int i)
{
	m->waiting[i] = 1;
	int key = 1, ss = m->size;
	while (m->waiting[i] && key) {
		key = TestAndSet(&(m->lock));
	}
	m->waiting[i] = 0;

	while (m->mutex <= 0);
	m->mutex--;

	int j = (i + 1) % ss;
	while ((j != i) && !m->waiting[j]) {
		j = (j + 1) % ss;
	}
	if (j == i) {
		m->lock = 0;
	}
	else {
		m->waiting[j] = 0;
	}
}

void signal(volatile struct xhl* m)
{
	m->mutex++;
}

void mutex_init(volatile struct xhl* m)
{
	m->mutex = 1;
	m->lock = 0;
	int i = 0;
	for (i = 0;i<20;i++) {
		m->waiting[i] = 0;
	}
	m->size = 0;
}

struct SHEET *open_console(struct SHTCTL *shtctl, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHEET *sht = sheet_alloc(shtctl);
	unsigned short *buf = (unsigned short *)memman_alloc_4k(memman, 256 * 165 * 2);
	sheet_setbuf(sht, buf, 256, 165, -1); /* 锟斤拷锟斤拷锟紽锟饺傦拷 */
	make_window8(buf, 256, 165, "console", 0);
	make_textbox8(sht, 5, 24, 250, 159, COL8_000000);
	sht->task = open_constask(sht, memtotal);
	sht->flags |= 0x20;	/* 锟絁锟絒锟絓锟斤拷锟斤拷锟斤拷 */
	return sht;
}
struct SHEET *open_console1(struct SHTCTL *shtctl, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHEET *sht = sheet_alloc(shtctl);
	unsigned short *buf = (unsigned short *)memman_alloc_4k(memman, 256 * 165 * 2);
	sheet_setbuf(sht, buf, 256, 165, -1); 
	make_window8(buf, 256, 165, "buy ticket", 0);
	make_textbox8(sht, 5, 24, 250, 159, COL8_000000);
	sht->task = open_constask(sht, memtotal);
	sht->flags |= 0x20;	
	return sht;
}

struct TASK *open_constask(struct SHEET *sht, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_alloc();
	int *cons_fifo = (int *)memman_alloc_4k(memman, 128 * 4);
	task->cons_stack = memman_alloc_4k(memman, 64 * 1024);
	task->tss.esp = task->cons_stack + 64 * 1024 - 12;
	task->tss.eip = (int)&console_task;
	task->tss.es = 1 * 8;
	task->tss.cs = 2 * 8;
	task->tss.ss = 1 * 8;
	task->tss.ds = 1 * 8;
	task->tss.fs = 1 * 8;
	task->tss.gs = 1 * 8;
	*((int *)(task->tss.esp + 4)) = (int)sht;
	*((int *)(task->tss.esp + 8)) = memtotal;
	task_run(task, 2, 2); /* level=2, priority=2 */
	fifo32_init(&task->fifo, 128, cons_fifo, task);
	return task;
}


