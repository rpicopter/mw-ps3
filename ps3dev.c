#include <linux/joystick.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <error.h>

#include "ps3dev.h"


static struct js_event js_e[0xff];
static int ret;
static int i;
static int nc=0;

static int map(int x, int in_min, int in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int rec_open(const char *path, struct s_rec *s) {
	s->aux = -1;
	s->fd = open (path, O_RDONLY | O_NONBLOCK);	
	if (s->fd < 0) {
		printf("can't open [%s]: [%i] [%s]\n", path, s->fd, strerror(errno));
		s->fd = 0;
		return -1;
	}
	return 0;
}

int process_jsevent(struct s_rec *s, struct js_event *e) {
	if ((e->type & JS_EVENT_INIT)==JS_EVENT_INIT) {
		//printf("JS buffer full?\n");	
		return 0;
	}

	if (e->type==JS_EVENT_BUTTON && e->value) {
		//printf("Button pressed: %2u value: %4i\n",e->number,e->value);
		s->aux = e->number;
		nc = 1;
	}
    else if ((e->type==JS_EVENT_AXIS) && (e->number<4)) {
		//printf("A %2u VAL: %4i\n",e->number,e->value);
		switch(e->number) {
			case 0: s->yprt[0] = map(e->value,-32767,32767,1000, 2000); nc=1; break;
			case 1: s->yprt[3] = map(e->value,-32767,32767,2500, 650)-575; nc=1; break;
			case 2: s->yprt[2] = map(e->value,-32767,32767,1000, 2000); nc=1; break;
			case 3: s->yprt[1] = map(e->value,-32767,32767,2000, 1000); nc=1; break;
		}
	}
	return 0;	
}

int rec_update(struct s_rec *s) {
	ret = read (s->fd, js_e, sizeof(js_e));
	if (ret<0 && errno != EAGAIN) {
		printf("Error reading js: [%i] [%s]\n",errno,strerror(errno));
		return -1;
	} 
	if (ret>0) {
        nc = 0;
		for (i=0;i<ret/sizeof(struct js_event);i++)
		    process_jsevent(s,&js_e[i]);
		if (nc) return 1;
	}	
	
	return 0;
}

int rec_close(struct s_rec *s) {
	if (s->fd) {
		close(s->fd);
	}
	s->fd = 0;
	return 0;
}

