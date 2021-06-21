#include "myHeader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>

int outputP(int semid) {//sema down
	struct sembuf buf;
	buf.sem_num = 0;
	buf.sem_op = -1;
	buf.sem_flg = SEM_UNDO;

	if (semop(semid, &buf, 1) == -1) {
		perror("outputP failed");
		exit(1);
	}
	return (0);
}

int outputV(int semid) {//sema up
	struct sembuf buf;
	buf.sem_num = 0;
	buf.sem_op = 1;
	buf.sem_flg = SEM_UNDO;

	if (semop(semid, &buf, 1) == -1) {
		perror("outputV failed");
		exit(1);
	}
	return (0);
}
void printLED(char arr[8]) {

	int dev = open("/dev/fpga_led", O_RDWR);
	if (dev < 0) {
		printf("open LED device error!\n");
		exit(1);
	}

	int res = 0;
	int i, j;
	for(i=7, j=0; i>=0; i--){
		res += (1<<(j++)) * arr[i];
	}
	write(dev, &res, 1);

	close(dev);
	return;
}

void printDOT(char arr[10][7]) {
	int dev = open("/dev/fpga_dot", O_WRONLY);
	if (dev < 0) {
		printf("open DOT device error!\n");
		exit(1);
	}

	unsigned char res[10];
	int i,j,k;
	for (i = 0; i < 10; i++) {
		unsigned char tmp = 0;
		for (j = 0; j < 7; j++) {
			if (arr[i][j] == 1) tmp += (1 << (6 - j));
		}

		res[i] = tmp;
	}
	write(dev, res, 10);

	close(dev);
	return;
}

void printTEXT(char arr[50], int len) {
	int dev = open("/dev/fpga_text_lcd", O_WRONLY);
	if (dev < 0) {
		printf("open TEXT device error!\n");
		exit(1);
	}

	// maximum length is set to 16
	if (len > 16) write(dev, &arr[len - 16], 16);
	else write(dev, arr, 16);

	close(dev);
	return;
}

void printFND(char arr[4]) {
	int dev = open("/dev/fpga_fnd", O_RDWR);
	if (dev < 0) {
		printf("open FND device error!\n");
		exit(1);
	}

	unsigned char res[4];
	int i;
	for (i = 0; i < 4; i++) {
		if ((arr[i] < '0') || (arr[i] > '9')) res[i] = arr[i];
		else res[i] = arr[i] - '0';
	}

	write(dev, res, 4);
	close(dev);
	return;
}

void printTYPE(int type) {
	int dev = open("/dev/fpga_dot", O_WRONLY);
	if (dev < 0) {
		printf("open DOT device error!\n");
		exit(1);
	}

	unsigned char FPGA[2][10] = {
		{0x1c, 0x36, 0x63, 0x63, 0x63, 0x7f, 0x7f, 0x63, 0x63, 0x63}, // A
		{0x0c, 0x1c, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e}  // 1
	};
	int select = (!type) ? 0 : 1;

	write(dev, FPGA[select], 10);
	close(dev);
	return;
}
void outputProc() {
	key_t key = ftok("./", OUTPUT_PROCESS);
	int shmid = shmget(key, sizeof(OUTDATA), IPC_CREAT | 0644);
	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	OUTDATA* shmaddr = (OUTDATA*)shmat(shmid, NULL, 0);
	memset(shmaddr, 0, sizeof(OUTDATA));

	char led[3][8] = { {1,0,0,0,0,0,0,0}, {0,0,1,0,0,0,0,0}, {0,0,0,1,0,0,0,0} }; //led[1], led[3], led[4]
	int cnt = 0;
	while (1) {
		int cnt2 = 0;
		switch (shmaddr->mode) {
		case 0: {
			printFND(shmaddr->led);
			if (!shmaddr->textMode) printLED(led[0]);
			else if (shmaddr->ledMode == 1) {
				printLED(led[1]);
				if (cnt2 == 5) {
					cnt2 = 0;
					shmaddr->ledMode = 0;
				}
				cnt2++;
			}
			else {
				printLED(led[2]);
				if (cnt2 == 5) {
					cnt2 = 0;
					shmaddr->ledMode = 1;
				}
				cnt2++;
			}
			break;
		}
		case 1: {
			printFND(shmaddr->fnd);
			break;
		}
		case 2: {
			printTEXT(shmaddr->textLCD[0], shmaddr->textLen);
			printTYPE(shmaddr->textMode);
			break;
		}
		case 3: {
			char tmp[10][7];
			if (shmaddr->visibleCursor == 0) printDOT(shmaddr->dot);
			else {
				int y,x;
				for (y = 0; y < 10; y++) {
					for (x = 0; x < 7; x++) tmp[y][x] = shmaddr->dot[y][x];
				}
				//print cursor periodically
				if (cnt <= 5) tmp[shmaddr->cursor.y][shmaddr->cursor.x] = 1;
				else {
					tmp[shmaddr->cursor.y][shmaddr->cursor.x] = 0;
					if (cnt == 10) cnt = 0;
				}

				printDOT(tmp);
				cnt++;
			}
			printFND(shmaddr->fnd);
			break;
		}
		}
	}
	return;
}

