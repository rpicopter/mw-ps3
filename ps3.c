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
#include <sys/time.h>
#include <sched.h>
#include <sys/mman.h>
#include <getopt.h>
#include "ps3dev.h"
#include <mw/msp.h>
#include <mw/shm.h>
#include <mw/msg.h>


#define TIMEOUT_MS 1000
int verbose; 

struct s_rec js;

int ret;
int err = 0;
int stop = 0;

int trim[3] = {0,0,0};//in degrees * 1000
int mode = 0;

struct S_MSP_BOXCONFIG boxconf;
struct S_MSG msg;

#define FILTER_LEN 2
uint8_t filter[FILTER_LEN] = {113,119};

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

long TimeSpecMS(struct timespec *dt) {
        return dt->tv_sec*1000 + dt->tv_nsec/1000000;
}

struct timespec *TimeSpecDiff(struct timespec *ts1, struct timespec *ts2) //difference between ts1 and ts2
{
        static struct timespec ts;
        ts.tv_sec = ts1->tv_sec - ts2->tv_sec;
        ts.tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
        if (ts.tv_nsec < 0) { //wrap around
                ts.tv_sec--;
                ts.tv_nsec += 1000000000;
        }
        return &ts;
}

//-1 - error
// 0 - no update but read ok
// 1 - update
uint8_t checkUpdate(int8_t valid) {
	/*
	static struct timespec prev;
	static uint8_t initiated = 0;
	struct timespec cur;

	if (!initiated) {
		clock_gettime(CLOCK_REALTIME, &prev);
		initiated = 1;
	}


	clock_gettime(CLOCK_REALTIME, &cur);

	if (TimeSpecMS(TimeSpecDiff(&cur,&prev))>TIMEOUT_MS) 
		return 0;

	if (valid) prev = cur; //we got valid update to reset timer

	return 0;
	*/

	if (valid < 0) {
		printf("Receiver reading error: [%s]\n",strerror(ret));
		return 1;
	}
	return 0;
}

uint16_t toggleBox(uint16_t v) {
	return v>0?0:0xFFFF;
}

void do_adjustments(struct s_rec *js) {
	if (js->aux<0) return;

	switch (js->aux) {
		case 13: //o - horion mode
			if (boxconf.supported[BOXHORIZON]) {
				boxconf.active[BOXHORIZON] = toggleBox(boxconf.active[BOXHORIZON]);
				msp_SET_BOX(&msg,&boxconf);
				shm_put_outgoing(&msg);
				if (verbose) printf("HORIZON: %u\n",boxconf.active[BOXHORIZON]);
			}
			break;		
		case 14: //x - baro mode
			if (boxconf.supported[BOXBARO]) {
				boxconf.active[BOXBARO] = toggleBox(boxconf.active[BOXBARO]);
				msp_SET_BOX(&msg,&boxconf);
				shm_put_outgoing(&msg);
				if (verbose) printf("BARO: %u\n",boxconf.active[BOXBARO]);
			}
			break;
		default:
			printf("Unknown command %i\n",js->aux);
	}

	js->aux=-1; //reset receiver command

} 


void processIncoming() {
	struct S_MSG m;
	uint8_t ret;
	ret=shm_scan_incoming_f(&m,filter,FILTER_LEN);
	while (ret) {
		switch(m.message_id) {
			case 113:
				msp_parse_BOX(&boxconf,&m);
				break;
			case 119:
				msp_parse_BOXIDS(&boxconf,&m);
				break;
		}
		ret=shm_scan_incoming_f(&m,filter,FILTER_LEN);
	}
}

void loop() {   
	int16_t ret; 
	uint8_t counter = 0;    
	uint8_t baro_initiated = 0; //timeout for ps3 stick get back to position 0 after baro initialized                                
	struct S_MSP_RC rc;

	msp_BOXIDS(&msg); //get supported boxes
	shm_put_outgoing(&msg);

	if (verbose) printf("Starting main loop...\n");

	rc.roll = rc.pitch = rc.yaw = rc.throttle = 1500;
	while (!stop) {
		if ((counter%50)==0) { //everysec refresh active boxes
			msp_BOX(&msg);
			shm_put_outgoing(&msg);
		}

		processIncoming();

		mssleep(20);
		ret = rec_update(&js); 
		if (checkUpdate(ret)) {
			printf("Receiver update timeout\n!");
			stop=1;
			break;
		}
		
		do_adjustments(&js);
		
		rc.roll = constrain(js.yprt[2],1000,2000);
		rc.pitch = constrain(js.yprt[1],1000,2000);
		rc.yaw = constrain(js.yprt[0],1000,2000);
		if (boxconf.active[BOXBARO]) {
			if (baro_initiated>50) { //1 seconds to get throttle stick back to 0
				rc.throttle += js.yprt[3]/50;	//make is 1/25 sensitive
				rc.throttle = constrain(rc.throttle,950,2000);
			} else 
				baro_initiated++;
		} else {
			baro_initiated = 0;
			rc.throttle += js.yprt[3]/100;	//make is 1/25 sensitive
			rc.throttle = constrain(rc.throttle,950,2000);
			//rc.throttle = constrain(js.yprt[3]+950,950,2000);
		}
		rc.aux1=rc.aux2=rc.aux3=rc.aux4=1500;

		//if (verbose) printf("%u %u %u %u\n",rc.roll,rc.pitch,rc.yaw,rc.throttle);
		msp_SET_RAW_RC(&msg,&rc);
		shm_put_outgoing(&msg);

		counter++;
		if (counter==200) counter = 0;
	}
}


int main(int argc, char **argv) {

	signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);
	verbose = 1;

	if (verbose) printf("Opening shm\n");
	//dbg_init(251);
	//dbg_init(DBG_MSP|DBG_VERBOSE);
	if (shm_client_init()) return -1;

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

