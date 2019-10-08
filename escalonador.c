#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "shared_defs.h"

static void start_process(int signal);
static void process_ended(int signal);
static void context_switch(int signal);
static void finish(int signal);

static void* shared;
static Process p;
static int segment, num_proc;
static float round_robin_time;

int main(void){
    // set values for static variables.
    num_proc = 0;
    round_robin_time = 0.5;

    // create shared memory area with key 0x2230
    // notice 0x2230 = 8752. Hexadecimal is better for use with ipcs.
    segment = shmget (0x2230, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if(segment == -1) handle("segment error\n.");

    // attach to shared memory area.
    shared = shmat(segment, 0, 0);
    if(shared == -1) handle("segment attachment error\n.");

    // registers multiple signal handlers.
    signal(SIGUSR1, start_process);
    signal(SIGALRM, context_switch);
    signal(SIGCHLD, process_ended);
    signal(SIGINT, finish);
    signal(SIGQUIT, finish);
    
    //wait for signals.
    for(EVER) pause();
    return 0;
}

void start_process(int signal){
    num_proc++;
    
    // the handler should enter a critical region here to read from shared memory.
    p = (Process) shared;

}

static void finish(int signal){
    // detach from shared memory area.
    shmdt(shared);

    // IMPORTANT: if you don't clear away shared memory,
    // it will stay in the system (verifiable by the command ipcs),
    // and next time the server is called it will give a "segment error" since it will
    // be trying to create a segment that already exists.
    // Therefore the server is tasked with clearing away all shared memory segments
    // after the client attempts to close it. This is way this signal handler was created.

    // destroy shared memory segment.
    shmctl(segment, IPC_RMID, 0);

    //exit
    printf("A shutdown of the scheduler service was requested.\n Shutting down.\n");
    exit(EXIT_SUCCESS);
}

