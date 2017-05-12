/* 命令行窗口相关 */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>

// calc
#define INVALID		-0x7fffffff
int strtol(char *s, char **endp, int base);	/* 标准函数 <stdlib.h> */
char *skipspace(char *p);
int getnum(char **pp, int priority);

unsigned short *buf_win_b[3], *buf_win_ps, *sht_win_b1[3], *buf_win_ps1;
struct SHEET *sht_win_b[3], *sht_win_ps = 0, *sht_win_ps1, *task_ps1;
struct TASK *task_b[3], *task_ps = 0, *task_mem,*task_b1[3];

extern struct MEMMAN *memman;
extern struct SHTCTL *shtctl;
extern struct TASKCTL *taskctl;
extern unsigned int memtotal;


/* LJH */
extern unsigned int mem4con;
int switch_mode;
struct SHEET  *key_win1;
struct CONSOLE *cons1;
void task_counter(struct SHEET *sht_win_b);

struct xhl mm;
int mutex_init_t = 0;
int fla = 0;
int book = 20;

void create_cpt();
void Read();
void Write();


void task_counter(struct SHEET *sht_win_b);
void printInfo(void);

void console_task(struct SHEET *sheet, unsigned int memtotal)
{
	struct TIMER *timer;
	struct TASK *task = task_now();
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	int i, fifobuf[128], *fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	struct CONSOLE cons;
	char cmdline[30];
	cons.sht = sheet;
	cons.cur_x = 8;
	cons.cur_y = 28;
	cons.cur_c = -1;
	*((int *) 0x0fec) = (int) &cons;

	/* wzh */
	cons.dir_info = (struct DIRINFO *)memman_alloc(memman, sizeof(struct DIRINFO));
	cons.dir_info->adr_parent = 0;											//* 无父节点目录
	cons.dir_info->adr_dir = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);	//* 当前目录为根目录
	cons.dir_info->clustno = 20;
	cons.dir_info->maxsize = 224;							//* 根目录的finfo最大为224
	sprintf(cons.dir_info->name, "ROOT");

	fifo32_init(&task->fifo, 128, fifobuf, task);
	timer = timer_alloc();
	timer_init(timer, &task->fifo, 1);
	timer_settime(timer, 50);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200)); 

	/*显示提示符*/
	cons_putchar(&cons, '>', 1);
	for (;;) {
		io_cli();
		if (fifo32_status(&task->fifo) == 0) {
			task_sleep(task);  
			io_sti();
		} else {
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1) { /*光标用定时器*/
				if (i != 0) {
					timer_init(timer, &task->fifo, 0); /*下次置0 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_FFFFFF;
					}
				} else {
					timer_init(timer, &task->fifo, 1); /*下次置1 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_000000;
					}
				}
				timer_settime(timer, 50);
			}
			if (i == 2) { /*光标ON */
				cons.cur_c = COL8_FFFFFF;
			}
			if (i == 3) { /*光标OFF */
				boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				cons.cur_c = -1;
			}
			if (256 <= i && i <= 511) { /*键盘数据（通过任务A）*/
				if (i == 8 + 256) {
					/*退格键*/
					if (cons.cur_x > 16) {
					/*用空格擦除光标后将光标前移一位*/
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				} else if (i == 10 + 256) {
					/*回车键*/
					/*将光标用空格擦除后换行 */
					cons_putchar(&cons, ' ', 0);
					cmdline[cons.cur_x / 8 - 2] = 0;
					cons_newline(&cons);
					cons_runcmd(cmdline, &cons, fat, memtotal); /*运行命令*/
					/*显示提示符*/
					cons_putchar(&cons, '>', 1);
				} else {
					/*一般字符*/
					if (cons.cur_x < 240) {
						/*显示一个字符之后将光标后移一位*/
						cmdline[cons.cur_x / 8 - 2] = i - 256;
						cons_putchar(&cons, i - 256, 1);
					}
				}
			}
			/*重新显示光标*/
			if (cons.cur_c >= 0) {
				boxfill8(sheet->buf, sheet->bxsize, cons.cur_c, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
			}
			sheet_refresh(sheet, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
		}
	}
}

void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;
	if (s[0] == 0x09) { /*制表符*/
		for (;;) {
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
			if (((cons->cur_x - 8) & 0x1f) == 0) {
				break; /*被32整除则break*/
			}
		}
	} else if (s[0] == 0x0a) { /*换行*/
		cons_newline(cons);
	} else if (s[0] == 0x0d) { /*回车*/
		/*先不做任何操作*/
	} else { /*一般字符*/
		putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		if (move != 0) {
			/* move为0时光标不后移*/
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
		}
	}
	return;
}

void cons_newline(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	if (cons->cur_y < 28 + 112) {
		cons->cur_y += 16; /*到下一行*/
	} else {
		/*滚动*/
		for (y = 28; y < 28 + 112; y++) {
			for (x = 8; x < 8 + 240; x++) {
				sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
			}
		}
		for (y = 28 + 112; y < 28 + 128; y++) {
			for (x = 8; x < 8 + 240; x++) {
				sheet->buf[x + y * sheet->bxsize] = COL8_000000;
			}
		}
		sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
	}
	cons->cur_x = 8;
	return;
}

void cons_putstr0(struct CONSOLE *cons, char *s)
{
	for (; *s != 0; s++) {
		cons_putchar(cons, *s, 1);
	}
	return;
}

void cons_putstr1(struct CONSOLE *cons, char *s, int l)
{
	int i;
	for (i = 0; i < l; i++) {
		cons_putchar(cons, s[i], 1);
	}
	return;
}

struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;

void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, unsigned int memtotal)
{
	if (strcmp(cmdline, "mem") == 0) {
		cmd_mem(cons, memtotal);
	} else if (strcmp(cmdline, "cls") == 0) {
		cmd_cls(cons);
	} else if (strncmp(cmdline, "type ", 5) == 0) {
		cmd_type(cons, fat, cmdline);
	} else if(strcmp(cmdline,"create counter") == 0){
		int i = 0;
		char s[20];
		for (i = 0; i < 3; i++) {
			sht_win_b[i] = sheet_alloc(shtctl);
			buf_win_b[i] = (unsigned short *) memman_alloc_4k(memman, 200 * 52 * 2);
			sheet_setbuf(sht_win_b[i], buf_win_b[i], 200, 52, -1); /* 无透明色 */
			sprintf(s, "counter%d", i);
			make_window8(buf_win_b[i], 200, 52, s, 0);
			task_b[i] = task_alloc();
			strcpy(task_b[i]->name,s);
			task_b[i]->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
			task_b[i]->tss.eip = (int) &task_counter;
			task_b[i]->tss.es = 1 * 8;
			task_b[i]->tss.cs = 2 * 8;
			task_b[i]->tss.ss = 1 * 8;
			task_b[i]->tss.ds = 1 * 8;
			task_b[i]->tss.fs = 1 * 8;
			task_b[i]->tss.gs = 1 * 8;
			*((int *) (task_b[i]->tss.esp + 4)) = (int) sht_win_b[i];
		}

		sheet_slide(sht_win_b[0], 100, 600);
		sheet_slide(sht_win_b[1], 400, 600);
		sheet_slide(sht_win_b[2], 700, 600);

		sheet_updown(sht_win_b[0], 	3);
		sheet_updown(sht_win_b[1], 	4);
		sheet_updown(sht_win_b[2], 	5);
		
	} else if(strcmp(cmdline,"run counter") == 0){
		if (switch_mode == 1) {
			task_run(task_b[0], 1, 1);
			task_run(task_b[1], 1, 2);
			task_run(task_b[2], 2, 3);
		}
		else {
			task_run_in2(task_b[0],120);
			task_run_in2(task_b[1],110);
			task_run_in2(task_b[2],100);
		}
	} else if(strcmp(cmdline,"run counter -rr") == 0){
		if (switch_mode == 1) {
			task_run(task_b[0], 2, 2);
			task_run(task_b[1], 2, 2);
			task_run(task_b[2], 2, 2);
		}
		else {
			task_run_in2(task_b[0],120);
			task_run_in2(task_b[1],120);
			task_run_in2(task_b[2],120);
		}
	} else if(strcmp(cmdline,"pause counter") == 0){
		int i;
		for (i = 0; i < 3; i++){
			io_cli();
			task_sleep(task_b[i]);
			io_sti();
		}
	} else if(strcmp(cmdline, "kill counter") == 0){
		int i;
		for (i = 0; i < 3; i++){
			memman_free_4k(memman,(unsigned int) buf_win_b[i], 200 * 52);
			memman_free_4k(memman,(unsigned int) task_b[i]->tss.esp, 64 * 1024);
			sheet_free(sht_win_b[i]);
			task_sleep(task_b[i]);
			task_b[i]->flags = TASK_UNUSE;
		}
	} else if(strcmp(cmdline,"shut down") == 0){
		int i[2];
		i[999] = i[2];
	} else if(strcmp(cmdline, "show resolution") == 0){
		char s[30];
		sprintf(s, "%d*%d\n\n",binfo->scrnx,binfo->scrny );
		cons_putstr0(cons, s);
	}  else if(strcmp(cmdline,"ps") == 0){
		if(sht_win_ps != 0){
			sheet_updown(sht_win_ps, 3);
			/*cons_putstr0(cons, "PS Already Running!.\n\n");*/
			return;
		}
		sht_win_ps = sheet_alloc(shtctl);
		buf_win_ps = (unsigned short *) memman_alloc_4k(memman, 440 * 284 * 2);
		sheet_setbuf(sht_win_ps, buf_win_ps, 440, 284, -1); /* 无透明色 */
		make_window8(buf_win_ps, 440, 284, "ps", 0);
		sheet_slide(sht_win_ps, 300, 0);
		sheet_updown(sht_win_ps, 	3);

		task_ps = task_alloc();
		strcpy(task_ps->name,"ps");
		task_ps->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
		task_ps->tss.eip = (int) &printInfo;
		task_ps->tss.es = 1 * 8;
		task_ps->tss.cs = 2 * 8;
		task_ps->tss.ss = 1 * 8;
		task_ps->tss.ds = 1 * 8;
		task_ps->tss.fs = 1 * 8;
		task_ps->tss.gs = 1 * 8;
		task_run(task_ps, 1, 2);
	} else if(strcmp(cmdline, "kill ps") == 0){
/*		if(sht_win_ps == 0){
			cons_putstr0(cons, "No ps Task Found!.\n\n");
			return;
		}*/
		sheet_updown(sht_win_ps, -1);
		/*sheet_free(sht_win_ps);
		sht_win_ps = 0;
		task_sleep(task_ps);
		memman_free_4k(memman, (unsigned int)buf_win_ps, 440 * 284);
		memman_free_4k(memman, (unsigned int)task_ps->tss.esp, 64 * 1024);
		task_ps->flags = TASK_UNUSE;*/
	}else if (strcmp(cmdline, "show mem") == 0) {
		sht_win_ps1 = sheet_alloc(shtctl);
		buf_win_ps1 = (unsigned short *)memman_alloc_4k(memman, 440 * 284 * 2);
		sheet_setbuf(sht_win_ps1, buf_win_ps1, 440, 284, -1);
		make_window8(buf_win_ps1, 440, 284, "mem total", 0);
		sheet_slide(sht_win_ps1, 300, 0);
		sheet_updown(sht_win_ps1, 9);

		task_mem = task_alloc();
		strcpy(task_mem->name, "mem total");
		task_mem->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
		task_mem->tss.eip = (int)&printmem;
		task_mem->tss.es = 1 * 8;
		task_mem->tss.cs = 2 * 8;
		task_mem->tss.ss = 1 * 8;
		task_mem->tss.ds = 1 * 8;
		task_mem->tss.fs = 1 * 8;
		task_mem->tss.gs = 1 * 8;
		task_run(task_mem, 1, 2);
	} else if (cmdline[0] == 'c' && cmdline[1] == 'a' && cmdline[2] == 'l'){
		char ss[30];
		char *s = cmdline + 3;
		char *p;
		for (p = s; *p > ' '; p++) { }	/* 一直读到空格为止 */
		int i = getnum(&p, 9);
		if (i == INVALID) {
			cons_putstr0(cons, "Calc ERROR! Check your command!\n\n");
		} else {
			sprintf(ss, "= %d = 0x%x\n", i, i);
			cons_putstr0(cons, ss);
		}
	} else if (strcmp(cmdline, "switch2") == 0) {
		switch_mode = 2;
	} else if (strcmp(cmdline, "switch1") == 0) {
		switch_mode = 1;
	} else if (strcmp(cmdline, "init rw") == 0) {
		create_cpt();
	} else if (strcmp(cmdline,"read")==0) {
		//Reader(cons1);
		//mm.waiting1+=5;
		struct TASK *task233 = task_alloc();
		int *cons_fifo = (int *)memman_alloc_4k(memman, 128 * 4);
		task233->cons_stack = memman_alloc_4k(memman, 64 * 1024);
		task233->tss.esp = task233->cons_stack + 64 * 1024 - 12;
		task233->tss.eip = (int)&Read;
		task233->tss.es = 1 * 8;
		task233->tss.cs = 2 * 8;
		task233->tss.ss = 1 * 8;
		task233->tss.ds = 1 * 8;
		task233->tss.fs = 1 * 8;
		task233->tss.gs = 1 * 8;
		*((int *)(task233->tss.esp + 4)) = (int)key_win1;
		*((int *)(task233->tss.esp + 8)) = memtotal;
		task_run(task233, 2, 1); 
		fifo32_init(&task233->fifo, 128, cons_fifo, task233);
	} else if (strcmp(cmdline, "write") == 0) {
		//Write(cons1);
		//mm.waiting2+=10;
		struct TASK *task234 = task_alloc();
		int *cons_fifo = (int *)memman_alloc_4k(memman, 128 * 4);
		task234->cons_stack = memman_alloc_4k(memman, 64 * 1024);
		task234->tss.esp = task234->cons_stack + 64 * 1024 - 12;
		task234->tss.eip = (int)&Write;
		task234->tss.es = 1 * 8;
		task234->tss.cs = 2 * 8;
		task234->tss.ss = 1 * 8;
		task234->tss.ds = 1 * 8;
		task234->tss.fs = 1 * 8;
		task234->tss.gs = 1 * 8;
		*((int *)(task234->tss.esp + 4)) = (int)key_win1;
		*((int *)(task234->tss.esp + 8)) = memtotal;
		task_run(task234, 2, 2); /* level=2, priority=2 */
		fifo32_init(&task234->fifo, 128, cons_fifo, task234);
	} else if (strncmp(cmdline, "mkf ", 4) == 0) {// wzh
		cmd_mkf(cons, cmdline);
		cons_newline(cons);
	} else if (strcmp(cmdline, "dir a") == 0) {
		cmd_dir(cons);
	} else if (strcmp(cmdline, "dir") == 0 || strcmp(cmdline, "ls") == 0) {
		cmd_ls(cons);
	} else if (strncmp(cmdline, "mkd ", 4) == 0) {
		cmd_mkd(cons, fat, cmdline);
		cons_newline(cons);
	} else if (strncmp(cmdline, "finf ", 5) == 0) {
		cmd_finf(cons, cmdline);
		cons_newline(cons);
	} else if (strncmp(cmdline, "infc ", 5) == 0) {
		cmd_infc(cons, cmdline);
		cons_newline(cons);
	} else if (strcmp(cmdline, "showfat") == 0) {
		show_fat(cons, fat);
		cons_newline(cons);
	} else if (strcmp(cmdline, "test disk") == 0) {		//*2 test
		test_disk(cons, fat);
	} else if (strcmp(cmdline, "test encoding") == 0) {	//*2 test
		cmd_fat_test(cons, fat);
	} else if (strncmp(cmdline, "del ", 4) == 0) {
		cmd_del(cons, fat, cmdline);
		cons_newline(cons);
	} else if (strncmp(cmdline, "cd ", 3) == 0) {
		cmd_cd(cons, cmdline, fat);
		cons_newline(cons);
	} else if (cmdline[0] != 0) {
		if (cmd_app(cons, fat, cmdline) == 0) {
			/*不是命令，不是应用程序，也不是空行*/
			cons_putstr0(cons, "Bad command.\n\n");
		}
	}
	return;
}

