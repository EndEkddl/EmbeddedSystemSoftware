#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "android/log.h"

#define LOG_TAG "MyTag"
#define LOGV(...)   __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define EXIT -1
#define PLAY 0
#define CORRECT 1
#define DEVICE_NAME "/dev/BullsAndCows"
#define MAJOR_NUM 242
typedef struct _arg{
		int state, cnt;
} _arg;

_arg arg;

JNIEXPORT jint JNICALL Java_org_example_ndk_StartActivity_openDevice(JNIEnv *env, jobject this){
	int fd = open(DEVICE_NAME, O_RDWR);
	if(fd==-1) {
		LOGV("OPEN device error!\n");
		return -1;
	}
	LOGV("OPEN SUCCESS\n");
	return fd;
	
}
JNIEXPORT void JNICALL Java_org_example_ndk_StartActivity_startGame
  (JNIEnv *env, jobject this, jint fd, jobject ret){


	jclass retClass = (*env)->GetObjectClass(env,ret);
	jfieldID jstateID = (*env)->GetFieldID(env,retClass,"state", "I");
	jfieldID jcntID = (*env)->GetFieldID(env,retClass,"cnt", "I");

	ioctl(fd, _IO(242, 0));
	read(fd, &arg, sizeof(arg));
	LOGV("startGame - read, arg.state : %d\n", arg.state);
	
	switch(arg.state){
		case CORRECT:
			(*env)->SetIntField(env, ret, jstateID, arg.state);
			(*env)->SetIntField(env, ret, jcntID, arg.cnt);
		break;
		case PLAY:
			LOGV("Playing now!\n");
			(*env)->SetIntField(env, ret, jstateID, arg.state);
		break;
		case EXIT:
			(*env)->SetIntField(env, ret, jstateID, arg.state);
		break;
		default: break;
	}

	return;
}

JNIEXPORT void JNICALL Java_org_example_ndk_StartActivity_closeDevice
  (JNIEnv *env, jobject this, jint fd){
	close(fd);
	return;
}
