#ifndef PS3DEV_H
#define PS3DEV_H

struct s_rec {
    int yprt[4];
    int aux;

    int fd;
};

int constrain(int x, int min, int max);
int8_t rec_open(const char *path, struct s_rec *s);
int8_t rec_config(struct s_rec *s, int *t, int *v);
int8_t rec_update(struct s_rec *s);
int8_t rec_close(struct s_rec *s);

#endif

