#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include "process_table.h"

static void start_process(int signal);
static void process_ended(int signal);
static void context_switch(int signal);
static void finish(int signal);
static void debugger(int signal);

static void* shared;
static Process p;
static ProcessTable table;
static struct timeval cur_time;
static struct timeval start_time;
static struct timeval minute_start_time;
static struct itimerval timer;
static int segment;


// utility to fork new child process
static pid_t fork_util(const char* path){
    pid_t pid;

    if( (pid = fork()) < 0){ 
        handle("failed to start process at %s\n", path);
    }

    if (pid == 0){
        execlp(path, path, (char*) 0);
    }
    
    return pid;
}
// forks new child process to exec program at path and sends a SIGSTOP to new child.
static pid_t fork_stop(const char* path){
    pid_t pid = fork_util(path);
    kill(pid, SIGSTOP);
    return pid;
}
// gets relative time from the start of the minute, in seconds.
static unsigned char get_rel_time(){
    // get current time
    gettimeofday(&cur_time, NULL);
    return (unsigned char) (cur_time.tv_sec - minute_start_time.tv_sec);
}

// gets the time the current process is running, in milliseconds.
static unsigned int get_time_ran(){
    // get current time
    gettimeofday(&cur_time, NULL);
    return (__useconds_t) (cur_time.tv_usec - start_time.tv_usec) / 1000;
}

static void disarm_timer(){
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
}

// stops the current process, mark it as ran and make sure it is in the table.
// the current process is then set to NULL.
static void disable_current_process(){
    if (p){
        pid_t pid = get_pid(p);
        unsigned short pol = policy(p);
        __useconds_t time_ran = get_time_ran();
        // if the process isn't real time it was removed from the table
        // when ran, so it must be readded. If it is real time suffices
        // to mark it as having run.
        if (!POLICY_REAL_TIME(pol))
            // no preemption should be possible to occur.
            insertProcess(table, p, pol, 0, time_ran);
        else setRan(table, p);
        // stop the process.
        kill(pid, SIGSTOP);
    }
    p = NULL;
}

// checks whether the minute is up, 
// and if it is resets the table and the minute timer.
// then performs a context switch
static void check_minute(){
    if (get_rel_time() >= 60){
        reset(table);
        disarm_timer();
        disable_current_process();
        gettimeofday(&minute_start_time, NULL);
        context_switch(0);
    }
}

// static FILE* tmp;

int main(void){
    // set values for static variables.
    table = create_table();
    p = NULL;
    
    // tmp = fopen("scheduler.txt", "w");

    puts("BR 0.");

    // reference shared memory area with key 0x2230
    // notice 0x2230 = 8752. Hexadecimal is better for use with ipcs.
    segment = shmget (0x2230, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if(segment == -1) handle("segment error\n.");

    // attach to shared memory area.
    shared = shmat(segment, 0, 0);
    if(shared == -1) handle("segment attachment error\n.");

    puts("BR 1.");
    // fclose(tmp);

    // registers multiple signal handlers.
    signal(SIGUSR1, start_process);
    signal(SIGUSR2, debugger); // show signal
    signal(SIGSTOP, debugger);
    signal(SIGCONT, debugger);
    signal(SIGALRM, context_switch);
    signal(SIGCHLD, process_ended);
    signal(SIGINT, finish);
    signal(SIGQUIT, finish);
    
    // get current time
    gettimeofday(&minute_start_time, NULL);

    //wait for signals.
    // whenever a signal is handled, check whether the minute is up
    for(EVER) {
        pause();
        check_minute();
    }
    return 0;
}

void start_process(int signal){
    Process to_add;
    pid_t to_add_pid;
    unsigned char relative_time;

    // since access to shared memory is only made after a signal is received
    // from the interpreter, to mean that reading is OK, we don't need to create
    // mutual exclusion here when reading from shared memory.
    to_add = (Process) shared;

    // create the actual process and record PID.
    to_add_pid = fork_stop(path(to_add));
    to_add = process_pid(to_add, to_add_pid);

    // now we can allow the interpreter to change memory by sending it a signal.
    to_add_pid = getppid();
    kill(to_add_pid, SIGUSR1);

    // update current time
    relative_time = get_rel_time();

    // insert the new process in the process table,
    // and handle preemption.
    if (insertProcess(table, to_add, p ? policy(p) : 0, relative_time, 0) || !p){
        disarm_timer();
        context_switch(0);
    }
}

static void finish(int signal){
    // detach from shared memory area.
    shmdt(shared);

    // destroy process table.
    free_table(table);

    // fclose(tmp);
    
    // free current process in case it isn't at the table
    if (p && !POLICY_REAL_TIME(policy(p))){
        kill(get_pid(p), SIGKILL);
        free_process(p);
    }

    //exit
    printf("A shutdown of the scheduler service was requested.\n Shutting down...\n");
    exit(EXIT_SUCCESS);
}

static void context_switch(int signal){
    // seconds
    unsigned char relative_time = get_rel_time();
    pid_t pid;
    char time_to_next_alarm;
    unsigned short pol;
    // if there is a current process we need to
    // make it inactive.
    disable_current_process();

    p = next_process(table, relative_time);
    // if no process can be currently run, we must set the next process
    // to run to be the next real time process.
    if (!p){
        time_to_next_alarm = time_to_next_real_time(table, relative_time) * 1000;
        // if there are no next real time process, no process can be run,
        // in that case we must check whether we are in the end of that minute,
        // and then we must reset the table and start over for the next minute.
        // if we are not in the end, we can just return. A call to check_minute
        // covers every case.
        if (time_to_next_alarm < 0) 
            return check_minute();        

        // otherwise we set the time accordingly.
        timer.it_value.tv_sec = 0;
        timer.it_value.tv_usec = time_to_next_alarm * 1000;
        setitimer(ITIMER_REAL, &timer, NULL);
        return;
    }
    
    // start p.
    pid = get_pid(p);
    kill(pid, SIGCONT);

    // set start time for current process.
    gettimeofday(&start_time, NULL);

    // figure out when to context switch next.
    pol = policy(p);
    switch (PLP(pol)){
    case REAL_TIME:
        time_to_next_alarm = GET_D(pol) * 1000;
        break;
    case ROUND_ROBIN:
        time_to_next_alarm = getQuantum(table);
        break;
    case PRIORITY:
        time_to_next_alarm = time_to_next_real_time(table, relative_time) * 1000;
        break;
    }

    // it shouldnt be possible for time_to_next_alarm to be negative here.
    assert(time_to_next_alarm > 0);

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = time_to_next_alarm * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}

static void process_ended(int signal){
    char* str = path(p);
    pid_t pid = get_pid(p);
    
    waitpid(pid, NULL, 0);

    pid = fork_util(str);
    set_pid(p, pid);

    disarm_timer();
    context_switch(0);
}

static void debugger(int signal){
    
}