#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <ctype.h>
//#include <sys/type.h>
#include <sys/stat.h>

#define TIMER_DEVICE "/dev/dev_driver"
#define MAJOR_NUM 242
#define SET_OPTION _IOR(MAJOR_NUM, 0, _arg *) // macros to encode a command(SET_OPTION)
#define COMMAND _IOR(MAJOR_NUM, 1, _arg *) // macros to encode a command(COMMAND)

//structure for ioctl arguments
typedef struct ioctl_arg {
	int interval, cnt, init;
}_arg;

_arg input;

int main(int argc, char* argv[]) {

	//exception for wrong input
	if (argc != 4) {
		printf("\tYour input format is invalid.\n");
		printf("\t<<correct format : /app TIMER_INTERVAL[1-100] TIMER_CNT[1-100] TIMER_INIT[0001-8000]\n>>");
		return -1;
	}

	int interval, cnt, init;
	interval = atoi(argv[1]);
	cnt = atoi(argv[2]);
	init = atoi(argv[3]);

	//set structure member to use it in ioctl function
	input.interval = interval;
	input.cnt = cnt;
	input.init = init;

	printf("interval : %d, cnt : %d, init : %d\n", input.interval, input.cnt, input.init);

	//exception for wrong input
	if (!interval || !cnt || !init) {
		printf("\tYour input format is invalid.\n");
		printf("\t<<correct format : /app TIMER_INTERVAL[1-100] TIMER_CNT[1-100] TIMER_INIT[0001-8000]\n>>");
		return -1;
	}

	int fd = open(TIMER_DEVICE, O_RDWR);
	//SET_OPTION for passing input 
	ioctl(fd, SET_OPTION, &input);
	//COMMAND for timer
	ioctl(fd, COMMAND, &input);

	close(fd);
	return 0;
}
