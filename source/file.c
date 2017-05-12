#include "bootpack.h"
#include "string.h"					//** 使用memcpy(()
/* 文件相关函数 */


//static struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

extern struct MEMMAN *memman;

void file_readfat(int *fat, unsigned char *img)
/*将磁盘映像中的FAT解压缩 */
{
	int i, j = 0;
	for (i = 0; i < MAX_FAT; i += 2) {
		fat[i + 0] = (img[j + 0] | img[j + 1] << 8) & 0xfff;
		fat[i + 1] = (img[j + 1] >> 4 | img[j + 2] << 4) & 0xfff;
		j += 3;
	}
	return;
}

void encoding_fat(int *fat, unsigned char *img_fat) {	//*2 fat编码
	int i, j = 0;
	for (i = 0; i < MAX_FAT; i += 2) {
		img_fat[j] = fat[i] & 0xff;
		img_fat[j + 1] = (fat[i] >> 8 | fat[i + 1] << 4) & 0xff;
		img_fat[j + 2] = (fat[i + 1] >> 4) & 0xff;
		j += 3;
	}
	return;
}


void file_loadfile(int clustno, int size, char *buf, int *fat, char *img)
{
	int i;
	for (;;) {
		if (size <= 512) {
			for (i = 0; i < size; i++) {
				buf[i] = img[clustno * 512 + i];
			}
			break;
		}
		for (i = 0; i < 512; i++) {
			buf[i] = img[clustno * 512 + i];
		}
		size -= 512;
		buf += 512;
		clustno = fat[clustno];
	}
	return;
}

char *file_loadfile2(int clustno, int *psize, int *fat)
{
	int size = *psize, size2;
	char *buf, *buf2;
	buf = (char *) memman_alloc_4k(memman, size);
	file_loadfile(clustno, size, buf, fat, (char *) (ADR_DISKIMG + 0x003e00));
	if (size >= 17) {
		size2 = tek_getsize(buf);
		if (size2 > 0) {	/* */
			buf2 = (char *) memman_alloc_4k(memman, size2);
			tek_decomp(buf, buf2, size2);
			memman_free_4k(memman, (int) buf, size);
			buf = buf2;
			*psize = size2;
		}
	}
	return buf;
}


struct FILEINFO *file_search(char *name, struct FILEINFO *finfo, int max)
{
	int i, j;
	char s[12];
	for (j = 0; j < 11; j++) {
		s[j] = ' ';
	}
	j = 0;
	for (i = 0; name[i] != 0; i++) {
		if (j >= 11) { return 0; /*没有找到*/ }
		if (name[i] == '.' && j <= 8) {
			j = 8;
		} else {
			s[j] = name[i];
			if ('a' <= s[j] && s[j] <= 'z') {
				/*将小写字母转换为大写字母*/
				s[j] -= 0x20;
			}
			j++;
		}
	}
	for (i = 0; i < max; ) {
		if (finfo->name[0] == 0x00) {
			break;
		}
		if ((finfo[i].type & 0x08) == 0) {		//*2 
			for (j = 0; j < 11; j++) {
				if (finfo[i].name[j] != s[j]) {
					goto next;
				}
			}
			return finfo + i; /*找到文件*/
		}
	next:
		i++;
	}
	return 0; /*没有找到*/
}
/* wzh */

/* 增加文件 */														//**
struct FILEINFO * file_addfile(struct FILEINFO *finfo, struct FILEINFO *af, int max) {
	int i;
	for (i = 0; i < max; i++) {
		if (finfo[i].name[0] == 0 || finfo[i].name[0] == 0xe5) break;
	}
	if (i < max) {													// 找到合适的文件记录
		int j;
		for (j = 0; j<8; j++) {
			if (af->name[j] <= 'z' && af->name[j] >= 'a') {
				af->name[j] -= 0x20;
			}
		}
		for (j = 0; j<3; j++) {
			if (af->ext[j] <= 'z' && af->ext[j] >= 'a') {
				af->ext[j] -= 0x20;
			}
		}
		memcpy(finfo + i, af, sizeof(struct FILEINFO));
	}
	else { return 0; }					          					// 增加文件失败
	return finfo + i;														// 成功
}
/* 删除文件，释放内存与磁盘空间，若是目录，则删除目录后一下的记录*/
int file_remove(int *fat, struct FILEINFO *delf) {
	int i;
	if ((delf->type & 0x10) != 0) {		//* 删除文件为目录
		char * buf = memman_alloc(memman, delf->size);
		file_loadfile(delf->clustno, delf->size, buf, fat, (char *)(ADR_DISKIMG + 0x003e00));
		struct FILEINFO *subf = (struct FILEINFO *)buf;
		int submax = delf->size / 32;
		for (i = 0; i < submax; i++) {
			if (subf[i].name[0] == 0x00) { break; }
			if (subf[i].name[0] == 0xe5) { continue; }
			file_remove(fat, subf);
		}
		memman_free(memman, buf, delf->size);
	}
	if (delf->size > 0) {
		int j;
		for (i = delf->clustno; i<2880;) {
			if (fat[i] >= 0xff8) {
				fat[i] = 0;
				break;
			}
			j = fat[i];
			fat[i] = 0;
			i = j;
		}
		if (i >= 2880) { return 0; } //* fat异常
	}
	delf->name[0] = 0xe5;
	return 1; //* 成功删除
}



