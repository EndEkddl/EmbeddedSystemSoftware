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
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>

int base = 10, button, prevKey=-1;
int baseidx = 0; 
int basearr[4] = { 10,8,4,2 };
unsigned char flag = 0;
MAINDATA maindata;
int checkFirst = 1;
int checkPeriod, checkPeriod2;
char led[4][8] = { {1,0,0,0,0,0,0,0},{0,1,0,0,0,0,0,0}, {0,0,1,0,0,0,0,0}, {0,0,0,1,0,0,0,0} };
unsigned char keypad[9][3] = {".QZ","ABC","DEF","GHI","JKL","MNO","PRS","TUV","WXY"};

void sig_handler(int sigNum) {
	flag = 1; return;
}

void initialize() {

	int i=0, j=0;
	for (i = 0; i < 8; i++) maindata.led[i] = 0;
	for (i = 0; i < 4; i++) maindata.fnd[i] = 0;
	for (i = 0; i < 50; i++) maindata.textLCD[0][i] = maindata.textLCD[1][i] = 0;
	for (i = 0; i < 10; i++) {
		for (; j < 7; j++) maindata.dot[i][j] = 0;
	}
	printDOT(maindata.dot);
	printLED(maindata.led);
	printFND(maindata.fnd);
	printTEXT(maindata.textLCD[0], 16);

	base = 10;
	button = maindata.visibleCursor = 0;
	maindata.textLen = 0;
	prevKey = -1;
	maindata.textMode = 0; //alphabet or number mode
	maindata.cursor.y = maindata.cursor.x = 0;
	checkFirst = 1; //Is it first exectuon?

	printDOT(maindata.dot);
	printLED(maindata.led);
	printFND(maindata.fnd);
	printTEXT(maindata.textLCD[0], 16);

	return;
}

void setFNDTime() {
	time_t t;
	char buf[50] = { 0, };
	time(&t);
	sprintf(buf, "%s", ctime(&t));

	int i = 0;
	while (1) {
		if (buf[i] == ':')break;
		i++;
	}

	int j=0;
	for (; j < 2; j++) maindata.fnd[j] = buf[i + j - 2] - '0'; // set Hour
	for (j = 2; j < 4; j++) maindata.fnd[j] = buf[i + j - 1] - '0'; // set Minute
	return;
}

