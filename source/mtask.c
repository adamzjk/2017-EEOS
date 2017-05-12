/* 多任务管理 */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>

struct TASKCTL *taskctl;
struct TIMER *task_timer;
extern int switch_mode;
int avg_wait_time;
int timeslice;

int task_timeslice(int priority);
void effective_nice(struct TASKLEVEL *tl, int timeslice);

struct TASK *task_now(void)
{
	struct TASK *ans =  (taskctl->level[taskctl->now_lv]).now;

//	printInfo("now ok");
	return ans;
	//struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
	//return tl->tasks[tl->now];
}


void task_add(struct TASK *task)
{
	//printInfo("add..");

	struct TASKLEVEL *tl = &taskctl->level[task->level];
	if (tl->tasks == 0)
		tl->now = task; // !!!!
	task->next = tl->tasks;
	tl->tasks = task;
	//tl->tasks[tl->running] = task;
	tl->running++;
	task->flags = TASK_RUNNING; /*活动中*/

	//printInfo("add ok");
	return;
}


void task_remove(struct TASK *task)
{
	//printInfo("rm..");

	struct TASKLEVEL *tl = &taskctl->level[task->level];
	struct TASK *i;

	if(tl->tasks == task){
		tl->tasks = task->next;
	}
	else{
		for(i=tl->tasks; i != 0; i = i->next){
			if(i->next == task){
				break;
			}
		}
		if(i == 0){
			printInfo("!!!rm failed");
		}
		i->next = task->next;
	}

	if(tl->now == task){			// 去除正在运行的task
		tl->now = tl->tasks;
	}

	tl->running-- ;
	if(tl->running == 0){
		tl->now = 0;
		tl->tasks = 0;
	}
	/*int j;
	for(j=0;j<MAX_TASKS;j++)
		if((taskctl->level[i->level]).tasks[j] == i)
			break;
*/
	if (tl->running_now < tl->running) {
		tl->running_now++; /* 偢傟傞偺偱丄偙傟傕偁傢偣偰偍偔 */
	}
	if (tl->running_now >= tl->running) {
		/* now偑偍偐偟側抣偵側偭偰偄偨傜丄廋惓偡傞 */
		tl->running_now = 0;
	}
	task->flags = TASK_IDLE;
	task->next = 0;

	//printInfo("rm ok");
	return;
}

//	/*寻找task所在的位置*/
//	for (i = 0; i < tl->running; i++) {
//		if (tl->tasks[i] == task) {
//			/*在这里 */
//			break;
//		}
//	}
//
//	tl->running--;
//	if (i < tl->now) {
//		tl->now--; /*需要移动成员，要相应地处理 */
//	}
//	if (tl->now >= tl->running) {
//		/*如果now的值出现异常，则进行修正*/
//		tl->now = 0;
//	}
//	task->flags = 1; /* 休眠中 */
//
//	/* 移动 */
//	for (; i < tl->running; i++) {
//		tl->tasks[i] = tl->tasks[i + 1];
//	}


void task_switchsub(void)
{
	//printInfo("swi_sub..");
	int i;
	/*寻找最上层的LEVEL */
	for (i = 0; i < MAX_TASKLEVELS; i++) {
		if (taskctl->level[i].running > 0) {
			break; /*找到了*/
		}
	}
	taskctl->now_lv = i;
	taskctl->lv_change = 0;
	avg_wait_time = 0;
	//printInfo("swi_sub ok");
	return;
}


void task_idle(void)
{
	for (;;) {
		io_hlt();
	}
}


struct TASK *task_init(struct MEMMAN *memman)
{
	//printInfo("init..");

	int i;
	struct TASK *task, *idle;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;

