#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <getopt.h>
#include "ps3dev.h"
#include <mw/msp.h>
#include <mw/shm.h>
#include <mw/msg.h>

int verbose; 

struct s_rec js;

int ret;
int err = 0;
int stop = 0;

int trim[3] = {0,0,0};//in degrees * 1000
int mode = 0;


void do_adjustments(struct s_rec *js) {
	if (js->aux<0) return;

	switch (js->aux) {
		default:
			printf("Unknown command %i\n",js->aux);
	}

	js->aux=-1; //reset receiver command

} 

void catch_signal(int sig)
{
	printf("signal: %i\n",sig);
	stop = 1;
}

void mssleep(unsigned int ms) {
  struct timespec tim;
   tim.tv_sec = ms/1000;
   tim.tv_nsec = 1000000L * (ms % 1000);
   if(nanosleep(&tim , &tim) < 0 )
   {
      printf("Nano sleep system call failed \n");
   }
}

void loop() {                                          
	struct S_MSP_RC rc;
	struct S_MSG msg;

	if (verbose) printf("Starting main loop...\n");
	int yprt[4] = {0,0,0,0};
	while (!stop) {
		mssleep(20);
		ret = rec_update(&js); 
		// 0 - no update but read ok
		// 1 - update
		if (ret < 0) {
			stop = 1;
			printf("Receiver reading error: [%s]\n",strerror(ret));
			break;
		}
		
		do_adjustments(&js);
		
		memcpy(yprt,js.yprt,sizeof(int)*4);

		if (yprt[3]>2000) yprt[3]=2000;
		if (yprt[3]<950) yprt[3]=950;
		rc.roll = yprt[2];
		rc.pitch = yprt[1];
		rc.yaw = yprt[0];
		rc.throttle = yprt[3];
		rc.aux1=rc.aux2=rc.aux3=rc.aux4=1500;

		//if (verbose) printf("%i %i %i %i\n",yprt[0],yprt[1],yprt[2],yprt[3]);
		msp_SET_RAW_RC(&msg,&rc);
		shm_put_outgoing(&msg);
	}
}


int main(int argc, char **argv) {

	signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);
	verbose = 1;

	if (verbose) printf("Opening shm\n");
	dbg_init(251);
	if (shm_client_init()) return -1;


/*
	ret=ps3config_open(&ps3config,CFG_PATH);
	if (ret<0) {
		printf("Failed to initiate config! [%s]\n", strerror(ret));	
		return -1;
	}*/


	ret=rec_open("/dev/input/js0",&js);
	if (ret<0) {
		printf("Failed to initiate receiver! [%s]\n", strerror(ret));	
		return -1;
	}

	loop();

	rec_close(&js);

	shm_client_end();

	if (verbose) printf("Closing.\n");
	return 0;
}

