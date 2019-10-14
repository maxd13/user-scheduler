// Interface for ProcessTable TAD
#pragma once

// Maximal number of REAL-TIME processes allowed in the process table.
#define MAX_RTIME 100
// number of priority levels for priority based processes.
#define PRIOR_LEVELS 8
// total percentage of avaiable time dedicated to running priority based processes.
#define PRIORITY_TIME 0.8

typedef struct process_table* ProcessTable;

// creates empty process table
ProcessTable create_table();

// frees the process table
void free_table(ProcessTable table);

// marks a REAL-TIME process as having already run this minute.
// If the process is not in the table, undefined behaviour occurs.
void setRan (ProcessTable table, Process p);

// checks whether a REAL-TIME process has already run this minute.
// If the process is not in the table, undefined behaviour occurs.
char getRan (ProcessTable table, Process p);

// Inserts new process in the process table.
// Return 1 if the addition should cause the added process to be immediatly executed (preemption),
// and -1 if the process couldn't be added, otherwise 0.
// The cur_policy is the policy of the currently running process, or NULL if there isn't any.
// The cur_time parameter gives the current time in seconds since the beggining of the minute.
// Both arguments are used to determine if preemption occurs.
// The time_run_last parameter should tell how long the added process ran for last time it was executed. 
// If it hasn't been executed yet, it should be set to 0.
char insertProcess(ProcessTable table, Process p, unsigned short cur_policy, unsigned char cur_time, unsigned int time_run_last);

// The very soul of the scheduler.
// This routine determines what process should run next based on the current time.
// This also removes the chosen process from the table, UNLESS the process is REAL-TIME.
// We assume that whenever this routine is called there is no current process running,
// i.e. this routine should only be called during a context switch.
// Returns NULL if there are no processes that can be run.
Process next_process(ProcessTable table, unsigned char cur_time);

// Resets information associated with the process table for the next minute execution.
// This routine should be called every 60 seconds.
void reset(ProcessTable table);