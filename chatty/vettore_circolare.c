#define _POSIX_C_SOURCE 200809L
#include <libchatty.h>
extern pthread_mutex_t mtx_stats;
extern struct statistics  chattyStats;

circular_vector* init_circular_vector(circular_vector* history, int size){
	history =(circular_vector*) calloc(1,sizeof(circular_vector));
	if(!history) return NULL;
	history->msgs = (message_t*) calloc(size,sizeof(message_t)); //vettore allocato ed inizializzato
	if(!history->msgs) return NULL;
	history->count_msg = 0;
	history->index_frist = 0;
	history->size = size;
	return history;
}
int insert_msg(circular_vector* history,message_t* msg){
	if(!msg || !history) return 0;
	int index;
	index = history->count_msg  % history->size;
	if(history->count_msg >= history->size){//inzio a riutilizzare entry
		 history->index_frist = (index+1)%history->size; //il messaggio piu vecchio è nella entry successiva a quella dove ho inserito il corrente.
		free(history->msgs[index].data.buf);//libero lo spazio occopuato da vecchio buffer
		history->msgs[index].data.buf = NULL;
	}
	//inserisco il messaggio nella history
	history->msgs[index] = *msg;
	history->msgs[index].data.buf = strdup(msg->data.buf);
	history->count_msg += 1;
	if(msg->hdr.op == TXT_MESSAGE){
		pthread_mutex_lock(&mtx_stats); //aggiorno il numero di messaggi non consegnati
		chattyStats.nnotdelivered += 1;
		pthread_mutex_unlock(&mtx_stats);
	}
	
	return 1;
}
//va chiamata nella delete e destory della hashtable
void destory_circular_vector(circular_vector *history){
	for(int i = 0; i < history->size ; i++)  free(history->msgs[i].data.buf); //dealloco i buffer dei messaggi presenti
	free(history->msgs);//dealloco l'array contente i messaggi
}
