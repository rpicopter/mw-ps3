#ifndef PS3DEV_H
#define PS3DEV_H

struct s_rec {
    int yprt[4];
    int aux;

    int fd;
};

int rec_open(const char *path, struct s_rec *s);
int rec_config(struct s_rec *s, int *t, int *v);
int rec_update(struct s_rec *s);
int rec_close(struct s_rec *s);

#endif

