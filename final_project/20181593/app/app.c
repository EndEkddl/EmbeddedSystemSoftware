#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <ctype.h>
#include <syscall.h>

#define DEVICE_NAME "/dev/BullsAndCows"
#define MAJOR_NUM 242

typedef struct ioctl_arg {
	int state, cnt;
}_arg;

_arg arg;

int main(void) {
	int fd, i;
	fd = open(DEVICE_NAME, O_RDWR);
	printf("================using syscall====================\n");
	i = syscall(379, fd);

	printf("================using printf====================\n");
	if (fd < 0) {
		printf("Open device error!\n");
		return -1;
	}
	do {
		ioctl(fd, _IO(242, 0));
		read(fd, &arg, sizeof(arg));
		printf("cnt:%d, state:%d\n",  arg.cnt, arg.state);
	} while (arg.state != -1);

	close(fd);

	return 0;
}