	taskctl = (struct TASKCTL *) memman_alloc_4k(memman, sizeof (struct TASKCTL));
	for (i = 0; i < MAX_TASKS; i++) {
		taskctl->tasks0[i].flags = TASK_UNUSE;
		taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
		taskctl->tasks0[i].next = 0;	// 这里！
		taskctl->tasks0[i].langmode = 0; // 这里！
		set_segmdesc(gdt + TASK_GDT0 + i, 103, (int) &taskctl->tasks0[i].tss, AR_TSS32);
	}
	for (i = 0; i < MAX_TASKLEVELS; i++) {
		taskctl->level[i].running = 0;
		taskctl->level[i].now = 0;
		taskctl->level[i].tasks = 0;
		taskctl->level[i].running_now = 0;
	}

	task = task_alloc();
	task->flags = TASK_RUNNING; /*活动中标志*/
	task->priority = 2; /* 0.02秒*/
	task->level = 0; /*最高LEVEL */
	task->nice = 120;
	task->wait_time = 0;
	//task->nice = 120;
	//task->wait_time = 0;
	task_add(task);
	task_switchsub(); /* LEVEL 设置*/
	load_tr(task->sel);
	task_timer = timer_alloc();
	int timeslice = task->priority;
	if (switch_mode == 2) {
		timeslice = task_timeslice(task->nice);
	}
	timer_settime(task_timer, timeslice);

	idle = task_alloc();
	strcpy(idle->name,"sentry");
	idle->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024;
	idle->tss.eip = (int) &task_idle;
	idle->tss.es = 1 * 8;
	idle->tss.cs = 2 * 8;
	idle->tss.ss = 1 * 8;
	idle->tss.ds = 1 * 8;
	idle->tss.fs = 1 * 8;
	idle->tss.gs = 1 * 8;
	task_run(idle, MAX_TASKLEVELS - 1, 1);

	//printInfo("init ok");

	return task;
}


struct TASK *task_alloc(void)
{
	//printInfo("alloc...");
	int i;
	struct TASK *task;
	for (i = 0; i < MAX_TASKS; i++) {
		if (taskctl->tasks0[i].flags == TASK_UNUSE) {
			task = &taskctl->tasks0[i];
			task->flags = TASK_IDLE;   /*正在使用的标志*/
			task->tss.eflags = 0x00000202; /* IF = 1; */
			task->tss.eax = 0; /*这里先置为0*/
			task->tss.ecx = 0;
			task->tss.edx = 0;
			task->tss.ebx = 0;
			task->tss.ebp = 0;
			task->tss.esi = 0;
			task->tss.edi = 0;
			task->tss.es = 0;
			task->tss.ds = 0;
			task->tss.fs = 0;
			task->tss.gs = 0;
			task->tss.ldtr = 0;
			task->tss.iomap = 0x40000000;
			task->tss.ss0 = 0;
			//printInfo("alloc ok");
			return task;
		}
	}
	//printInfo("!alloc fail!");
	return 0; /*全部正在使用*/
}

void task_run_in2(struct TASK *task,int Nice)
{
	//printInfo("run2...");
	task->level = 1;
	task->priority = 2;
	task->nice = Nice;
	task->wait_time = 0;

	if (task->flags != TASK_RUNNING) {
		/*从休眠状态唤醒的情形*/
		task->nice = Nice;
		task_add(task);
	}
	taskctl->lv_change = 1; /*下次任务切换时检查LEVEL */
	return;
}
void task_run(struct TASK *task, int level, int priority)
{
	//printInfo("run...");
	if (level < 0) {
		level = task->level; /*不改变LEVEL */
	}
	if (priority > 0) {
		task->priority = priority;
	}
	
	task->nice = 120;
	task->wait_time = 0;

	if (task->flags == TASK_RUNNING && task->level != level) { 
		/*改变活动中的LEVEL */
		task_remove(task); /*这里执行之后flag的值会变为1，于是下面的if语句块也会被执行*/
	}
	if (task->flags != TASK_RUNNING) {
		/*从休眠状态唤醒的情形*/
		task->level = level;
		task_add(task);
	}
	taskctl->lv_change = 1; /*下次任务切换时检查LEVEL */
	//printInfo("run ok");
	return;
}

