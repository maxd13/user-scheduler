#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
// #include <assert.h>
#include "shared_defs.h"

static void finish(int signal);
static void start_process(int signal);
// expects SIGUSR1 signal to say that the scheduler finished reading shared memory.
static void scheduled(int signal);

// flag to check if memory area is ready to be written.
// a mutex of sorts.
static char ready;

static void* shared;
static int segment;
pid_t scheduler;

// instruction buffer
#define BUF_SIZE 100
#define INSTRUCTION_SIZE 200
static char buffer[BUF_SIZE][INSTRUCTION_SIZE];
static char buffer_pos;

int main(int argc, char const *argv[]){

    buffer_pos = 0;

    // clear garbage.
    for (unsigned char i = 0; i < BUF_SIZE; i++)
        for (unsigned char j = 0; j < INSTRUCTION_SIZE; j++)
            buffer[i][j] = 0;
        
    

    // create shared memory area with key 0x2230
    // notice 0x2230 = 8752. Hexadecimal is better for use with ipcs.
    segment = shmget (0x2230, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if(segment == -1) handle("segment error\n.");

    // attach to shared memory area.
    shared = shmat(segment, 0, 0);
    if(shared == -1) handle("segment attachment error\n.");

    ready = 1;

    // start scheduler as child.
    if( (scheduler = fork()) < 0 ){ 
        handle("failed to start scheduler.\n");
    }

    if (scheduler == 0){
        execl("Scheduler", "scheduler", (char*) 0);   
    }
    else{
        // registers multiple signal handlers.
        signal(SIGUSR1, scheduled);
        signal(SIGALRM, start_process);
        signal(SIGINT, finish);
        signal(SIGQUIT, finish);
        signal(SIGCHLD, finish);

        // set up alarm for start_process.
        alarm(5);

        // skip program name
        argc--; argv++;
        if (argc > 1 || !argc){
            printf("this program receives exactly one argument.\n");
            finish(0);
        }
        
        // keep feeding input strings to the buffer, 
        // either from the input file or from stdin.
        
        char instruction[INSTRUCTION_SIZE];
        char pos = 0;

        FILE* in = fopen(argv[0], "r");
        if (!in) handle("could not open file for reading.\n");
        printf("Reading from file...\n");
        while (fgets(instruction, INSTRUCTION_SIZE, in)){
            strcpy(buffer[pos++], instruction);
            printf("\tInstruction read: %s\n", instruction);
        }

        fclose(in);
        printf("Finished reading file.\n");
        printf("Waiting for signals.\n");
        for(EVER) pause();
        // else for(EVER){
        //     printf("interpreter>\n");
        //     getline(&instruction, &read, stdin);
        //     strncpy(buffer[pos++], instruction, read + 1);
        //     printf("\tInstruction read.\n");
        // }
        
    }
    
    return 0;
}

static void finish(int signal){

    // kill scheduler
    if (signal != SIGCHLD)
        kill(scheduler, signal);
    
    
    waitpid(scheduler, NULL, 0);

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

    // exit
    printf("Interpreter exiting...\n");
    exit(EXIT_SUCCESS);
}

static Process compile_instruction(char* instruction){
    char process_path[MAX_PATH];
    char priority;
    size_t path_len;
    char pI = 0;
    char pD;
    char process_Ipath[MAX_PATH] = "";
    unsigned short process_policy;
    unsigned short quantum = 0;

    if (strncmp("Run ", instruction, 4))
        handle("instructions must begin with 'Run'.\n");
    
    sscanf(instruction, "Run %s", process_path);
    path_len = strlen(process_path) + 4;

    if (!strncmp(" PR=", instruction[path_len], 4)){
        sscanf(instruction[path_len], " PR=%d,", &priority);
        if (priority < 0 || priority > 7)
            handle("priority level must be between 0 and 7 inclusive.\n");
        // change it so that it can be added to the policy.
        priority = priority << 4;
        process_policy = PRIORITY | priority;
    }
    else if (!strncmp(" I=", instruction[path_len], 3)){
        if (sscanf(instruction[path_len], " I=%d", &pI) == EOF)
            sscanf(instruction[path_len], " I=%s D=%d,", process_Ipath, &pD);
        else
            sscanf(instruction[path_len], " D=%d,", &pD);
        
        process_policy = REAL_TIME | (pI ? SET_I(pI) : MAKES_REFERENCE) | SET_D(pD);
    }
    else if (instruction[path_len] == ','){
        sscanf(instruction[path_len + 1], " Quantum=%d.", &quantum);
        process_policy = ROUND_ROBIN | SET_ROBIN_TIME(quantum);
    }
    else
        handle("invalid instruction.\n");
    
    if (process_Ipath) 
        return create_process_with_relative_schedule(process_path, process_Ipath, process_policy);
    else
        return create_process(process_path, process_policy);
}

static void start_process(int signal){
    if (ready && buffer[buffer_pos]){
        Process p = compile_instruction(buffer[buffer_pos++]);
        memcpy(shared, p, PROCESS_SIZE);
        kill(scheduler, SIGUSR1);
        ready = 0;
    }
    alarm(1);
}

static void scheduled(int signal){
    ready = 1;
}