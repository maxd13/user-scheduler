#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "process_table.h"
#include "rax/rax.h"

// Process Queue: internal abstract data type.
// Used for storing priority based and ROUND-ROBIN processes.

// Node for queue. It is implemented as a linked list.
static struct queue_node{
    Process p;
    struct queue* next;

};

typedef struct queue_node* Node;

// create Node
static Node cNode(Process p){
    Node new = (Node) malloc(sizeof(struct queue_node));
    if(!new) handle("no memory to create process table node for process at %s\n", path(p));
    new->p = p;
    new->next = NULL;
    return new;
}

static void insertNode(Node head, Process ins){
    if(!head) return;
    Node new = cNode(ins);
    // do be careful that it is not checked whether head->next was NULL or not.
    // we simply assume that we will never have to insert in a node in which it isnt,
    // so we can avoid the check.
    head->next = new;
}

// notice this doesnt liberate only the list of nodes, but also the processes.
static void freeNode(Node n){
    if(!n) return;
    freeNode(n->next);
    free_process(n->p);
    free(n);
}

// The actual queue.
static struct pqueue{
    Node head;
    Node last;
    // number of milliseconds the processes in this queue were run
    //since the queue has been allowed to run.
    unsigned int time_run; 
};

typedef struct pqueue* ProcessQueue;

// create empty process queue.
static ProcessQueue createQueue(){
    ProcessQueue new = (ProcessQueue) malloc(sizeof(struct pqueue));
    if(!new) handle("no memory to create a new priority queue of processes. %s\n");
    new->head = NULL;
    new->last = NULL;
    new->time_run = 0;
    return new;
}

static void insertQueue(ProcessQueue queue, Process p, unsigned int addtime){
    Node new = cNode(p);
    if(!queue->head){
        queue->head = new;
        queue->last = new;
    }
    else {
        queue->last->next = new;
        queue->last = new;
    }
    if(addtime) queue->time_run += addtime;
}

static Process popQueue(ProcessQueue queue){
    if (!queue || !queue->head)
        return NULL;
    Process p = queue->head->p;
    Node aux = queue->head;
    queue->head = queue->head->next;
    free(aux); // this doesnt free the processes in the node list.
    return p;
}

static char queueEmpty(ProcessQueue queue){
    return queue->head ? 0 : 1;
}

// frees processes too
static void freeQueue(ProcessQueue queue){
    freeNode(queue->head);
    free(queue);
}

// Heap for keeping REAL-TIME processes. It is actually implemented as an ordered array.
// Ideally I would implement a binary search tree to avoid the costs of using insertion sort 
// to insert into the heap array, or use a more sophisticated online sorting algorithm than insertion
// and still mantain the array based implementation. But if I keep thinking about optmizing data structures
// I am never going to actually finish this project, so insertion sort it is.
static struct process_heap{
    // the array of processes.
	Process node[MAX_RTIME];

    // flag tells whether given process ran this minute.
    char ran[MAX_RTIME];

    // total time in seconds used each 60 seconds for REAL-TIME processes in the heap.
    char time_used;
    
    // size of heap array.
	int size;
};

typedef struct process_heap* ProcessHeap;

static ProcessHeap createHeap(){
	ProcessHeap new = (ProcessHeap) malloc(sizeof(struct process_heap));
	if(!new) 
        handle("no memory to create area for storing information about REAL-TIME processes\n");
    for (uint16_t i = 0; i < MAX_RTIME; i++)
        new->ran[i] = 0;
    new->time_used = 0;
    new->size = 0;
	return new;
}

// finds the position in the heap corresponding to the current time.
// WARNING: May return position out of array bounds.
static int bin_search(ProcessHeap h, unsigned char time){
    if (!h || h->size <= 0) return -1;
    int i = 0;
    int j = h->size;
    int m;
    while (i < j){
        m = (i + j)/2;
        if (time > STIME(h->node[m])) i = m + 1;
        else j = m;
    }
    return i;
}

// finds the position the process should be in the heap, 
// assuming the heap is ordered by the start time of processes.
// WARNING: May return position out of array bounds.
static int searchProcess(ProcessHeap h, Process p){
    assert(h);
    assert(p);
    return bin_search(h, STIME(p));
}

