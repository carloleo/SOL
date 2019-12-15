#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <libchatty.h>



//MUTEX 
pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER; //job queue
pthread_cond_t  qcond = PTHREAD_COND_INITIALIZER; //job queue
//VARIABILI EXTERN
extern volatile sig_atomic_t flag;
extern pthread_mutex_t mtx_stats;
extern struct statistics  chattyStats;


/* ------------------- funzioni di utilita' -------------------- */

static Node_t *allocNode()         { return calloc(1,sizeof(Node_t));  }
static Queue_t *allocQueue()       { return calloc(1,sizeof(Queue_t)); }
static void freeNode(Node_t *node) { free((void*)node); }
static void LockQueue(pthread_mutex_t *qlock)            { pthread_mutex_lock(qlock);   }
static void UnlockQueue(pthread_mutex_t *qlock)          { pthread_mutex_unlock(qlock); }
static void UnlockQueueAndWait()   { pthread_cond_wait(&qcond, &qlock); }
static void UnlockQueueAndSignal() {
     pthread_mutex_unlock(&qlock);
    pthread_cond_signal(&qcond);
   
}


Queue_t *initQueue(Queue_t * q) {
    q = allocQueue();
    if (!q) return NULL;
    q->head = allocNode();
    if (!q->head) return NULL;
    q->head->data = NULL; 
    q->head->next = NULL;
    q->tail = q->head;    
    q->qlen = 0;
    return q;
}

void deleteQueue(Queue_t *q) {
    while(q->head != q->tail) { //coda non vuota all'usita
        fprintf(stderr,"entro!!\n");
        Node_t *p = (Node_t*)q->head;
        q->head = q->head->next;
        // free(p->data);
        freeNode(p);
     }
     if (q->head){
        //fprintf(stderr, "elimino la testa\n");
        freeNode((void*)q->head);
    } 
}

int push(Queue_t *q, void *data, pthread_mutex_t*qlock) {
    Node_t *n = allocNode();
    if(!n) return 0;
    if(!data) return 0;
    n->data = data; 
    n->next = NULL;
    LockQueue(qlock);
    q->tail->next = n;
    q->tail       = n;
    q->qlen      += 1;
    UnlockQueueAndSignal();
    return 1;
}

void *pop(Queue_t *q,pthread_mutex_t*qlock) { 
    LockQueue(qlock);
    while(q->head == q->tail && !flag) {  
	UnlockQueueAndWait(); //empty queue
    }
    if(flag){
        UnlockQueue(qlock);
        return NULL;
    }  
    // locked
    assert(q->head->next);
    Node_t *n  = (Node_t *)q->head;
    void *data = (q->head->next)->data;
    q->head    = q->head->next;
    q->qlen   -= 1;
    assert(q->qlen>=0);
    UnlockQueue(qlock);
    freeNode(n);
    return data;
} 

// WARNING: accesso in sola lettura non in mutua esclusione
unsigned long length(Queue_t *q) {
    unsigned long len = q->qlen;
    return len;
}


//funzioni di debug
void print_queue(list*q,pthread_mutex_t* lock){
    node* curr = NULL;
    pthread_mutex_lock(lock);
    curr = q->head;
    printf("queue:\n");
    while(curr != NULL){
       printf("%s\n",curr->usr.nickname);
       fflush(stdout);
       curr = curr->next;
    }
    pthread_mutex_unlock(lock);
    return;
}

void print_list(Queue_t* q){
    LockQueue(&qlock);
    fprintf(stderr, "job:\n");
    Node_t *curr = q->head;
    while(curr != q->tail){
        fprintf(stderr,"%d\n",*((int*)curr->next->data) );
        curr = curr->next;
    }
    UnlockQueue(&qlock);
}


