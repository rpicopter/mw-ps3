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

int verbose; 

struct s_rec js;

int ret;
int err = 0;
int stop = 0;

struct S_MSP_BOXCONFIG boxconf;
struct S_MSG msg;

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

uint8_t checkUpdate(int8_t valid) {
	// valid argument values:
	// -1 - error
	// 0 - no update but read ok
	// 1 - update

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
		case 12: //??
			if (boxconf.supported[BOXLAND]) {
				boxconf.active[BOXLAND] = toggleBox(boxconf.active[BOXLAND]);
				mspmsg_SET_BOX_create(&msg,&boxconf);
				shm_put_outgoing(&msg);
				if (verbose) printf("BOXLAND: %u\n",boxconf.active[BOXLAND]);
			}
			break;
		case 13: //o - horion mode
			if (boxconf.supported[BOXHORIZON]) {
				boxconf.active[BOXHORIZON] = toggleBox(boxconf.active[BOXHORIZON]);
				mspmsg_SET_BOX_create(&msg,&boxconf);
				shm_put_outgoing(&msg);
				if (verbose) printf("HORIZON: %u\n",boxconf.active[BOXHORIZON]);
			}
			break;		
		case 14: //x - baro mode
			if (boxconf.supported[BOXBARO]) {
				boxconf.active[BOXBARO] = toggleBox(boxconf.active[BOXBARO]);
				mspmsg_SET_BOX_create(&msg,&boxconf);
				shm_put_outgoing(&msg);
				if (verbose) printf("BARO: %u\n",boxconf.active[BOXBARO]);
			}
			break;
		case 16: //select
			if (boxconf.supported[BOXGPSHOLD]) {
				boxconf.active[BOXGPSHOLD] = toggleBox(boxconf.active[BOXGPSHOLD]);
				mspmsg_SET_BOX_create(&msg,&boxconf);
				shm_put_outgoing(&msg);
				if (verbose) printf("BOXGPSHOLD: %u\n",boxconf.active[BOXGPSHOLD]);
			}
			break;
		default:
			printf("Unknown command %i\n",js->aux);
	}

	js->aux=-1; //reset receiver command

} 


void processIncoming() {
	#define FILTER_LEN 2
	static uint8_t filter[FILTER_LEN] = {113,119}; //shm_scan normally scans all IDs. But we are only interested in the provided ones;

	struct S_MSG m;
	uint8_t ret;
	ret=shm_scan_incoming_f(&m,filter,FILTER_LEN);
	while (ret) {
		switch(m.message_id) {
			case 113:
				mspmsg_BOX_parse(&boxconf,&m);
				break;
			case 119:
				mspmsg_BOXIDS_parse(&boxconf,&m);
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

	mspmsg_BOXIDS_create(&msg); //get supported boxes
	shm_put_outgoing(&msg);

	if (verbose) printf("Starting main loop...\n");

	rc.roll = rc.pitch = rc.yaw = rc.throttle = 1500;
	while (!stop) {
		if ((counter%50)==0) { //everysec refresh active boxes
			mspmsg_BOX_create(&msg); //get status for each box 
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
		}
		rc.aux1=rc.aux2=rc.aux3=rc.aux4=1500;

		//if (verbose) printf("%u %u %u %u\n",rc.roll,rc.pitch,rc.yaw,rc.throttle);
		mspmsg_SET_RAW_RC_create(&msg,&rc);
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

