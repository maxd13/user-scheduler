#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "../src/shared_defs.h"

static void ok_signal(int signal){
    
}

void test_standalone_scheduler_executes_abstract_exec_txt(void){
    int segment;
    pid_t scheduler;
    void* shared;

    signal(SIGUSR2, ok_signal);

    Process prio1 = create_process("echo/echo1.sh", PRIORITY | P7);
    Process prio2 = create_process("echo/echo2.sh", PRIORITY | P2);
    Process rt1 = create_process("echo/echo3.sh", REAL_TIME  | SET_I(10) | SET_D(15));
    Process rr1 = create_process("echo/echo4.sh", ROUND_ROBIN);
    Process rr2 = create_process("echo/echo5.sh", ROUND_ROBIN);
    Process rt2 = create_process("echo/echo6.sh", REAL_TIME  | SET_I(9) | SET_D(3));
    Process rt3 = create_process_with_relative_schedule("echo/echo6.sh", "echo/echo3.sh", REAL_TIME | MAKES_REFERENCE | SET_D(5));

    Process processes[7] = {prio1, prio2, rt1, rr1, rr2, rt2, rt3};

    
    // create shared memory area with key 0x2230
    // notice 0x2230 = 8752. Hexadecimal is better for use with ipcs.
    segment = shmget (0x2230, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if(segment == -1) handle("segment error\n.");

    // attach to shared memory area.
    shared = shmat(segment, 0, 0);
    if(shared == -1) handle("segment attachment error\n.");

    // start scheduler as child.
    if( (scheduler = fork()) < 0 ){ 
        handle("failed to start scheduler.\n");
    }

    else if (scheduler == 0){
        execl("../build/bin/debug/Scheduler", "scheduler", (char*) 0);   
    }
    else{

        printf("scheduler at %d\n", scheduler);

        for (int i = 0; i < 7; i++){
            printf("starting process %d at %s\n", i, path(processes[i]));
            memcpy(shared, processes[i], PROCESS_SIZE);
            kill(scheduler, SIGUSR1);
            pause();
            printf("received OK signal from scheduler\n");
            sleep(1);
        }
        
        puts("All processes added to scheduler, letting it run freely.");

        sleep(60);

        puts("Finishing scheduler.");

        kill(scheduler, SIGINT);
        waitpid(scheduler, NULL, 0);

        puts("Liberating memory.");

        // detach from shared memory area.
        shmdt(shared);

        // destroy shared memory segment.
        shmctl(segment, IPC_RMID, 0);

        printf("Test finished.\n");
    }
}

int main(int argc, char const *argv[]){
    test_standalone_scheduler_executes_abstract_exec_txt();
    return 0;
}

