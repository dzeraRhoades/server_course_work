#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>



typedef struct
{
    int id;
    char string[BUFSIZ];
} msg;