#include "myHeader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
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

int inputP(int semid) { //sema down
	struct sembuf buf;
	buf.sem_num = 0;
	buf.sem_op = -1;
	buf.sem_flg = SEM_UNDO;

	if (semop(semid, &buf, 1) == -1) {
		perror("inputP failed");
		exit(1);
	}
	return (0);
}

int inputV(int semid) { //sema up
	struct sembuf buf;
	buf.sem_num = 0;
	buf.sem_op = 1;
	buf.sem_flg = SEM_UNDO;

	if (semop(semid, &buf, 1) == -1) {
		perror("inputV failed");
		exit(1);
	}
	return (0);
}

void inputProc() {

	struct input_event Event[64];
	INDATA inData;
	unsigned char switchBuffer[9];
	int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	if (fd == -1) printf("input open error!\n");

	memset(&inData, 0, sizeof(INDATA));
	int dev = open("/dev/fpga_push_switch", O_RDWR | O_NONBLOCK);
	if (dev < 0) {
		printf("open SWITCH device error!(input)\n");
		close(dev);
		return ;
	}

	key_t key = ftok("./", INPUT_PROCESS);
	int shmid = shmget(key, sizeof(INDATA), IPC_CREAT | 0644);
	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	INDATA* shmaddr = (INDATA*)shmat(shmid, NULL, 0);
	memset(shmaddr, 0, sizeof(INDATA));

	int rd, rd2;
	while (1) {
		rd = read(fd, Event, sizeof(struct input_event) * 64);
		rd2 = read(dev, switchBuffer, sizeof(switchBuffer));

		//volume down
		while (Event[0].type == 1 && Event[0].value == PRESS && Event[0].code == 114) {
			rd = read(fd, Event, sizeof(struct input_event) * 64);
			shmaddr->value = Event[0].value;
			shmaddr->code = Event[0].code;
			shmaddr->type = Event[0].type;
		}
		//volume up
		while (Event[0].type == 1 && Event[0].value == PRESS && Event[0].code == 115) {
			rd = read(fd, Event, sizeof(struct input_event) * 64);
			shmaddr->value = Event[0].value;
			shmaddr->code = Event[0].code;
			shmaddr->type = Event[0].type;
		}

		strcpy(shmaddr->switchBuffer, switchBuffer);
		Event[0].value = Event[0].type = Event[0].code = 0;

		usleep(300000);
	}

	close(dev);
	return;
}