/*  allo_fat(int *fat)
分配扇区，并在fat上注册，置为0xfff
返回分配的扇区号
返回-1则分配失败
*/
int allo_fat(int *fat) {				//* 分配空的扇区，在fat上注册
	int i;
	for (i = 0; i < MAX_FAT; i++) {
		if (fat[i] <= 0) {
			fat[i] = 0xfff;
			return i;					//* 分配扇区号为i
		}
	}
	return -1;	//* 分配失败
}
// ----------------------- directory----------------------
void cd_up(struct CONSOLE *cons) {

	if (cons->dir_info->adr_parent == 0) {
		cons_putstr0(cons, "It is top directory!\n");
		return;
	}
	struct DIRINFO *par = cons->dir_info;						//* 本级目录信息
	char *buf = (char *)cons->dir_info->adr_dir;
	memman_free(memman, buf, cons->dir_info->maxsize * sizeof(struct FILEINFO));	//* 释放目录内存
	cons->dir_info = cons->dir_info->adr_parent;				//* 切换到上级目录起始地址
	memman_free(memman, (char *)par, sizeof(struct DIRINFO));		//* 删除本级目录信息
	char str[30];
	sprintf(str, "current dir %s\n", cons->dir_info->name);
	cons_putstr0(cons, str);
	return;
}

int cd_down(struct CONSOLE *cons, char *dirname, int *fat) {
	int i;
	char name[10];
	for (i = 0; i < 10; i++) {
		if (dirname[i] == 0 || dirname[i] == ' ' || dirname[i] == '/') {
			break;
		}
		name[i] = dirname[i];
	}
	name[i] = 0;
	if (i == 9) {
		cons_putstr0(cons, "DIRNAME too long!\n");
		return 1;
	}
	struct DIRINFO *subdir;
	char *buf;
	struct FILEINFO * finfo = file_search(name, cons->dir_info->adr_dir, cons->dir_info->maxsize);
	if (finfo == 0) {														//* 目录不存来
		return 1;
	}
	if ((finfo->type & 0x10) == 0 || finfo->size == 0) {					//* 不是目录???存疑
		return 1;
	}
	buf = memman_alloc(memman, finfo->size);								//*		
	file_loadfile(finfo->clustno, finfo->size, buf, fat, (char *)(ADR_DISKIMG + 0x003e00));
	subdir = (struct DIRINFO *) memman_alloc(memman, sizeof(struct DIRINFO)); //* 子目录
	subdir->adr_parent = cons->dir_info;	// 初始化子目录的信息
	subdir->maxsize = finfo->size / 32;
	subdir->clustno = finfo->clustno;
	sprintf(subdir->name, name);
	subdir->adr_dir = (struct FILEINFO *)buf;
	cons->dir_info = subdir;
	char str[30];
	sprintf(str, "current dir %s\n", cons->dir_info->name);
	cons_putstr0(cons, str);	// 显示当前目录
	if (dirname[i] == '/') {	// 如果还有下一层目录，则进去
		return cd_down(cons, dirname + i + 1, fat);
	}
	return 0;
}

char get_type(char *cmdline) {	//*2 根据用户的输入得到有效的文件属性
	int i;
	char type = 0x00;
	for (i = 0; cmdline[i] != 0 && cmdline[i] != ' '; i++) {
		if (cmdline[i] == 'u') {
			type |= 0x02;
		}
		else if (cmdline[i] == 'r') {
			type |= 0x01;
		}
		else if (cmdline[i] == 'w') {
			type &= 0xfe;
		}
		else if (cmdline[i] == 0) {
			return type;
		}
		else {
			type = 0x80;		//*2 输入类型错误 因为这个值没有被使用，所以用这个值返回做判断
			return type;
		}
	}
	return type;
}