void LevelInfo(int level,int y){
	char s[80];
	int x = 10;
	sprintf(s, "Task Level %d:",level);
	putfonts8_asc_sht(sht_win_ps, x, y, COL8_C6C6C6,COL8_000000 , s, 40); x += 8*13;
	struct TASK *tsk = 0;
	for(tsk=taskctl->level[level].tasks; tsk; tsk = tsk->next){
		sprintf(s, "->%s", tsk->name);
		putfonts8_asc_sht(sht_win_ps, x, y, COL8_C6C6C6,COL8_000000 , s, 40);
		x += 8*(strlen(tsk->name) + 2);
	}
}


void printInfo(void){
	struct FIFO32 fifo;

	int fifobuf[128];
	char s[64];

	fifo32_init(&fifo, 128, fifobuf, task_ps);
	struct TIMER *timer_ps;
	timer_ps = timer_alloc();
	timer_init(timer_ps, &fifo, 100);
	timer_settime(timer_ps, 10);

	int i,x,y;

	for(;;)
	{
		io_cli();
		if (fifo32_status(&fifo) == 0) {
			task_sleep(task_ps);
			io_sti();
		} else {
			fifo32_get(&fifo);
			timer_settime(timer_ps, 10);
			x = 10;
			y = 34;
			LevelInfo(0,y); y += 16;
			LevelInfo(1,y); y += 16;
			LevelInfo(2,y); y += 16*2;

			sprintf(s,"Now Level: %d Now Task: %d Mode = %d",taskctl->now_lv,taskctl->level[taskctl->now_lv].now->sel,switch_mode);
			putfonts8_asc_sht(sht_win_ps, x, y, COL8_C6C6C6,COL8_000000 , s, 50);	y+= 16*2;

			if(switch_mode == 1){
				sprintf(s,"Sel Level Time Flag  Name");
			} else{
				sprintf(s,"Sel Nice  Wait Flag  Name");
			}	
			putfonts8_asc_sht(sht_win_ps, x, y, COL8_C6C6C6,COL8_000000 , s, 40);	y+= 16;

			struct TASK *tsk;
			for(i=0; i<MAX_TASKS; i++){
				tsk = &taskctl->tasks0[i];
				if(tsk->flags == TASK_UNUSE){
					continue;
				}
				if (switch_mode == 1){
					sprintf(s, "%d   %d    %d    %d    %s" , tsk->sel, tsk->level, tsk->priority, tsk->flags, tsk->name);
				}
				else {
					sprintf(s, "%d   %d    %d    %d    %s", tsk->sel, tsk->nice, tsk->wait_time, tsk->flags, tsk->name);
				}
				putfonts8_asc_sht(sht_win_ps, x, y, COL8_C6C6C6, COL8_000000 , s, 40);	y+= 16;
			}
			while(y < sht_win_ps->bysize){
				putfonts8_asc_sht(sht_win_ps, x, y, COL8_C6C6C6, COL8_000000 , "       ", 40);
				y+= 16;
			}


			boxfill8(sht_win_ps->buf, sht_win_ps->bxsize, COL8_C6C6C6,
			 x, y, sht_win_ps->bxsize-x, sht_win_ps->bysize-y);
			//putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s);
			//sheet_refresh(sht, x, y, x + l * 8, y + 16);

		}
	}
}



