#pragma once
typedef struct heap* Heap;


Heap criaHeap(int* v, int n);
void subirHeap(Heap h, int i);
void descerHeap(Heap h, int i);
void inserirHeap(Heap h, int novo);
void removerHeap(Heap h);
int* heapsort(int* v, int n);