int main() {
	struct input_event Event[64];
	maindata.textLen = 0;
	prevKey = -1;
	maindata.visibleCursor = 0;
	maindata.textMode = 0;
	base = 10;
	
	int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	if (fd == -1) printf("main open error!\n");
	maindata.ledMode = 0;

	int dev = open("/dev/fpga_push_switch", O_RDWR);
	if (dev < 0) {
		printf("open SWITCH device error!(main)\n");
		close(dev);
		return -1;
	}
	(void)signal(SIGINT, sig_handler);
	
	int semid = semget((key_t)12345, 1, 0666 | IPC_CREAT);
	int semid2 = semget((key_t)12346, 1, 0666 | IPC_CREAT);
	
	key_t key = ftok("./", INPUT_PROCESS); 
	struct input_event* shmaddrIE;
	unsigned char* shmaddrSW;
	int shmid = shmget(key, sizeof(INDATA), IPC_CREAT | 0644);
	struct INDATA* shmaddr = (struct INDATA*)shmat(shmid, NULL, 0);
	memset(shmaddr, 0, sizeof(OUTDATA));
	
	key_t key2 = ftok("./", OUTPUT_PROCESS);
	int shmid2 = shmget(key2, sizeof(OUTDATA), IPC_CREAT | 0644);
	struct OUTDATA* shmaddr2 = (struct OUTDATA*)shmat(shmid2, NULL, 0);
	memset(shmaddr2, 0, sizeof(OUTDATA));

	unsigned char switchBuffer[9];
	unsigned char prevBuffer[9];

	while (!flag) {
		int rd = read(fd, Event, 64 * sizeof(struct input_event));
		while (Event[0].type==1 && Event[0].value==PRESS && Event[0].code==115) {// volume up(mode +)
			rd = read(fd, Event, 64 * sizeof(struct input_event));
			if (Event[0].type == 1 && Event[0].value == NO_PRESS && Event[0].code == 115) {
				if (maindata.mode >= 3) maindata.mode = 0;
				else maindata.mode++;
				initialize();
				printf("mode is changed to %d\n", maindata.mode);
			}
		}

		while (Event[0].type == 1 && Event[0].value == PRESS && Event[0].code == 114) {// volume down(mode -)
			rd = read(fd, Event, 64 * sizeof(struct input_event));
			if (Event[0].type == 1 && Event[0].value == NO_PRESS && Event[0].code == 114) {
				if (maindata.mode) maindata.mode--;
				else maindata.mode = 3;
				initialize();
				printf("mode is changed to %d\n", maindata.mode);
			}
		}

		if (Event[0].type = 1 && Event[0].value == 1 && Event[0].code == 158) { // back(exit)
			exit(0);
		}

		memcpy(prevBuffer, switchBuffer, sizeof(switchBuffer));
		read(dev, &switchBuffer, sizeof(switchBuffer));
		usleep(100000);
		
		switch (maindata.mode) {
		case 0: {// clock mode
			if (checkFirst) {
				checkFirst = 0;
				setFNDTime();
			}
			if (switchBuffer[0] == 1 && prevBuffer[0] != 1) { // change board time
				maindata.textMode = ~maindata.textMode; // textMode : changability of time(in clock mode)
				switchBuffer[0] = 0;
			}
			else if (switchBuffer[1] == 1) {
				setFNDTime(); // reset FND to board time
				switchBuffer[1] = 0;
			}
			else if (switchBuffer[2] == 1 && maindata.textMode) { // increase one hour
				maindata.fnd[1]++;
				if (maindata.fnd[1] == 10) {
					maindata.fnd[0]++;
					maindata.fnd[1] = 0;
				}
				if (maindata.fnd[0] == 2 && maindata.fnd[1] == 4) {
					maindata.fnd[0] = maindata.fnd[1] = 0;
				}
				switchBuffer[2] = 0;
			}
			else if (switchBuffer[3] == 1 && maindata.textMode) {// increase one minute
				maindata.fnd[3]++;
				if (maindata.fnd[3] == 10) {
					maindata.fnd[2]++;
					maindata.fnd[3] = 0;
				}
				if (maindata.fnd[2] == 6) {
					maindata.fnd[2] = 0;
					maindata.fnd[1]++;
					if (maindata.fnd[1] == 10) {
						maindata.fnd[0]++;
						maindata.fnd[1] = 0;
					}
					if (maindata.fnd[0] == 2 && maindata.fnd[1] == 4) {
						maindata.fnd[0] = maindata.fnd[1] = 0;
					}
				}
				switchBuffer[3] = 0;
			}
			printFND(maindata.fnd);
			break;
		}
		case 1: {// Counter mode
			int square, cubic, fourSq;
			square = base*base; //^2
			cubic = square*base; //^3
			fourSq = square*square; //^4

			if (switchBuffer[0] == 1) {// change the base (10->8->4->2->10->..)
				baseidx = (baseidx + 1) % 4;
				base = basearr[baseidx];
				printf("base is %d\n", base);
			}
			else if (switchBuffer[1] == 1) button += square; //increase 100's number
			else if (switchBuffer[2] == 1) button += base; // increase 10's number 
			else if (switchBuffer[3] == 1) button++; // increase 1's number

			//change the format according to the current base
			int tmp = button;
			if (button >= fourSq) tmp = tmp % fourSq;
			maindata.fnd[0] = tmp / cubic;
			tmp %= cubic;
			maindata.fnd[1] = tmp / square;
			tmp %= square;
			maindata.fnd[2] = tmp / base;
			tmp %= base;
			maindata.fnd[3] = tmp;

			printFND(maindata.fnd);
			break;
		}
		case 2: { //Text editor mode
			if (switchBuffer[1] == 1 && switchBuffer[2] == 1) {// reset LCD
				int i;
				for (i = 0; i <= maindata.textLen; i++) maindata.textLCD[0][i] = 0;
				maindata.textLen = 0;
				button += 2;
			}
			else if (switchBuffer[4] == 1 && switchBuffer[5] == 1) { // Change the mode
				if (maindata.textMode) maindata.textMode = 0;
				else maindata.textMode = 1;
				button += 2;
			}
			else if (switchBuffer[7] == 1 && switchBuffer[8] == 1) {// put space
				maindata.textLCD[0][++maindata.textLen] = ' ';
				button += 2;
				prevKey=-1;
			}
			else{
				int i,j;
				for(i=0; i<9; i++){
					if(switchBuffer[i]){
						if(maindata.textMode==0){//alphabet mode
							if(prevKey==i){//current switch and prev switch has same value.
								for(j=0; j<3;j++){if(keypad[i][j]==maindata.textLCD[0][maindata.textLen]) break;}
								j = (j+1)%3;
								maindata.textLCD[0][maindata.textLen]=keypad[i][j];							
							}
							else{
								maindata.textLCD[0][++maindata.textLen] = keypad[i][0];
								prevKey=i;
							}
						}
						else{//number mode
							maindata.textLCD[0][++maindata.textLen] = i+'1';

						}
						button++;
					}
				}
			}
			
			int tmp = button;
			tmp %= 10000;
			maindata.fnd[0] = tmp / 1000;
			tmp %= 1000;
			maindata.fnd[1] = tmp / 100;
			tmp %= 100;
			maindata.fnd[2] = tmp / 10;
			tmp %= 10;
			maindata.fnd[3] = tmp;

			printTEXT(maindata.textLCD[0], maindata.textLen);
			printFND(maindata.fnd);
			printTYPE(maindata.textMode);
			break;
		}
		case 3: {
			button++;
			int y = maindata.cursor.y; 
			int x = maindata.cursor.x;

			if (switchBuffer[0] == 1) {// reset matrix and cursor 
				int i,j;
				for (i = 0; i < 10; i++) {
					for (j = 0; j < 7; j++) maindata.dot[i][j] = 0;
				}
				for (i = 0; i < 4; i++) maindata.fnd[i] = 0;
				y = x = 0;
			}
			else if (switchBuffer[1] == 1) {//up
				if (y) y--;
				else y = 9;
			}
			else if (switchBuffer[3] == 1) {//left
				if (x) x--;
				else x = 6;
			}
			else if (switchBuffer[5] == 1) {//right
				if (x != 6) x++;
				else x = 0;
			}
			else if (switchBuffer[7] == 1) {//down
				if (y != 9) y++;
				else y = 0;
			}
			else if (switchBuffer[2] == 1) {//cursor ON/OFF
				maindata.visibleCursor = (maindata.visibleCursor) ? 0 : 1;
			}			
			else if (switchBuffer[4] == 1) {// select the current POINT 
				maindata.dot[y][x] = (maindata.dot[y][x]) ? 0 : 1;
			}
			else if (switchBuffer[6] == 1) {// reset only matrix
				int i,j;
				for (i = 0; i < 10; i++) {
					for (j = 0; j < 7; j++) maindata.dot[i][j] = 0;
				}
			}
			else if (switchBuffer[8] == 1) {// reverse the matrix
				int i,j;
				for (i = 0; i < 10; i++) {
					for (j = 0; j < 7; j++) {
						maindata.dot[i][j] = (maindata.dot[i][j]) ? 0 : 1;
					}
				}
			}
			else button--;

			if (maindata.visibleCursor) {//print the matrix with cursor
				char tmpArr[10][7];
				int i,j;
				for (i = 0; i < 10; i++) {
					for (j = 0; j < 7; j++) tmpArr[i][j] = maindata.dot[i][j];
				}
				if (checkPeriod < 10)tmpArr[y][x] = 1;
				else {
					tmpArr[y][x] = 0;
					if(checkPeriod==20) checkPeriod = 0;
				}
				checkPeriod++;
			}
			else printDOT(maindata.dot); // print only the matrix

			maindata.cursor.y = y;
			maindata.cursor.x = x;
			int tmp = button;
			tmp %= 10000;
			maindata.fnd[0] = tmp / 1000;
			tmp %= 1000;
			maindata.fnd[1] = tmp / 100;
			tmp %= 100;
			maindata.fnd[2] = tmp / 10;
			tmp %= 10;
			maindata.fnd[3] = tmp;
			printFND(maindata.fnd);
			break;
		}			
		}
		
		if (maindata.mode == 0) {
			if (maindata.textMode) {
				if (maindata.ledMode) {
					printLED(led[2]);
					if (checkPeriod2 == 10) checkPeriod2 = maindata.ledMode = 0;
				}
				else {
					printLED(led[3]);
					if (checkPeriod2 == 10) {
						checkPeriod2 = 0;
						maindata.ledMode = 1;
					}
				}
				checkPeriod2++;
			}
			else printLED(led[0]);
		}
		else if (maindata.mode == 1) {
			switch (base) {// turn on the led according to the current base
			case 10: { printLED(led[0]); break;}
			case 8: { printLED(led[1]); break; }
			case 4: { printLED(led[2]); break; }
			case 2: { printLED(led[3]); break; }
			}
		}
	
	}
	return 0;
}