void task_counter(struct SHEET *sht_win_b)
{
	struct FIFO32 fifo;
	struct TIMER *timer_1s;
	int i, fifobuf[128], count = 0;
	char s[40];

	fifo32_init(&fifo, 128, fifobuf, 0);
	timer_1s = timer_alloc();
	timer_init(timer_1s, &fifo, 100);
	timer_settime(timer_1s, 100);

	for (;;) {
		count++;
		io_cli();
		if (fifo32_status(&fifo) == 0) {
			io_sti();
		} else {
			i = fifo32_get(&fifo);
			io_sti();
			if (i == 100) {
				sprintf(s, "speed =%7dw/s", count/10000);
				putfonts8_asc_sht(sht_win_b, 24, 28, COL8_C6C6C6,COL8_000000 , s, 18);
				count = 0;
				timer_settime(timer_1s, 100);
			}
		}
	}
}

void cmd_mem(struct CONSOLE *cons, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char s[60];
	sprintf(s, "total %dMB\nfree %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	cons_putstr0(cons, s);
	return;
}

void cmd_cls(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	for (y = 28; y < 28 + 128; y++) {
		for (x = 8; x < 8 + 240; x++) {
			sheet->buf[x + y * sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
	cons->cur_y = 28;
	return;
}

void cmd_type(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo = file_search(cmdline + 5, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	char *p;
	if (finfo != 0) {
		/*找到文件的情况*/
		p = (char *) memman_alloc_4k(memman, finfo->size);
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		cons_putstr1(cons, p, finfo->size);
		memman_free_4k(memman, (unsigned int) p, finfo->size); 
	} else {
		/*没有找到文件的情况*/
		cons_putstr0(cons, "File not found.\n");
	}
	cons_newline(cons);
	return;
}

int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	char name[18], *p, *q;
	struct TASK *task = task_now();
	int i, segsiz, datsiz, esp, dathrb;

	/*根据命令行生成文件名*/
	for (i = 0; i < 13; i++) {
		if (cmdline[i] <= ' ') {
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0; /*暂且将文件名的后面置为0*/

	/*寻找文件 */
	finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo == 0 && name[i -1]!= '.') {
		/*由于找不到文件，故在文件名后面加上“.hrb”后重新寻找*/
		name[i ] = '.';
		name[i + 1] = 'H';
		name[i + 2] = 'R';
		name[i + 3] = 'B';
		name[i + 4] = 0;
		finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	}

	if (finfo != 0) {
		/*找到文件的情况*/
		p = (char *) memman_alloc_4k(memman, finfo->size);
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		if (finfo->size >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00) {
			segsiz = *((int *) (p + 0x0000));
			esp    = *((int *) (p + 0x000c));
			datsiz = *((int *) (p + 0x0010));
			dathrb = *((int *) (p + 0x0014));
			q = (char *) memman_alloc_4k(memman, segsiz);
			*((int *) 0xfe8) = (int) q;
			set_segmdesc(gdt + 1003, finfo->size - 1, (int) p, AR_CODE32_ER + 0x60);
			set_segmdesc(gdt + 1004, segsiz - 1,      (int) q, AR_DATA32_RW + 0x60);
			for (i = 0; i < datsiz; i++) {
				q[esp + i] = p[dathrb + i];
			}
			start_app(0x1b, 1003 * 8, esp, 1004 * 8, &(task->tss.esp0));
			memman_free_4k(memman, (unsigned int) q, segsiz);
		} else {
			cons_putstr0(cons, ".hrb file format error.\n");
		}
		memman_free_4k(memman, (unsigned int) p, finfo->size);
		cons_newline(cons);
		return 1;
	}
	/*没有找到文件的情况*/
	return 0;
}

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	int ds_base = *((int *) 0xfe8);
	struct TASK *task = task_now();
	struct CONSOLE *cons = (struct CONSOLE *) *((int *) 0x0fec);
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht;
	int *reg = &eax + 1; /* eax后面的地址*/
	/*强行改写通过PUSHAD保存的值*/
	/* reg[0] : EDI, reg[1] : ESI, reg[2] : EBP, reg[3] : ESP */
	/* reg[4] : EBX, reg[5] : EDX, reg[6] : ECX, reg[7] : EAX */
	if (edx == 1) {
		cons_putchar(cons, eax & 0xff, 1);
	} else if (edx == 2) {
		cons_putstr0(cons, (char *) ebx + ds_base);
	} else if (edx == 3) {
		cons_putstr1(cons, (char *) ebx + ds_base, ecx);
	} else if (edx == 4) {
		return &(task->tss.esp0);
	} else if (edx == 5) {
		sht = sheet_alloc(shtctl);
		sheet_setbuf(sht, (short *) ebx + ds_base, esi, edi, eax);
		make_window8((short *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
		sheet_slide(sht, 100, 50);
		sheet_updown(sht, 3); /*背景层高度3位于task_a之上*/
		reg[7] = (int) sht; 
	} else if (edx == 6) {
		sht = (struct SHEET *) ebx;
		putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
		sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
	} else if (edx == 7) {
		sht = (struct SHEET *) ebx;
		boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
	}
	return 0;
}

int *inthandler0c(int *esp)
{
	struct CONSOLE *cons = (struct CONSOLE *) *((int *) 0x0fec);
	struct TASK *task = task_now();
	char s[30];
	cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0); /*强制结束程序*/
}

int *inthandler0d(int *esp)
{
	struct CONSOLE *cons = (struct CONSOLE *) *((int *) 0x0fec);
	struct TASK *task = task_now();
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0);	/*强制结束程序*/
}




char *skipspace(char *p)
{
	for (; *p == ' '; p++) { }	/* 将空格跳过去 */
	return p;
}

int getnum(char **pp, int priority)
{
	char *p = *pp;
	int i = INVALID, j;
	p = skipspace(p);

	/*单项运算符*/
	if (*p == '+') {
		p = skipspace(p + 1);
		i = getnum(&p, 0);
	} else if (*p == '-') {
		p = skipspace(p + 1);
		i = getnum(&p, 0);
		if (i != INVALID) {
			i = - i;
		}
	} else if (*p == '~') {
		p = skipspace(p + 1);
		i = getnum(&p, 0);
		if (i != INVALID) {
			i = ~i;
		}
	} else if (*p == '(') { /*括号*/
		p = skipspace(p + 1);
		i = getnum(&p, 9);
		if (*p == ')') {
			p = skipspace(p + 1);
		} else {
			i = INVALID;
		}
	} else if ('0' <= *p && *p <= '9') { /*数值*/
		i = strtol(p, &p, 0);
	} else { /*错误 */
		i = INVALID;
	}

	/*二项运算符*/
	for (;;) {
		if (i == INVALID) {
			break;
		}
		p = skipspace(p);
		if (*p == '+' && priority > 2) {
			p = skipspace(p + 1);
			j = getnum(&p, 2);
			if (j != INVALID) {
				i += j;
			} else {
				i = INVALID;
			}
		} else if (*p == '-' && priority > 2) {
			p = skipspace(p + 1);
			j = getnum(&p, 2);
			if (j != INVALID) {
				i -= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '*' && priority > 1) {
			p = skipspace(p + 1);
			j = getnum(&p, 1);
			if (j != INVALID) {
				i *= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '/' && priority > 1) {
			p = skipspace(p + 1);
			j = getnum(&p, 1);
			if (j != INVALID && j != 0) {
				i /= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '%' && priority > 1) {
			p = skipspace(p + 1);
			j = getnum(&p, 1);
			if (j != INVALID && j != 0) {
				i %= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '<' && p[1] == '<' && priority > 3) {
			p = skipspace(p + 2);
			j = getnum(&p, 3);
			if (j != INVALID && j != 0) {
				i <<= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '>' && p[1] == '>' && priority > 3) {
			p = skipspace(p + 2);
			j = getnum(&p, 3);
			if (j != INVALID && j != 0) {
				i >>= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '&' && priority > 4) {
			p = skipspace(p + 1);
			j = getnum(&p, 4);
			if (j != INVALID) {
				i &= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '^' && priority > 5) {
			p = skipspace(p + 1);
			j = getnum(&p, 5);
			if (j != INVALID) {
				i ^= j;
			} else {
				i = INVALID;
			}
		} else if (*p == '|' && priority > 6) {
			p = skipspace(p + 1);
			j = getnum(&p, 6);
			if (j != INVALID) {
				i |= j;
			} else {
				i = INVALID;
			}
		} else {
			break;
		}
	}
	p = skipspace(p);
	*pp = p;
	return i;
}

void printmem(void)
{
	struct FIFO32 fifo;

	int fifobuf[128];
	char s[64];

	fifo32_init(&fifo, 128, fifobuf, task_mem);
	struct TIMER *timer_ps;
	timer_ps = timer_alloc();
	timer_init(timer_ps, &fifo, 100);
	timer_settime(timer_ps, 10);

	int  x, y;
	for (;;)
	{
		if (fifo32_status(&fifo) == 0) {
			task_sleep(task_mem);
		}
		else {
			fifo32_get(&fifo);
			timer_settime(timer_ps, 10);
			x = 10;
			y = 34;

			sprintf(s, "Total of memory is: %d ", memtotal);
			putfonts8_asc_sht(sht_win_ps1, x, y, COL8_C6C6C6, COL8_000000, s, 40);	y += 16 * 2;
			//meminfo(y);	
			sprintf(s, "Start of memory is: %d ", 0x00400000);
			putfonts8_asc_sht(sht_win_ps1, x, y, COL8_C6C6C6, COL8_000000, s, 40);	y += 16 * 2;

			sprintf(s, "Now total of memory is: %d ", memman_total(memman));
			putfonts8_asc_sht(sht_win_ps1, x, y, COL8_C6C6C6, COL8_000000, s, 40);  y += 16 * 2;

			boxfill8(sht_win_ps1->buf, sht_win_ps1->bxsize, COL8_C6C6C6,
				x, y, sht_win_ps1->bxsize - x, sht_win_ps1->bysize - y);
			//putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s);
			//sheet_refresh(sht, x, y, x + l * 8, y + 16);

		}
	}
}



void create_cpt()
{
	//设置书本总数
	book = 20;
	//开一个新的命令行任务（指回命令窗口等待下一个命令）
	key_win1 = open_console1(shtctl, mem4con);
	sheet_slide(key_win1, 30, 60 + 168);
	sheet_updown(key_win1, shtctl->top);
	//创建一个用来显示竞争信息的命令窗口
	cons1->sht = key_win1;
	cons1->cur_x = 5;
	cons1->cur_y = 30;
	cons1->cur_c = -1;
	/*********************************************************/
	cons_putstr0(cons1, "have 20 books!\n");
}
void Read()
{
	char s[60];
	int hh = 65;
	int MyBook = 0;
	while (1)
	{
		if (book > 0)
		{
			MyBook = book;
			sprintf(s, "%d book(s) remain \n", MyBook);
			cons_putstr0(cons1, s);
			sprintf(s, "Waiting, Let me take the %d book to you !\n", MyBook);
			//cons_putstr0(cons1, s);
			//相当于一个定时器，放慢刷新速度
			int temp = 0;
			while (hh > 5)
			{
				if (timerctl.count != temp)
				{
					temp = timerctl.count;
					sprintf(s, "LOG temp: %d  count: %d hh: %d \n", temp, timerctl.count, hh);
					//cons_putstr0(cons1, s);
					hh--;
				}
			}
			hh = 65;
			//更新抢到的书的编号
			MyBook = book;
			//将书本数量减少
			book--;
			
			//输出信息
			sprintf(s, "you read the %d book\n", MyBook);
			cons_putstr0(cons1, s);
			//检查还有没有书
			if (book == 0)
			{
				sprintf(s, "no book remain, sorry\n", book);
				cons_putstr0(cons1, s);
				break;
			}
			else if (book < 0)
			{
				sprintf(s, "may be competition appeared\n");
				cons_putstr0(cons1, s);
				break;
			}
			sprintf(s, "Now %d book(s) remain \n\n", book);
			//cons_putstr0(cons1, s);
		}
		else if (book == 0)
		{
			sprintf(s, "no book remain, sorry\n", book);
			cons_putstr0(cons1, s);
			break;
		}
		else
		{
			sprintf(s, "may be competition appeared\n");
			cons_putstr0(cons1, s);
			break;
		}
	}
	//让这个任务不结束
	int a = 0;
	while (1) { a = 1; };
}

void Write()
{
	char s[60];
	int hh = 65;
	int MyBook = 0;
	if (fla != 1) {
		fla++;
	}
	//如果还没对mutex初始化
	if (mutex_init_t == 0) {
		//初始化mutex
		mutex_init(&mm);
		mutex_init_t = 1;
	}
	//增大竞争的人数
	mm.size++;
	int wi = fla - 1;
	while (1)
	{
		//等待允许写
		wait(&mm, wi);
		if (book > 0)
		{
			MyBook = book;
			sprintf(s, "%d book(s) remain \n", MyBook);
			cons_putstr0(cons1, s);
			sprintf(s, "Waiting, Let me take the %d book to you !\n", MyBook);
			//cons_putstr0(cons1, s);
			//放慢刷新速度
			int temp = 0;
			while (hh > 5)
			{
				if (timerctl.count != temp)
				{
					temp = timerctl.count;
					sprintf(s, "LOG temp: %d  count: %d hh: %d \n", temp, timerctl.count, hh);
					//cons_putstr0(cons, s);
					hh--;
				}
			}
			hh = 65;
			//更新当前抢到的书的编号
			MyBook = book;
			book--;
			//检查还有没有书
			if (book == 0)
			{
				sprintf(s, "no book remain, sorry\n", book);
				cons_putstr0(cons1, s);
				break;
			}
			else if(book<0)
			{
				sprintf(s, "may be competition appeared\n");
				cons_putstr0(cons1, s);
				break;
			}
			sprintf(s, "you write the %d book\n", MyBook);
			cons_putstr0(cons1, s);
			sprintf(s, "Now %d book(s) remain \n\n", book);
			//cons_putstr0(cons1, s);
		}//如果写者都写完了，就放开限制
		else if (book == 0)
		{
			sprintf(s, "no book remain, sorry\n", book);
			cons_putstr0(cons1, s);
			signal(&mm);
			break;

		}
		else if(book<0)
		{
			sprintf(s, "may be competition appeared\n");
			cons_putstr0(cons1, s);
			signal(&mm);
			break;

		}
		//允许下一次的写
		signal(&mm);
	}
	int b = 0;
	while (1) { b = 1; }
}


/* 吴志恒 */
void cmd_dir(struct CONSOLE *cons)					//*
{

	struct FILEINFO *finfo = cons->dir_info->adr_dir;
	int i, j;
	char s[30];
	for (i = 0; i < cons->dir_info->maxsize; i++) {	//* maxsize为该目录下最大条目数 
		if (finfo[i].name[0] == 0x00) {				//* 有效文件头信息末尾，finfo为当前目录下文件头信息的起始地址
			break;
		}
		if (finfo[i].name[0] != 0xe5) {				//* 文件已删除		
			if ((finfo[i].type & 0x08) == 0) {		//*2 显示（所有）文件 
				sprintf(s, "filename.ext   %7d\n", finfo[i].size);
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				if ((finfo[i].type & 0x10) != 0) {
					s[9] = 'D';
					s[10] = 'I';
					s[11] = 'R';
				}
				else {
					s[9] = finfo[i].ext[0];
					s[10] = finfo[i].ext[1];
					s[11] = finfo[i].ext[2];
				}
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}
void cmd_ls(struct CONSOLE *cons)					//*
{

	struct FILEINFO *finfo = cons->dir_info->adr_dir;
	int i, j;
	char s[30];
	for (i = 0; i < cons->dir_info->maxsize; i++) {	//* maxsize为该目录下最大条目数 
		if (finfo[i].name[0] == 0x00) {				//* 有效文件头信息末尾，finfo为当前目录下文件头信息的起始地址
			break;
		}
		if (finfo[i].name[0] != 0xe5) {				//* 文件已删除		
			if ((finfo[i].type & 0x0a) == 0) {		//*2 显示（非隐藏）文件 
				sprintf(s, "filename.ext   %7d\n", finfo[i].size);
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				if ((finfo[i].type & 0x10) != 0) {
					s[9] = 'D';
					s[10] = 'I';
					s[11] = 'R';
				}
				else {
					s[9] = finfo[i].ext[0];
					s[10] = finfo[i].ext[1];
					s[11] = finfo[i].ext[2];
				}
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

void finfo_init(struct FILEINFO * finfo) {		// 初始化name和ext
	int i;
	for (i = 0; i < 8; i++) {
		finfo->name[i] = ' ';
	}
	for (i = 0; i < 3; i++) {
		finfo->ext[i] = ' ';
	}
	for (i = 0; i < 10; i++) {
		finfo->reserve[i] = 0;
	}
	finfo->size = (unsigned int)0;
	finfo->clustno = (unsigned short)0;
	finfo->date = (unsigned short)0;
	finfo->time = (unsigned short)0;
	finfo->type = 0x00;
}

/* 创建目录 */									//**
void cmd_mkd(struct CONSOLE *cons, int *fat, char *cmdline) {
	cmdline += 4;
	struct FILEINFO mkd;
	int i;
	char name[12];
	for (i = 0;; i++) {								// 判断文件名是否过长
		if (cmdline[i] == ' ' || cmdline[i] == 0) { break; }
		if (i >= 8) {
			cons_putstr0(cons, "file name too long!\n");
			return;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0;
	struct FILEINFO *fifi = file_search(name, cons->dir_info->adr_dir, cons->dir_info->maxsize);
	if (fifi != 0) {							// 文件（目录）存在
		cons_putstr0(cons, "The file exits!");
		return;
	}
	finfo_init(&mkd);
	for (i = 0; i < 8; i++) {
		if (cmdline[i] == 0 || cmdline[i] == ' ') { break; }
		mkd.name[i] = cmdline[i];
	}
	if (cmdline[i] != 0) {		// 目录属性
		if (cmdline[i + 1] == 'u') {	// 隐藏
			mkd.type = 0x02 | 0x10;
		}
		else if (cmdline[i + 1] == ' ' || cmdline[i + 1] == 0) {
			mkd.type = 0x10;
		}
		else {
			mkd.type = 0x10;
			cons_putstr0(cons, "type error\n");
		}
	}
	else {													// 默认可读可写
		mkd.type = 0x10;
	}
	fifi = file_addfile(cons->dir_info->adr_dir, &mkd, cons->dir_info->maxsize);
	if (fifi == 0) {	// 目录增加失败
		cons_putstr0(cons, "Can't add the file ");
		cons_putstr0(cons, cmdline);
		cons_newline(cons);
	}
	else {
		/*
		为文件目录分配扇区
		决定该目录size
		*/
		int clustno = allo_fat(fat);
		if (clustno == -1) {							//* 扇区分配失败
			cons_putstr0(cons, "Cluster allocation failed!\n");
			return;
		}
		else {
			fifi->clustno = clustno;
			fifi->size = 512;
		}
	}
	// 写回磁盘
	unsigned int size = cons->dir_info->maxsize * 32, clustno;
	clustno = cons->dir_info->clustno;
	dsk_fat_write(fat);
	char res = dsk_write(fat, clustno, (char *)cons->dir_info->adr_dir, size);
	if (res != 0) {
		cons_putstr0(cons, "can not write disk!!\n");
		return;
	}
	cons_putstr0(cons, "Adding successful.\n");
	return;
}

/* 创建文件 */									//**
void cmd_mkf(struct CONSOLE *cons, char *cmdline) {
	cmdline += 4;
	struct FILEINFO mkf;
	int i, j;
	char name[12];
	for (i = 0;; i++) {								// 判断文件名是否过长
		if (cmdline[i] == ' ' || cmdline[i] == 0) { break; }
		if (i >= 12) {
			cons_putstr0(cons, "file name too long!\n");
			return;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0;
	struct FILEINFO *fifi = file_search(name, cons->dir_info->adr_dir, cons->dir_info->maxsize);
	if (fifi != 0) {
		cons_putstr0(cons, "file exits!");
		return;
	}
	finfo_init(&mkf);
	for (i = 0;; i++) {				// 文件名
		if (cmdline[i] == ' ') { goto filtyp; }
		if (cmdline[i] == '.') { break; }
		if (cmdline[i] == 0) { goto write; }
		if (i == 8) {
			cons_putstr0(cons, "error! name too long!\n");
			return;
		}
		mkf.name[i] = cmdline[i];
	}
	for (j = 0;; j++) {
		if (cmdline[i + 1 + j] == ' ') { break; }
		if (cmdline[i + 1 + j] == 0) { goto write; }
		if (j == 3) {
			cons_putstr0(cons, "error! suffix too long!\n");
			return;
		}
		mkf.ext[j] = cmdline[i + j + 1];
	}
	i += j + 1;
filtyp:
	i++;
	char type = get_type(cmdline + i);
	if (type == 0x80) {
		mkf.type = 0x00;
		cons_putstr0(cons, "type error\n");
	}
	else if ((type & 0x10) != 0) {
		cons_putstr0(cons, "get_type error!");
		return;
	}
	else {
		mkf.type = type;
	}
write:
	fifi = file_addfile(cons->dir_info->adr_dir, &mkf, cons->dir_info->maxsize);
	if (fifi == 0) {							// 增加失败
		cons_putstr0(cons, "Can't add the file ");
		cons_putstr0(cons, cmdline);
	}
	else {
		cons_putstr0(cons, "Adding successful.\n");
	}
}

/* 删除文件 */									//**
void cmd_del(struct CONSOLE *cons, int *fat, char *cmdline) {
	int i;
	struct FILEINFO *delf;
	char name[13];
	/*根据命令行生成文件名*/
	for (i = 0; i < 13; i++) {
		if (cmdline[i + 4] <= ' ' || cmdline[i + 4] == 0) {
			break;
		}
		name[i] = cmdline[i + 4];
	}
	name[i] = 0; /*暂且将文件名的后面置为0*/
	delf = file_search(name, cons->dir_info->adr_dir, 224);
	if (delf != 0) {
		if (file_remove(fat, delf) == 0) {
			cons_putstr0(cons, "Removing failed!\n");		// 删除失败
			return;
		}
	}
	else {
		cons_putstr0(cons, "Can't find the file ");
		cons_putstr0(cons, name);				// 删除失败
		return;
	}
	// 写回磁盘
	unsigned int size = cons->dir_info->maxsize * 32, clustno;
	clustno = cons->dir_info->clustno;
	dsk_fat_write(fat);
	char res = dsk_write(fat, clustno, (char *)cons->dir_info->adr_dir, size);
	if (res != 0) {
		cons_putstr0(cons, "can not write disk!!\n");
		return;
	}
	cons_putstr0(cons, "Delete successful.\n");
}

void cmd_cd(struct CONSOLE *cons, char *cmdline, int *fat) {
	char comm[128];
	int i;
	for (i = 0; i < 128; ++i) {
		comm[i] = 0;
	}
	cmdline += 3;
	if (strcmp(cmdline, "../") == 0) { // 回到上层目录
		cd_up(cons);
	}
	else if (strncmp(cmdline, "./", 2) == 0) {	// 跳转到当前目录的子目录
		if (cd_down(cons, cmdline + 2, fat) != 0) {
			cons_putstr0(cons, "path error!");
			cons_newline(cons);
		}
	}
	else {
		cons_putstr0(cons, "Path error!\n");
	}
	cons_newline(cons);
	return;
}

void cmd_finf(struct CONSOLE *cons, char *cmdline) {	// 查看文件属性
	cmdline += 5;
	struct FILEINFO *finfo = file_search(cmdline, cons->dir_info->adr_dir, cons->dir_info->maxsize);
	if (finfo == 0) {
		cons_putstr0(cons, "file not exits!\n");
		return;
	}
	char line[30], type[4];
	// filename
	sprintf(line, "Name: %s", cmdline); //*??
	cons_putstr0(cons, line);
	cons_newline(cons);
	// type
	if ((finfo->type & 0x10) != 0) { type[0] = 'd'; }		// 文件或目录
	else { type[0] = 'f'; }
	if ((finfo->type & 0x01) != 0) { type[1] = 'r'; }	// 只读
	else { type[1] = '-'; }
	if ((finfo->type & 0x02) != 0) { type[2] = 'u'; }		// 隐藏
	else { type[2] = '-'; }
	type[3] = 0;
	sprintf(line, "\tType: %s", type); //*??
	cons_putstr0(cons, line);
	cons_newline(cons);
	// size
	sprintf(line, "\tSize: %7d", finfo->size); //*??
	cons_putstr0(cons, line);
	cons_newline(cons);
	//// time
	//sprintf(line, "File: %7d", finfo->size); //*??
	//cons_putstr0(cons, line);
	//cons_newline;
	return;
}

void cmd_infc(struct CONSOLE *cons, char *cmdline) {	// 文件属性修改
	cmdline += 5;
	char name[12], type;
	int i;
	for (i = 0;; i++) {
		if (cmdline[i] == ' ') { break; }
		if (cmdline[i] == 0) { return; }
		if (i >= 12) {
			cons_putstr0(cons, "name too long!");
			return;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0;
	struct FILEINFO *finfo = file_search(name, cons->dir_info->adr_dir, cons->dir_info->maxsize);
	if (finfo == 0) {
		cons_putstr0(cons, "file not exits!\n");
		return;
	}
	type = get_type(cmdline + 1 + i);
	if ((type & 0x80) != 0) {
		cons_putstr0(cons, "type error!");
		return;
	}
	if ((finfo->type & 0x10) != 0) {
		type |= 0x10;
	}
	finfo->type = type;

	return;
}
//void cmd_edi(){ // 编辑文本
//return;
//}
