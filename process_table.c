#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_defs.h"
#include "rax/rax.h"
#include <time.h>
#include <assert.h>

static struct queue_node{
    Process p;
    struct queue* next;

};

typedef struct queue_node* Node;

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

static struct pqueue{
    Node head;
    Node last;
    // number of milliseconds the processes in this queue were run
    //since the queue has been allowed to run.
    time_t time_run; 
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

static void insertQueue(ProcessQueue queue, Process p, time_t addtime){
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
#define MAX_HEAP 100
static struct process_heap{
	Process node[MAX_HEAP];
    char time_used; // total time in seconds used each 60 seconds for REAL-TIME processes in the heap.
	int size;
};

typedef struct process_heap* ProcessHeap;

static ProcessHeap createHeap(){
	ProcessHeap new = (ProcessHeap) malloc(sizeof(struct process_heap));
	if(!new) handle("no memory to create area for storing information about REAL-TIME processes\n");
    new->size = 0;
    new->time_used = 0;
	return new;
}

// compare two REAL-TIME processes for earlier start times based on
// their position in the internal array of a process heap.
// Tests i > j.
static char pcmpH(ProcessHeap h, int i, int j){
    return PROCESS_CMP(h->node[i], h->node[j]);
}

static int bin_search_rec(ProcessHeap h, int i, int j, int time){
    int m;
    if(i > j) return j;
    m = (i+ j) / 2;
    if (time > STIME(h->node[m]))
        return bin_search_rec(h, m+1, j, time);
    if (time < STIME(h->node[m]))
        return bin_search_rec(h, i, m-1, time);
    return m;
}

// finds the position in the heap corresponding to the current time.
static int bin_search(ProcessHeap h, int time){
    return bin_search_rec(h, 0, h->size - 1, time);
}

// finds the position the process should be in the heap, assuming the heap is ordered by the start time of processes.
static int searchProcess(ProcessHeap h, Process p){
    return bin_search(h, STIME(p));
}

static void insertHeap(ProcessHeap h, Process new){
    // remember: assertions should be disabled by gcc in production.
    assert(POLICY_REAL_TIME(policy(new)));
	if(h->size + 1 < MAX_HEAP){
        int index = searchProcess(h, new);
        if (index == h->size - 1 && PROCESS_CMP(new, h->node[index]))
            h->node[h->size] = new;
        else{
            for (int i = h->size; i > index; i--) h->node[i] = h->node[i - 1];
            h->node[index] = new;
        }
		h->size += 1;
        h->time_used += DTIME(new);
	} 
    else handle("not enough space in buffer to add REAL-TIME process at %s to the process table\n", path(new));
}

static void freeHeap(ProcessHeap h){
    if(!h || !h->size) return;
    for (int i = 0; i < h->size; i++) free_process(h->node[i]);
    free(h);
}

// time limits as percentages that each priority queue is allowed to occupy of the total CPU time. 
// sums 95.5, 0.5 seconds is used to run a single round robin process at a time.
// static const float priority_time_limits[8] = {36.7, 18.3, 12.1, 9.0, 7.2, 6.2, 5.3, 4.7};

typedef rax* PathTrie;

// number of priority levels
#define PRIOR_LEVELS 8
// total percentage of avaible time dedicated to running priority based processes.
#define PRIORITY_TIME 0.8

struct process_table{
    PathTrie absolute; // absolute path name lookup radix trie for REAL-TIME processes.
    PathTrie relative; // relative path name lookup radix trie for REAL-TIME processes.
    ProcessHeap real_time; // array for storing REAL-TIME processes.

    // uses 8 bits to verify for each priority level whether processes in it can run or not.
    // least significant bit used for 0 priority.
    char priority_runnable;
    ProcessQueue levels[PRIOR_LEVELS]; // queues of priority based processes.
    float priority_time_limits[PRIOR_LEVELS]; // amount of maximum time each priority level is allowed to run;
    float priority_total; // a weighted sum used to calculate priority_time_limits.
    char run_priority; // flag decides whether to run round robin or priority.

    ProcessQueue robin; // queue of round robin based processes.
    // time in milliseconds ranging in [0-4095] that each round robin process should be allowed to run.
    unsigned short quantum;
};

typedef struct process_table* ProcessTable;

// creates empty process table
ProcessTable create_table(){
    ProcessTable new = (ProcessTable) malloc(sizeof(struct process_table));
    if(!new) handle("no memory to create process table\n");
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
    new->quantum = 500; // default quantum is 0.5 secs.
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
    // time avaible for a given priority level.
    float priority_time = table->priority_time_limits[priority] * PRIORITY_TIME * time_avail;
    // if the time the priority level has run is greater than the avaible time, it should not continue
    // to run until the next minute.
    if ((table->levels[priority]->time_run / 1000.0) > priority_time){
        if(runnable(table, priority)) flip_runnable(table, priority);
        table->levels[priority]->time_run = 0;
    }
}

// calculate the fraction of the time each process level is allowed to run
// static void calculate_time_limits(ProcessTable table){
//     float total = 0;
//     uint8_t i;
//     for (i = 1; i <= PRIOR_LEVELS; i++)
//         if(table->levels[i - 1]) total += 1.0/i;
//     for (i = 1; i <= PRIOR_LEVELS; i++)
//         table->priority_time_limits[i - 1] = table->levels[i - 1] ? (1.0/i)/total : 0;
// }

// Inserts new process in the process table.
// Return 1 if the addition should cause the added process to be immediatly executed (preemption),
// and -1 if the process couldn't be added, otherwise 0.
// The cur_policy is the policy of the currently running process.
// The cur_time parameter gives the current time in seconds since the beggining of the minute.
// Both arguments are used to determine if preemption occurs.
// The time_run_last parameter should tell how long the added process ran for last time it was executed. 
// If it hasn't been executed yet, it should be set to 0.
char insertProcess(ProcessTable table, Process p, unsigned short cur_policy, unsigned char cur_time, time_t time_run_last){
    assert(table && p); // off in production
    assert(!validate_policy(cur_policy));
    assert(cur_time <= 60);
    unsigned short pol = policy(p);
    char preemption = 0;
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
                else resolve(p, ETIME((Process) reference));
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

            // we must now figure out whether preemption should occur or not.
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
            break;
        case PRIORITY:
            char priority = GET_PRIORITY(pol);
            if (!table->levels[priority]) table->levels[priority] = createQueue();
            
            // if the level is empty it is not being counted in the weighted sum,
            // so since we are adding a process to it, it should now start counting.
            if (!table->levels[priority]->head){
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


            // we must now figure out whether preemption should occur or not.
            // preemption should only occur for a process that has not yet run (this minute)
            // when the current process is also priority based and the added process 
            // has greater priority then the current one.
            // It should also be checked whether the priority level of the added process can still keep running
            preemption = !cur_time && 
                         POLICY_PRIORITY(cur_policy) &&
                         priority > GET_PRIORITY(cur_policy) &&
                         runnable(table, priority);
            break;
    }
    // We could argue based on the code that
    // if cur_policy is the policy of the added process, no preemption can ever occur.
    return preemption;
}

// The very soul of the scheduler.
// This routine determines what process should run next based on
// the policy of the current process and the current time.
// This also removes the chosen process from the table, UNLESS the process is REAL-TIME.
Process next_process(ProcessTable table, unsigned short cur_policy, unsigned char cur_time){
    // TODO
    return NULL;
}