static void insertHeap(ProcessHeap h, Process new){
    // remember: assertions should be disabled by gcc in production.
    assert(h);
    assert(POLICY_REAL_TIME(policy(new)));
	if(h->size + 1 < MAX_RTIME){
        int index = searchProcess(h, new); // if -1 the heap is empty
        if (index < 0)
            h->node[0] = new;
        else {
            // notice this gives us worst case O(n^2).
            for (int i = h->size; i > index; i--) 
                h->node[i] = h->node[i - 1];
            h->node[index] = new;
        }
		h->size += 1;
        h->time_used += DTIME(new);
	} 
    else handle("not enough space in buffer to add REAL-TIME process at %s to the process table\n", path(new));
}

// frees processes
static void freeHeap(ProcessHeap h){
    if(!h || !h->size) return;
    for (int i = 0; i < h->size; i++) free_process(h->node[i]);
    free(h);
}

// time limits as percentages that each priority queue is allowed to occupy of the total CPU time. 
// sums 95.5, 0.5 seconds is used to run a single round robin process at a time.
// static const float priority_time_limits[8] = {36.7, 18.3, 12.1, 9.0, 7.2, 6.2, 5.3, 4.7};

typedef rax* PathTrie;

struct process_table{
    // absolute path name lookup radix trie for REAL-TIME processes.
    PathTrie absolute;
    // relative path name lookup radix trie for REAL-TIME processes.
    PathTrie relative;
    // array for storing REAL-TIME processes.
    ProcessHeap real_time;

    // uses 8 bits to verify for each priority level whether processes in it can run or not.
    // least significant bit used for 0 priority.
    char priority_runnable;
    // queues of priority based processes.
    ProcessQueue levels[PRIOR_LEVELS];
    // amount of maximum time each priority level is allowed to run;
    float priority_time_limits[PRIOR_LEVELS];
    // a weighted sum used to calculate priority_time_limits.
    float priority_total;
    // flag decides whether to run round robin or priority.
    char run_priority;

    // queue of round robin based processes.
    ProcessQueue robin; 
    // time in milliseconds ranging in [0-4095] that each round robin process should be allowed to run.
    unsigned short quantum;
};



// creates empty process table
ProcessTable create_table(){
    ProcessTable new = (ProcessTable) malloc(sizeof(struct process_table));
    if (!new) handle("no memory to create process table\n");
    new->absolute = NULL;
    new->relative = NULL;
    new->real_time = NULL;

    new->priority_runnable = 0xFF; // sets all bits to 1.
    // make sure there is no garbage in the table which might accidentally evaluate to true.
    for (char i = 0; i < PRIOR_LEVELS; i++){
        new->levels[i] = NULL;
        new->priority_time_limits[i] = 0;
    }
    new->priority_total = 0.0; // when there are no processes there is no sum.
    // by default, we expect priority run mode to have precedence, so we set the flag up.
    new->run_priority = 1;

    new->robin = NULL;
    new->quantum = QUANTUM;
    return new;
}

// frees the process table
void free_table(ProcessTable table){
    // I think by now there is no need to remind the reader that assertions should be disabled in production.
    assert(table);
    // raxFree wont free the processes, 
    // but that is alright since they are referenced in the heap,
    // so we can free them when we free the heap.
    // There is a rax routine to free the processes, but if we did that
    // there would be dangling pointers when we tried to free the heap,
    // and this might cause undefined behaviour.
    if(table->absolute)  raxFree(table->absolute);
    if(table->relative)  raxFree(table->relative);
    if(table->real_time) freeHeap(table->real_time);

    for (uint8_t i = 0; i < PRIOR_LEVELS; i++)
        if(table->levels[i]) freeQueue(table->levels[i]);
    
    if(table->robin) freeQueue(table->robin);
    free(table);
}

// checks the runnable mask for whether a given priority level can run.
static char runnable(ProcessTable table, unsigned char priority){
    return (table->priority_runnable >> priority) & 1;
}

// flips the runnable flag of a priority level.
static void flip_runnable(ProcessTable table, unsigned char priority){
    table->priority_runnable ^= 1 << priority;
}

// decides whether a priority level is allowed to run or not.
// if it cant, it is set as not runnable, and run times for priority queues are cleared.
static void can_run(ProcessTable table, unsigned char priority){
    assert(table);
    assert(priority < PRIOR_LEVELS);
    // time avaible for priority and round robin processes, in seconds.
    char time_avail = 60 - table->real_time->time_used;
    // time avaible for the given priority level.
    float priority_time = table->priority_time_limits[priority] * PRIORITY_TIME * time_avail;
    // if the time the priority level has run is greater than the avaible time, it should not continue
    // to run until the next minute.
    if ((table->levels[priority]->time_run / 1000.0) > priority_time){
        if (runnable(table, priority)) flip_runnable(table, priority);
        table->levels[priority]->time_run = 0;
    }
}