void show_fat(struct CONSOLE *cons, int *fat) {
	int i;
	char str[301];	//显示前300条fat记录
	for (i = 0; i < 300; i++) {
		if (fat[i] <= 0) {
			str[i] = '0';
		}
		else if (fat[i] >= 0xff8) {
			str[i] = '2';
		}
		else { str[i] = '1'; }
	}
	str[i] = 0;
	cons_putstr0(cons, str);
	cons_newline(cons);
}
//-----------------------------------test fat encoding---------------------------------
void cmd_fat_test(struct CONSOLE *cons, int *fat) {
	// 设计fat编解码的测试算法
	// 先编码，再解码，对比前后结果是否一致
	// 输出前300条记录
	unsigned char *img_fat = memman_alloc(memman, MAX_FAT);
	encoding_fat(fat, img_fat);
	if (test_encoding(fat, img_fat)) {
		cons_putstr0(cons, "encoding good\n");
	}
	else {
		cons_putstr0(cons, "encoding bad\n");
	}
	int *test_fat = (int *)memman_alloc(memman, sizeof(int)*MAX_FAT);
	file_readfat(test_fat, img_fat);
	cons_putstr0(cons, "fat: ");
	show_fat(cons, fat);
	cons_newline(cons);
	cons_putstr0(cons, "test fat: ");
	show_fat(cons, test_fat);
	memman_free(memman, (char *)test_fat, sizeof(int)*MAX_FAT);
	memman_free(memman, img_fat, MAX_FAT);
	cons_newline(cons);
}

int test_encoding(int *fat, char *img_fat) {		//*2 测试encoding 不正确则返回零
	int *test_fat = (int *)memman_alloc(memman, sizeof(int)*MAX_FAT);
	file_readfat(test_fat, img_fat);				//	解压缩
	if (strncmp((char *)test_fat, (char *)fat, sizeof(int)*MAX_FAT) == 0) {
		memman_free(memman, (char *)test_fat, sizeof(int)*MAX_FAT);
		return 1;
	}
	else {
		memman_free(memman, (char *)test_fat, sizeof(int)*MAX_FAT);
		return 0;
	}
}


//----------disk operation------------------
/* fat写回磁盘 */
char dsk_fat_write(int *fat) {
	unsigned int clustno = 2;
	unsigned char * fat_adr = memman_alloc(memman,sizeof(unsigned char) * MAX_FAT);
	encoding_fat(fat,fat_adr);	// 编码算法
	char res = dsk_write(fat, clustno, fat_adr, MAX_FAT);
	res = dsk_write(fat, clustno + 9, fat_adr, MAX_FAT);
	memman_free(memman, fat_adr,MAX_FAT);
	return 0;
}
/* 从磁盘读取fat */
char dsk_fat_read(int *fat) {
	unsigned int clustno = 2;
	unsigned char *img_fat = memman_alloc(memman, sizeof(unsigned char) * MAX_FAT);
	file_readfat(fat, img_fat);	// 解码算法
	char res = dsk_write(fat, clustno, img_fat, MAX_FAT);
	if (res == 0) {
		res = dsk_write(fat, clustno + 9, img_fat, MAX_FAT);
		if (res != 0) { 
			memman_free(memman, img_fat,MAX_FAT);
			return res; 
		}
	}
	else {
		memman_free(memman, img_fat,MAX_FAT);
		return res;
	}
	memman_free(memman, img_fat,MAX_FAT);
	return 0;
}

