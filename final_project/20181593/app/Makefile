default:
	arm-none-linux-gnueabi-gcc -static -Wall -Wextra -o app app.c

clean:
	rm -rf app

push:
	adb push app /data/local/tmp
