
#include<stdbool.h>
#include<stdlib.h>
#include<stdio.h>

#include <termios.h>
#include <unistd.h> // for STDIN_FILENO

#include "utils.h"


void my_assert(bool r, const char *fcname, int line, const char *fname){
    if(!r){
        fprintf(stderr, "ERROR: my_assert FAIL: %s() line %d in %s\n", fcname, line, fname);
    }
}
void *my_alloc(size_t size){
    void *ret = malloc(size);
    if(!ret){
        fprintf(stderr, "ERROR: cannot malloc!\n");
        exit(101);
    }
    return ret;
}

void call_termios(int reset)
{
    static struct termios tio, tioOld;
    tcgetattr(STDIN_FILENO, &tio);
    if (reset)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &tioOld);
    }
    else
    {
        tioOld = tio; // backup
        cfmakeraw(&tio);
        tio.c_oflag |= OPOST;
        tcsetattr(STDIN_FILENO, TCSANOW, &tio);
    }
}

void info(const char *str){
    fprintf(stderr, "INFO: %s\n\r", str);
}
void debug(const char *str){
    fprintf(stderr, "DEBUG: %s\n\r", str);
}
void error(const char *str){
    fprintf(stderr, "ERROR: %s\n\r", str);
}
void warn(const char *str){
    fprintf(stderr, "WARN: %s\n\r", str);
}

/* end of utils.c */