/* 写回磁盘 */
void wait_dsk() {	//判断磁盘是否可以传送命令
	while (1) {
		if (io_in8(0x1f7) & 0x40) {
			break;
		}
	}
}
//para：起始扇区号、 写回磁盘的内容的起始地址，内容大小
//return: 成功则返回零
char dsk_write(int *fat,unsigned int clustno,unsigned char *buf, unsigned int size) {
	int i,j;
	//char mask,state[10],cmp;
	short *adr = (short *)buf;
	unsigned int lbaadr = (unsigned int)clustno;
	//state[8] = 'w';
	//state[9] = 0;
	for (i = clustno; i < MAX_FAT;) {
		if (size <= 0 || i > MAX_FAT) { break; }
		//------------------test read first--------
		wait_dsk();
		io_out8(0x1f2, 0x01);					//操作的扇区个数 1
		wait_dsk();
		io_out8(0x1f3, lbaadr&0xff);			//低位lba地址
		wait_dsk();
		io_out8(0x1f4, (lbaadr >> 8) & 0xff);	//中位lba地址
		wait_dsk();
		io_out8(0x1f5, (lbaadr >> 16) & 0xff);	//高位lba地址
		wait_dsk();
		io_out8(0x1f6, 0xe0);					//剩余高位lba地址以及lba模式
		wait_dsk();
		io_out8(0x1f7, 0x30);					//写命令
		for (j = 0; j < 256; j++) {
			io_out16(0x1f0,adr[j]);
		}
		if ( fat[i] >= 0xffe) {
			break; // end
		}
		else if (fat[i] == 0) {
			return 255; // 错误，size和扇区数不对应
		}
		else {
			i = fat[i];
		}
		size -= 512;
		buf += 512;
	}
	return 0;
}

/* 读磁盘*/
//para：起始扇区号、 写回磁盘的内容的起始地址，内容大小
//return: 成功则返回零
char dsk_read(int *fat, unsigned int clustno, unsigned char *buf, unsigned int size) {
	int i,j,z;
	char state[10];
	unsigned char mask, cmp = 0x01;
	short data;
	unsigned int lbaadr = (unsigned int)clustno;
	state[8] = 'r';
	state[9] = 0;
	//read disk
	for (i = clustno; i < MAX_FAT;) {
		if (size <= 0 || i > MAX_FAT) { break; }
		wait_dsk();
		io_out8(0x1f2, 0x01);
		wait_dsk();
		io_out8(0x1f3, lbaadr & 0xff);
		wait_dsk();
		io_out8(0x1f4, (lbaadr >> 8) & 0xff);
		wait_dsk();
		io_out8(0x1f5, (lbaadr >> 16) & 0xff);
		wait_dsk();
		io_out8(0x1f6, 0xe0);
		wait_dsk();
		io_out8(0x1f7, 0x20);
		while (1==1) {//判断状态
			mask = io_in8(0x1f7);
			if ((mask & 0x08) != 0) {
				break; //可以读取数据，跳出循环
			}
			cmp = 0x01;
			for (z = 0; z <= 8; z++) {
				if (mask & cmp) state[z] = '1';
				else state[z] = '0';
				cmp = cmp << 1;
			}
			if ((mask & 0x01) != 0) {
				return mask; //发生错误,不是错误寄存器的状态
			}
//			cons_putstr0(cons, state);
//			cons_newline(cons);
		}
		for (j = 0; j < 256; j++) {
			data = io_in16(0x1f0);
			buf[i] = data & 0xff;
			buf[i + 1] = (data >> 8) & 0xff;
		}
		if (fat[i] >= 0xffe) {
			break; // end
		}
		else if (fat[i] == 0) {
			return 255; // 错误，size和扇区数不对应
		}
		else {
			i = fat[i];
		}
		size -= 512;
		buf += 512;
	}
	return 0;
}
//---------------------
/* 测试读写磁盘
给定buf，size，起始扇区
test 读取100个字节并打印
*/
//---------------------
void test_disk(struct CONSOLE *cons, int *fat) {
	unsigned int size = cons->dir_info->maxsize * 32;
//	size = 512;
	unsigned int clustno = cons->dir_info->clustno;
	unsigned char *buf = (unsigned char *)memman_alloc(memman, size);
	unsigned char res;
	char line[30];
//	clustno = 1;
	dsk_write0(cons, fat, clustno, (unsigned char *)cons->dir_info->adr_dir, size);
	res = dsk_read0(cons, fat, clustno, buf, size);
	if (res) {
		cons_putstr0(cons,"read errors!\n");
	}
	if (memcmp(buf, (unsigned char *)cons->dir_info->adr_dir, size) == 0) {
		cons_putstr0(cons, "good write and read\n");
		cons_newline(cons);
	}
	else {
		char test_buf[101];
		memcpy(test_buf, (char *)cons->dir_info->adr_dir, 100);
		cons_putstr0(cons, "bad write and read\n");
		buf[100] = 0;
		test_buf[100] = 0;
		cons_putstr0(cons, (char *)buf);
		cons_newline(cons);
		cons_putstr0(cons, test_buf);
		cons_newline(cons);
	}
	memman_free(memman, buf, size);
	return;
}


