#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syscall.h>

int main() {
	int fd;
	char buf;
	fd = open("/dev/stopwatch", O_WRONLY);
	if (fd < 0) {
		printf("Open(/dev/stopwatch) failed!\n");
		return -1;
	}
	write(fd, &buf, 1);
	close(fd);

	return 0;
}