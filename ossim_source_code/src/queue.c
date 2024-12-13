#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
	if(q->size < MAX_QUEUE_SIZE && q->size >= 0) {
        	q->proc[q->size] = proc;
       		q->size++;
    }
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
	if(q->size == 0) {
        return NULL;
    }

    int highest_priority_index = 0;
    for(int i = 0; i < q->size; i++) {
        if(q->proc[i]->priority > q->proc[highest_priority_index]->priority) {
            highest_priority_index = i;
        }
    }

    struct pcb_t* highest_priority_pcb = q->proc[highest_priority_index];

    for(int i = highest_priority_index + 1; i < q->size; i++) {
        q->proc[i - 1] = q->proc[i];
    }
    q->size--;
    return highest_priority_pcb;
}