// marks a REAL-TIME process as having already run this minute.
// If the process is not in the table, undefined behaviour occurs.
void setRan (ProcessTable table, Process p){
    table->real_time->ran[searchProcess(table->real_time, p)] = 1;
}

// checks whether a REAL-TIME process has already run this minute.
// If the process is not in the table, undefined behaviour occurs.
char getRan (ProcessTable table, Process p){
    return table->real_time->ran[searchProcess(table->real_time, p)];
}

// Inserts new process in the process table.
// Return 1 if the addition should cause the added process to be immediatly executed (preemption),
// and -1 if the process couldn't be added, otherwise 0.
// The cur_policy is the policy of the currently running process, or O if there isn't any.
// The cur_time parameter gives the current time in seconds since the beggining of the minute.
// Both arguments are used to determine if preemption occurs.
// The time_run_last parameter should tell how long the added process ran for last time it was executed. 
// If it hasn't been executed yet, it should be set to 0.
char insertProcess(ProcessTable table, Process p, unsigned short cur_policy, unsigned char cur_time, unsigned int time_run_last){
    assert(table && p); // off in production
    assert(cur_policy ? !validate_policy(cur_policy) : 1);
    assert(cur_time <= 60);
    unsigned short pol = policy(p);
    char preemption = 0;
    char priority;
    switch (PLP(pol)){
        case REAL_TIME: 
            if (!table->real_time) table->real_time = createHeap();

            // lets obtain the process path here for later convenience.
            char *s = path(p);

            // First we must figure out whether the added process makes reference to another
            // process or not. If it does, we should resolve the reference that the start time of 
            // the added process matches the end time of the process it makes reference to.
            // This way our scheduling algorithm can treat an Ipath process rougly the same way as a normal process.
            if (POLICY_MAKES_REFERENCE(pol)){
                char* ipath = Ipath(p);
                // find the referenced process in the path tries.
                void* reference = raxFind(*ipath == '/' ? table->absolute : table->relative, ipath, strlen(ipath));
                if (reference == raxNotFound){
                    fprintf(stderr, "Process to be added at %s makes reference to a non existent process.\n", s);
                    fprintf(stderr, "Non existent process was supposed to be at %s\n", ipath);
                    fprintf(stderr, "The added process will not be executed.\n");
                    return -1;
                }
                //sets this processes's start time to the end time of the referenced process.
                else {
                    resolve(p, ETIME((Process) reference));
                    pol = policy(p);
                }
            }

            // Now we must check whether this process conflicts with other processes in the heap.
            int pos = searchProcess(table->real_time, p);
            // check compatibility with previous process, if there is one.
            if (pos > 0 && ETIME(table->real_time->node[pos - 1]) > GET_I(pol)){
                fprintf(stderr, "Process to be added at %s conflicts with previous process.\n", s);
                fprintf(stderr, "The added process will not be executed.\n");
                return -1;
            }
            
            // check compatibility with next process, if there is one.
            if (pos < table->real_time->size - 1 && STIME(table->real_time->node[pos]) < PETIME(pol)){
                fprintf(stderr, "Process to be added at %s conflicts with subsequent process.\n", s);
                fprintf(stderr, "The added process will not be executed.\n");
                return -1;
            }

            // Next we check whether the process path is relative or absolute, and add it to the respective trie.
            if(*s == '/') {
                if (!table->absolute) {
                    table->absolute = raxNew();
                    if(!table->absolute) 
                        handle("no memory to allocate area to keep absolute path names of program files.\n");
                }
                if(!raxTryInsert(table->absolute, s, strlen(s), p, NULL)){
                    fprintf(stderr, "Process already exists at %s\n", s);
                    fprintf(stderr, "Since only one process per location is accepted, the added process will not be executed.\n");
                    return -1;
                }
            }
            else{
                if (!table->relative) {
                    table->relative = raxNew();
                    if(!table->relative) 
                        handle("no memory to allocate area to keep relative path names of program files.\n");
                }
                if(!raxTryInsert(table->relative, s, strlen(s), p, NULL)){
                    fprintf(stderr, "Process already exists at %s\n", s);
                    fprintf(stderr, "Since only one process per location is accepted, the added process will not be executed.\n");
                    return -1;
                }
            }
            
            // finally we insert the process in the process heap.
            insertHeap(table->real_time, p);

            // We must now figure out whether preemption should occur or not.
            // If there is no process currently running, it should not.
            if (!cur_policy) return 0;


            // If the current process is REAL-TIME preemption should only occur
            // if the current process should already have ended or is about to end in the current time,
            // and if the added process should be run immediately after the current process,
            // i.e. if the end time of the current process matches the start time of this process.
            // Otherwise, if the current process is not REAL-TIME, preemption should occur whenever the
            // time is right.
            if (POLICY_REAL_TIME(cur_policy)){
                char end_time = PETIME(cur_policy);
                preemption = (end_time == GET_I(pol)) && cur_time >= end_time;
            }
            else 
                preemption = cur_time >= GET_I(pol);

            break;
        case ROUND_ROBIN:
            if (!table->robin) table->robin = createQueue();
            insertQueue(table->robin, p, time_run_last); // add process to queue
            //check if quantum should be updated.
            unsigned short quantum = GET_QUANTUM(pol);
            if(quantum) table->quantum = quantum;
            // no preemption should occur in favour of a ROUND-ROBIN process.
            break;
        case PRIORITY:
            priority = GET_PRIORITY(pol);
            if (!table->levels[priority]) table->levels[priority] = createQueue();
            
            // if the level is empty it is not being counted in the weighted sum,
            // so since we are adding a process to it, it should now start counting.
            if (queueEmpty(table->levels[priority])){
                // update sum with value inversely proportional to priority level
                float add = 1.0/(priority + 1);
                table->priority_total += add;
                // priority time limits are fractions of the total sum.
                table->priority_time_limits[priority] = add/table->priority_total;
            }

            insertQueue(table->levels[priority], p, time_run_last); // add to queue

            // we should determine based on this addition whether the priority level of the added process
            // has already run enough or can keep running.
            can_run(table, priority);


            // We must now figure out whether preemption should occur or not.
            // If there is no process currently running, it should not.
            if (!cur_policy) return 0;

            // preemption should only occur for a process that has not yet run (this minute)
            // when the current process is also priority based and the added process 
            // has greater priority then the current one.
            // It should also be checked whether the priority level of the added process can still keep running
            preemption = !time_run_last && 
                         POLICY_PRIORITY(cur_policy) &&
                         priority > GET_PRIORITY(cur_policy) &&
                         runnable(table, priority);
            break;
    }
    // We could argue based on the code that
    // if cur_policy is the policy of the added process, no preemption can ever occur.
    return preemption;
}