//---------test------------
char dsk_read0(struct CONSOLE *cons, int *fat, unsigned int clustno, unsigned char *buf, unsigned int size) {
	int i, j, z;
	unsigned char mask, cmp = 0x01;
	char state[10];
	short data;
	unsigned int lbaadr = (unsigned int)clustno;
	state[8] = 'r';
	state[9] = 0;
	//read disk
	for (i = clustno; i < MAX_FAT;) {
		if (size <= 0 || i > MAX_FAT) { break; }
		wait_dsk();
		io_out8(0x1f2, 0x01);			// 输入要读的扇区数，为1
		wait_dsk();
		io_out8(0x1f3, lbaadr & 0xff);	// 输入起始扇区的低八位
		wait_dsk();
		io_out8(0x1f4, (lbaadr >> 8) & 0xff);// 输入起始扇区的中八位
		wait_dsk();
		io_out8(0x1f5, (lbaadr >> 16) & 0xff);// 输入起始扇区的高八位
		wait_dsk();
		io_out8(0x1f6, 0xe0);			// 输入起始扇区的剩余的高位以及设定LBA模式
		wait_dsk();
		io_out8(0x1f7, 0x20);			// 发送读命令，允许错误重复写
		while (1) {//判断状态
			mask = io_in8(0x1f7);		// 读取八位状态寄存器的值
			cmp = 0x01;
			for (z = 0; z < 8; z++) {	// 用于友好化显示的转换
				if (mask & cmp) state[z] = '1';
				else state[z] = '0';
				cmp = cmp << 1;
			}
			cons_putstr0(cons, state);	// 显示到控制台
			cons_newline(cons);			// 换行
			if (mask & 0x08) {			// 数据已经从磁盘到磁盘缓冲区，则退出循环
				break;
			}
			if ((mask & 0x01) != 0x00) {
				return mask; //发生错误，返回当前状态寄存器的状态,不是错误寄存器的状态
			}
		}
		for (j = 0; j < 256; j++) {		// 一个扇区的数据256*2
			data = io_in16(0x1f0);		// 从16位的数据端口读取数据
			buf[i] = data & 0xff;		
			buf[i + 1] = (data >> 8) & 0xff;
		}
		if (fat[i] >= 0xffe) {
			break; // end
		}
		else if (fat[i] == 0) {
			return 255; // 错误，size和扇区数不对应
		}
		else {
			i = fat[i];
		}
		size -= 512;
		buf += 512;
	}
	return 0;
}
char dsk_write0(struct CONSOLE *cons, int *fat, unsigned int clustno, unsigned char *buf, unsigned int size) {
	int i, j,z;
	unsigned char mask,cmp;
	char state[10];
	short *adr = (short *)buf;
	unsigned int lbaadr = (unsigned int)clustno;
	state[8] = 'w';
	state[9] = 0;
	for (i = clustno; i < MAX_FAT;) {
		if (size <= 0 || i > MAX_FAT) { break; }
		//------------------test read first--------
		wait_dsk();
		io_out8(0x1f2, 0x01);					//操作的扇区个数 1
		wait_dsk();
		io_out8(0x1f3, lbaadr & 0xff);			//低位lba地址
		wait_dsk();
		io_out8(0x1f4, (lbaadr >> 8) & 0xff);	//中位lba地址
		wait_dsk();
		io_out8(0x1f5, (lbaadr >> 16) & 0xff);	//高位lba地址
		wait_dsk();
		io_out8(0x1f6, 0xe0);					//剩余高位lba地址以及lba模式
		wait_dsk();
		io_out8(0x1f7, 0x30);					//写命令
		while (1) {//判断状态
			mask = io_in8(0x1f7);
			cmp = 0x01;
			for (z = 0; z < 8; z++) {
				if (mask & cmp) state[z] = '1';
				else state[z] = '0';
				cmp = cmp << 1;
			}
			if (mask & 0x01) {
				return mask; //发生错误,不是错误寄存器的状态
			}
			cons_putstr0(cons, state);
			cons_newline(cons);
			if (mask & 0x08) {
				break;
			}
		}
		for (j = 0; j < 256; j++) {
			io_out16(0x1f0, adr[j]);// 往16位数据端口写数据
		}
		if (fat[i] >= 0xffe) {		// 文件的最后一个扇区 
			break; // end
		}
		else if (fat[i] == 0) {
			return 255; // 错误，size和扇区数不对应
		}
		else {
			i = fat[i];
		}
		size -= 512;	// 如果size小于0，会结束for循环，退出程序
		buf += 512;
	}
	return 0;
}
