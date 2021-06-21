#ifndef MY_HEADER_H
#define MY_HEADER_H
#define INPUT_PROCESS 1
#define MAIN_PROCESS 2
#define OUTPUT_PROCESS 3
#define PRESS 1
#define NO_PRESS 0

typedef struct {
	int type;
	int value;
	int code;
	unsigned char switchBuffer[9];
} INDATA;

int inputP(int semid);
int inputV(int semid);
void inputProc();

typedef struct {
	int x, y;
} POINT;

typedef struct {
	int mode;
	int textMode;
	int ledMode;
	int textLen;
	int visibleCursor;
	char led[8];
	char fnd[4];
	char textLCD[2][50];
	char dot[10][7];
	POINT cursor;
} OUTDATA;

int outputP(int semid);
int outputV(int semid);

void printFND(char arr[4]);
void printLED(char arr[8]);
void printTEXT(char arr[50], int len);
void printDOT(char arr[10][7]);
void printTYPE(int type);
void outputProc();

typedef struct {
	int mode;
	int textMode;
	int ledMode;
	int textLen;
	int visibleCursor;
	char led[8];
	char fnd[4];
	char textLCD[2][50];
	char dot[10][7];
	POINT cursor;
} MAINDATA;
void sig_handler(int sigNum);
void initialize();
void setFNDTime();
#endif