// This internal function tells what process should run next on the assumption it is not
// a REAL-TIME process. See next_process below for details.
// This function was created to allow a recursive call to be made.
static Process case_no_real_time(ProcessTable table){
    // In case the next process is not REAL-TIME, 
    // to know its execution policy we simply have to check the specific flag for it.
    if (table->run_priority){

        // We figure out which priority level should be run by taking
        // the level with the greatest priority which is runnable and not empty.
        ProcessQueue level = NULL;
        unsigned char priority;
        for (priority = 0; priority < PRIOR_LEVELS; priority++){
            level = table->levels[priority];
            if (level && !queueEmpty(level) && runnable(table, priority))
                break;
            else level = NULL;
        }
        
        // Supposing no processes can run at all, we should give a chance for 
        // ROUND-ROBIN processes to run. 
        // So we wrap the rest of the code in an IF statement.
        if (level){
            // We pop the first process off the level queue.
            Process to_run = popQueue(level);

            // Since we popped a process, we need to figure out whether this casused
            // the queue to be made empty. In that case we need to update the total weighted sum.
            if (queueEmpty(level)){
                float subtract = 1.0/(priority + 1);
                table->priority_total -= subtract;
                table->priority_time_limits[priority] = 0.0;
            }
            
            // We also update the run_priority flag for the next execution.
            table->run_priority = 0;

            // finally just return
            return to_run;
        }
    }

    // Here, if the function hasn't returned, we handle the ROUND-ROBIN case.
    ProcessQueue robin = table->robin;

    // First we check whether there are processes to run.
    // If there aren't any and it is the proper turn to run ROUND-ROBIN processes,
    // then we should allow priority processes to run instead.
    // But if it was actually the turn  of priority processes and there weren't any to run,
    // then there are no processes to run at all, and we should just return NULL.
    // This is checked with the run_priority flag.
    // If NULL is returned it will always be the turn of priority processes next.
    if (!robin || queueEmpty(robin)){
        if (table->run_priority) 
            return NULL; // nothing can run
        // here we know that it is ROUND-ROBIN's turn, so we change the turn.
        table->run_priority = 1;
        return case_no_real_time(table);
    }

    // If there are processes, it is just a matter of popping one from the queue
    return popQueue(robin);
}

