# Educational Operating System

​	This is an educational operating system designed by Zhang et al in May, 2017, Beijing University of Posts and Telecommunications. 北京邮电大学操作系统课程设计

## Features

-  interrupt handling;
-  timing;
-  mouse and keyboard control;
-  three memory management methods: segmentation, Buddy algorithm and paging; 
-  two  multitaksing methods: priority quene or advanced RR algorithm; 
-  16bit color mode with 1024x768 resolution; 
-  windows management by sheet
-  basic file system.



## Requirments

​	qemu-system-i386 emulator(recommend), or a real computer.

## Compile

​	You can write/edit your own features in either C or Ensemble language, modify Makefile if necessary, and type ``make`` in !cons_nt.bat to compile use default compiler in z_tools.

​	Since default compiler is .exe file, Linux and macOS users may need other compilers. 

## Run with Qemu

​	For windows users, just type   ``make run`` in !cons_nt.bat, this will compile and boot OS with qemu emulator(using default compiler and qemu emulator from z_tools folder)

​	For macOS or Linux users, you have to compile it with other compilers, download and install qemu, cd into source dir, type  ``qemu-system-i386 -fda haribote.img`` in bash.

## Documentation

​	There is a 117 pages doc in Chinese, contact adamzjk@foxmail.com for details.



