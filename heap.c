#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_defs.h"
#include "rax/rax.h"
#include "heap.h"


#define MAX_HEAP 100
static struct process_heap{
	Process node[MAX_HEAP];
	int size;
};

typedef struct process_heap* ProcessHeap;

static ProcessHeap createHeap(){
	ProcessHeap h = (ProcessHeap) malloc(sizeof(struct process_heap));
	if(!h) handle("no memory to create empty process table %d\n");
    h->size = 0;
	return h;
}

static void upHeap(ProcessHeap h, int i){
	int j;
    Process aux;
	j = (i-1)/2;
	if(j >= 0)
		if(h->node[i] > h->node[j]){
			aux = h->node[i];
			h->node[i] = h->node[j];
			h->node[j] = aux;
			upHeap(h, j);
		}
}

static void downHeap(ProcessHeap h, int i){
	int j;
    Process aux;
	j = 2*i + 1;
	if(j <= h->size){
		if(j < h->size && h->node[j+1] > h->node[j]) j++;
		if(h->node[i] < h->node[j]){
			aux = h->node[i];
			h->node[i] = h->node[j];
			h->node[j] = aux;
			downHeap(h, j);
		}
	}
}

static void insertHeap(ProcessHeap h, Process novo){
	if(h->size < MAX_HEAP){
		h->node[h->size + 1] = novo;
		h->size += 1;
		upHeap(h, h->size);
	}
}

static Process removeHeap(ProcessHeap h){
	Process ret = NULL;
    if(h->size){
        ret = h->node[0];
		h->node[0] = h->node[h->size - 1];
		h->node[h->size - 1] = NULL;
		h->size -= 1;
		downHeap(h, 0);
	}
    return ret;
}

static Process peekHeap(ProcessHeap h){
    return h->size ? h->node[0] : NULL;
}

static void freeHeap(ProcessHeap h){
    if(!h || !h->size) return;
    for (size_t i = 0; i < h->size; i++)
        free_process(h->node[i]);
    free(h);
}
