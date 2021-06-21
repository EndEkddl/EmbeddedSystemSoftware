CC=arm-none-linux-gnueabi-gcc
TARGET=20181593
OBJECTS = main.o input.o output.o
OBJS = $(addprefix ,$(OBJECTS))

$(TARGET): $(OBJS)
	$(CC) -static -o $(TARGET) $(OBJS)

%.o: %.c 
	$(CC) -static -c -o $@ $<

clean:
	rm -rf $(OBJECTS) $(TARGET)

push:
	adb push $(TARGET) /data/local/tmp