int task_timeslice(int priority)
{
	//printInfo("timeslice..");
	if (priority < NICE_TO_PRIO(0))
		return (140 - priority) / 10;
	//return SCALE_PRIO(DEF_TIMESLICE * 4, priority);
	else
		return 4 * (140 - priority) / 10;
	//return SCALE_PRIO(DEF_TIMESLICE, priority);
}

void task_switch(void)
{
	//printInfo("swi...");

	struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
	struct TASK *new_task, *now_task = tl->now;
	//找下一个任务并更新running_now的值
	if(tl->now->next == 0){
		tl->now = tl->tasks;
		tl->running_now = 0;
	} else {
		tl->now = tl->now->next;
		tl->running_now++;
	}
	//如果level发生了变化
	if (taskctl->lv_change != 0) {
		task_switchsub();
		tl = &taskctl->level[taskctl->now_lv];
	}
	//找到下一个要执行的任务
	new_task = tl->now;
	//将其等待时间变为0
	tl->now->wait_time = 0;
	timeslice = new_task->priority;
	//如果在模式2中，根据nice值计算时间片大小
	if (switch_mode == 2) {
		timeslice = task_timeslice(new_task->nice);
	}
	//重置计时器
	timer_settime(task_timer, timeslice);
	//如果在模式2中，动态改变nice值
	if (switch_mode == 2) {
		effective_nice(tl, timeslice);
	}

	if (new_task != now_task) {
		//printInfo("");
		farjmp(0, new_task->sel);
	}

	//printInfo("swi ok");
	return;
}


void task_sleep(struct TASK *task)
{
	//printInfo("slp...");
	struct TASK *now_task;
	if (task->flags == TASK_RUNNING) {
		/*如果处于活动状态*/
		now_task = task_now();
		task_remove(task); /*执行此语句的话flags将变为1 */
		if (task == now_task) {
			/*如果是让自己休眠，则需要进行任务切换*/
			task_switchsub();
			now_task = task_now(); /*在设定后获取当前任务的值*/
			farjmp(0, now_task->sel);
		}
	}
	//printInfo("slpok");
	return;
}


void effective_nice(struct TASKLEVEL *tl, int timeslice)
{
	struct TASK *Task;
	int count = 0;
	//禁止中断
	io_cli();
	avg_wait_time = 0;
	//切换到第一个任务
	Task = tl->tasks;
	//计算平均等待时间并更新等待时间
	while (count <= tl->running) {
		//将所有等待中的任务的等待时间更新
		if (count != tl->running_now) {
			if (Task->wait_time > 20) {
				Task->wait_time =0;
			}
			Task->wait_time += timeslice;
		}
		avg_wait_time += Task->wait_time;
		count += 1;
		//计算下一个任务
		if (Task->next == 0) {
			Task = tl->tasks;
		}
		else {
			Task = Task->next;
		}
	}
	//计算平均等待时间
	avg_wait_time /= (tl->running + 1);
	count = 0;
	int dec = 0;
	int inc = 0;
	//切换到第一个任务
	Task = tl->tasks;
	while (count <= tl->running) {
		//如果等待时间比较短，降低优先级，更新nice值
		if (Task->wait_time < avg_wait_time) {
			inc = (avg_wait_time - Task->wait_time) / 4;
			Task->nice += inc;
			//限制最大nice值为139
			if (Task->nice > 139) {
				Task->nice = 139;
			}
		}//如果等待时间比较长，升高优先级，更新nice值
		else if (Task->wait_time > avg_wait_time) {
			dec = (Task->wait_time - avg_wait_time) / 4;
			Task->nice -= dec;
			//限制最小nice值
			if (Task->nice < 100) {
				Task->nice = 100;
			}
		}
		count += 1;
		//更新下一个任务
		if (Task->next == 0) {
			Task = tl->tasks;
		}
		else {
			Task = Task->next;
		}
	}
	io_sti();
}