// The very soul of the scheduler.
// This routine determines what process should run next based on the current time.
// This also removes the chosen process from the table, UNLESS the process is REAL-TIME.
// We assume that whenever this routine is called there is no current process running,
// i.e. this routine should only be called during a context switch.
// Returns NULL if there are no processes that can be run.
Process next_process(ProcessTable table, unsigned char cur_time){
    assert(table);

    // First we need to determine the policy of the next process.
    // To do this we check first whether it is REAL-TIME or not, 
    // if it isn't will be really easy to determine its policy then.

    // figure out the position close to which the REAL-TIME process to run would be.
    int pos = bin_search(table->real_time, cur_time);
    
    // In case pos returns negative, there are no REAL-TIME processes to run.
    // So we check that.
    if (pos >= 0){

        // If the position is out of bounds, 
        // then only the last process may possibly be allowed to run.
        // So we handle that case.
        if(pos >= table->real_time->size){
            Process cur = table->real_time->node[table->real_time->size - 1];
            char cur_ran = table->real_time->ran[table->real_time->size - 1];
            if (cur_time < ETIME(cur) && !cur_ran) return cur;
        }
        else {
            // get previous and current process
            Process prev = pos > 0 ? table->real_time->node[pos - 1] : NULL;
            char prev_ran = pos > 0 ? table->real_time->ran[pos - 1] : 0;
            Process cur = table->real_time->node[pos];
            char cur_ran = table->real_time->ran[pos];

            // Now we know that start time of prev < cur_time 

            // So we check first if the end time of prev is already up, 
            // if it isn't, and the process has not yet run its course,
            // we should simply return prev to run
            if (prev && !prev_ran && ETIME(prev) > cur_time)
                return prev;

            // To see if it is cur that has to run, suffices to check
            // whether cur_time is in betwen the start time of cur
            // inclusive, and the end time of cur, exclusive.
            // If cur_time is less than the start time 
            // there will be a least 1 second of difference between them,
            // and the scheduler can run a lot of processes in one second.
            // However, if the ran flag is set for cur, we should really be executing
            // the next process if one is avaiable.
            if (STIME(cur) <= cur_time && cur_time < ETIME(cur)){
                if (!cur_ran) return cur;
                return next_process(table, ETIME(cur));
            }

            // However, we should also check the case cur MAKES_REFERENCE to prev, in which case,
            // if prev has already run, we should allow cur to run immediately, unless cur_ran is set.
            if (PMR(cur) && prev_ran){
                if (!cur_ran) return cur;
                if (cur_time < ETIME(cur)) return next_process(table, ETIME(cur));
            }
        }
    }

    // If the process to run is neither prev nor cur, it must have some other execution policy.
    // We created a helper function for this case, so we simply call it.
    return case_no_real_time(table);
}

// Resets information associated with the process table for the next minute execution.
// This routine should be called every 60 seconds.
void reset(ProcessTable table){
    assert(table);
    uint16_t i;
    
    // Mark all REAL-TIME processes as not run.
    if (table->real_time)
        for (i = 0; i < MAX_RTIME; i++)
            table->real_time->ran[i] = 0;
            
    // Reset time run for priority processes.
    for (i = 0; i < PRIOR_LEVELS; i++)
        if (table->levels[i]) 
            table->levels[i]->time_run = 0;

    // All levels are now allowed to run.
    table->priority_runnable = 0xFF;

    // Reset time run for ROUND-ROBIN processes.
    if (table->robin) table->robin->time_run = 0;
}

// Prints out the whole current state of the process table.
void table_show(ProcessTable table){
    if (!table){
        puts("\nTHERE IS NO TABLE TO PRINT.");
        return;
    }
    
    Process cur;
    ProcessQueue level;
    char cur_ran;
    unsigned char numlevels;
    unsigned char blocked_levels;
    unsigned char active_levels;
    unsigned char empty_levels;
    unsigned int total_priority_time;
    char robin_full;
    char real_time_full;
    int i;
    Node process;

    // figure out number of total, active, empty and blocked levels.
    numlevels = 0;
    blocked_levels = 0;
    total_priority_time = 0;
    for (i = 0; i < PRIOR_LEVELS; i++){
        level = table->levels[i];
        if  (level && !queueEmpty(level)){
            numlevels++;
            total_priority_time += level->time_run;
            if (!runnable(table, i)) blocked_levels++;
        } 
    }
    active_levels = numlevels - blocked_levels;
    empty_levels = PRIOR_LEVELS - numlevels;

    // check whether there are ROUND-ROBIN processes.
    robin_full = table->robin && !queueEmpty(table->robin);

    // check whether there are REAL-TIME processes.
    real_time_full = table->real_time && table->real_time->size;

    // Prints out general information about the table.
    puts("\nPROCESS TABLE:");
    printf("Quantum: %d milliseconds.\n", table->quantum);
    printf("Run precedence: %s.\n", table->run_priority ? "PRIORITY" : "ROUND-ROBIN");
    printf("\n");

    // Print REAL-TIME processes.
    if (real_time_full){
        puts("\tREAL-TIME PROCESSES:");
        printf("\tTotal time allocated: %d.\n", table->real_time->time_used);

        printf("\n");

        for (i = 0; i < table->real_time->size; i++){
            cur = table->real_time->node[i];
            cur_ran = table->real_time->ran[i];

            print_process(cur);

            if (cur_ran)
                printf("\t\tand has already run this minute.\n");
            else
                printf("\t\tand has not yet run this minute.\n");

            printf("\n");
        }
    }
    else
        printf("\tNo REAL-TIME processes.\n\n");

    // Print PRIORITY based processes.
    if (numlevels){
        puts("\tPRIORITY BASED PROCESSES:");
        printf("\tTotal time used: %d milliseconds.\n", total_priority_time);
        printf("\tActive levels: %d.\n", active_levels);
        printf("\tBlocked levels: %d.\n", blocked_levels);
        printf("\tEmpty levels: %d.\n", empty_levels);
        printf("\n");

        for (i = 0; i < PRIOR_LEVELS; i++){
            level = table->levels[i];
            if (level && !queueEmpty(level)){
                printf("\tPRIORITY LEVEL %d.\n", i);
                printf("\tStatus: %s.\n", runnable(table, i) ? "ACTIVE" : "BLOCKED");
                printf("\n");
                for (process = level->head; process; process = process->next){
                    cur = process->p;
                    print_process(cur);
                    printf(".\n");
                }
                printf("\tTime run: %d.\n", level->time_run);
                printf("\n");
            }
        }
    }
    else
        printf("\tNo PRIORITY based processes.\n");
    
    printf("\n");

    // Print ROUND-ROBIN processes.
    if (robin_full){
        puts("\tROUND-ROBIN PROCESSES:");
        printf("\tTotal time used: %d milliseconds.\n", table->robin->time_run);

        for (process = table->robin->head; process; process = process->next){
            printf("\n");
            cur = process->p;
            print_process(cur);
        }
    }
    else
        printf("\tNo ROUND-ROBIN processes.\n");
    
    puts("\nEND TABLE");
    
}

// Gets the current time quantum for round robin processes.
unsigned short getQuantum(ProcessTable table){
    return table->quantum;
}

// gets time in seconds until the next REAL-TIME process is supposed to run.
// return -1 if there is no next process.
char time_to_next_real_time(ProcessTable table, unsigned char cur_time){
    assert(table);

    // figure out the position close to which the REAL-TIME process to run would be.
    int pos = bin_search(table->real_time, cur_time);
    
    // In case pos returns negative, there are no REAL-TIME processes to run.
    if (pos < 0) return -1;

    // If the position is out of bounds,
    // there is no next process
    if (pos >= table->real_time->size) return -1;

    Process cur = table->real_time->node[pos];
    char cur_ran = table->real_time->ran[pos];

    // get supposed time
    char time = STIME(cur) - cur_time;

    if (time > 0) {
        if (!cur_ran) return time;
        else return time + DTIME(cur) + time_to_next_real_time(table, ETIME(cur));
    }
    
    // if time is 0, this means the current process to run is cur,
    // so what we want is actually the process after that.
    if (time == 0) 
        return DTIME(cur) + time_to_next_real_time(table, ETIME(cur));

    return -1;
}