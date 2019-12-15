/*
 membox Progetto del corso di LSO 2017/2018

 Dipartimento di Informatica Università di Pisa
 Docenti: Prencipe, Torquati
 
 Studente: Carlo Leo
 Matricola: 546155
 
 Si dichiara che il contenuto di questo file è in ogni sua parte 
 opera originale dell'autore
*/




#ifndef LIBCHATTY
#define LIBCHATTY 
#include <stdio.h>  //include che servono a tutti all'insieme di moduli che implementa l'interfaccia
#include <pthread.h> 
#include <stats.h> 
#include <string.h> 
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <message.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <connections.h>


/*


	ADT UTLIZZATI

*/
/*
	Vettore circolare
*/
typedef struct circular_vector{
	message_t* msgs; //contine i messaggi della history
	long count_msg; //conta il numero di messaggi che vegono inseriti, quando diventa >= del massimo dei messaggi che devo ricordare per ogni client inzio a riutilizzare entry
	int index_frist; //contine la posizione in message_t del messaggio piu' vecchio
	int size; //contiene il numero massimo di messaggi che devo ricordare
}circular_vector;
/*

	Struttura che viene usata dal parser del file di configurazione

*/

typedef struct configuration
{
	char* unixpath; 
	char *statfilename;
	int maxconnections;
	int threadsinpool;
	int maxmessagesize;
	int maxfilesize;
	int maxhits;
	char *dirname;
	
}configuration;

/** Elemento della coda.
 *
 */
typedef struct Node {
    void        * data;
    struct Node * next;
} Node_t;

/** Struttura dati coda.
 *
 */
typedef struct Queue {
    Node_t        *head;
    Node_t        *tail;
    unsigned long  qlen;
} Queue_t;

/*
	Elemento della tabella hash

*/
typedef struct icl_entry_s {
    void* key;
    void *data;
    struct icl_entry_s* next;
} icl_entry_t;

/*
	Tabella hash
*/
typedef struct icl_hash_s {
    int nbuckets;
    int nentries;
    icl_entry_t **buckets;
    unsigned int (*hash_function)(void*); //funzione hash
    int (*hash_key_compare)(void*, void*); //funzione di comparazione
} icl_hash_t;

/*
 	Argormenti per il thread adetto alla gestione dei segnali

*/

typedef struct arg_handler
{
	pthread_t tid; //tid del thread master, a cui mada SIGUSR2 per attivare il protocollo di terminzione
	char *statfilename; //path del file di configurazione
	sigset_t set; //set per sigwait
}arg_handler;

/*
	Utente online

*/

typedef struct online_user
{
	char nickname[MAX_NAME_LENGTH+1];
	int fd;
}online_user;

/*
	ELEMENTO DELLA LISTA

*/
typedef struct node{
	online_user usr;
	struct node* next;
}node;
/*  
	LISTA
*/
typedef struct list
{	node* head; //puntatore al primo elemento;
	unsigned int len;
}list;

/*
  Campo data associato a un nikname nella hash table. Il nikname viene utilizzato come chiave nella tabella hash(essendo univoco)

*/
typedef struct  user
{
	pthread_mutex_t mtx;//mutex associato all'utente per la scrittura conccorente sulla sua socket
	circular_vector* history;
	int fd; //descrittore della socket privata del client con il server
}user;


/*

	MACRO UTLIZZATE NELLE FUNZIONI

*/

#define MAX_LINE_READ 128 //caratteri massimi letti in una volta da un file
#define N 1024 //grandezza dei buffer temporanei che uso nelle varie funzioni
#define CONFPATH "./DATA" //path dove trovo il file di configurazione
#define NBUCKETS 512 //dimensione tabella hash
//costati utlizzate per la funzione hash
#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4)) 
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))
#define UNIX_PATH_MAX 128
#define SYSCALL(var,sys,string,val_err) \
			if ((var = sys) == val_err){ \
				perror(string);   \
				exit(1);\
			}\
/*
	MACRO UTILIZZATE PER EFFETUARE L'OPERAZIONE DI FIND NELLA HASH TABLE
	@pram: key: valore utilizzato per il calcolo della mutex
	       i: variabile usata per accedere all'array di mutex
	       ht: hash table
	       m: array di mutex sull hash_table
	 quando si fa l'operazione di unlock l'indice per accere all'array di mutex già è stato calcolato.
*/
#define LOCK(key,i,ht,m) \
			i =  (* ht->hash_function)(key) % ht->nbuckets;\
			pthread_mutex_lock(&m[i]);
#define UNLOCK(i,m) \
			pthread_mutex_unlock(&m[i]); 

/*

 FIRMA DELLE  FUNZIONI


*/
/*
	@effects: alloca ed inzializza una lista
	@param: puntatore alla lista da allocare ed inizializzare
	@return: puntatore alla lista inizializzata,NULL in caso di errore (errno settato)
*/
list* initList(list*l);
/*
	@effects: dealloca la lista la puntata da l
	@param: puntatore alla lista da deallocare

*/

void deleteList(list*l);
/*
 @effects: legge i parametri dal file di configurazione
 @modifies: parm
 @return: 1 se non vi sono errori 0 altrimenti. ERRNO settato
*/
int parser(configuration *parm, const char filename[]);
/** 
@effects: alloca ed inizializza una coda
@param: puntatore alla coda da inizializzare
@return: NULL in caso di errore (errno settato), putantaore alla coda inizializzata
 */
Queue_t *initQueue(Queue_t*q);

/** 
@effects: elimina una coda
@param: puntatore alla coda da eliminare
 */
void deleteQueue(Queue_t *q);

/** 
   @effects: aggiunge un elemento alla coda puntata da q
   @param: puntatore alla coda
   		   puntatore all'elemnto da aggiungere
   		   mutex sulla coda
   @return: 0, 1 in caso di errore (errno settato)
 */
int push(Queue_t *q, void *data,pthread_mutex_t* qlock);

/** 
 @effects: Estrae un dato dalla coda
 @return: data puntatore al dato estratto.
 */

void  *pop(Queue_t *q,pthread_mutex_t* qlock);
/*
 @param: puntatore alla coda
 @return: lunghezza attuale della coda
 operazione di sola lettura non lavora in mutua esclusione

*/
unsigned long length(Queue_t *q);
/*
 @effects: genera un buffer contete i nickname online
 @param: puntatore alla lista degli utenti online, puntatore alla variabile che contine la lunghezza del buffer creato, puntatore alla mutex della lista
 @return: punatatore al bufffer, NULL in caso di errore(errno settato);

*/
void print_queue(list*q,pthread_mutex_t*);
/*
	@effects: costruisce un buffer contente tutti i nickname online, scrive la lunghezza
			  in len_buf per il chiamante
	@param: lista degli utenti online
			puntatore dove scrivere la lunghezza del buffer creato
			mutex sulla lista
	@return: buffer  contente tutti i nickname online


*/
char* get_nickname_online(list*q,unsigned int*len_buf,pthread_mutex_t*);
/*
	@effects: aggiunge un utente alla lista online,aggiorna il numero di utenti online
	@param: lista degli utenti online
			puntatore alla struttura che lo identifica
			mutex sulla struttura
	@return: 1 on succes, 0 in caso di errore 
*/

int add_user_online(list *l, online_user *data,pthread_mutex_t* qlock);

/*
	@effects: rimuove dalla lista degli utenti online l'utente che ha descrittore associato fd_user,aggiorna il numero di utenti online
	@param: lista utenti online
			mutex sulla lista
			descrittore che identifica l'utente da rimuovere 
	@return : puntatore alla struttura rimossa, NULL se non lo trova

*/
online_user* remove_user_online(list*l,pthread_mutex_t*qlock,int fd_user);
/*

	@effects: controlla se un utente è online cercandolo per fd
	@param: lista utenti online
			mutex sulla lista
			descrittore da cercare
	@return: 1 se lo trova 0 altrimenti

*/

int user_is_online(list*l,pthread_mutex_t* qlock, int fd_user);
/*
	@effects: controlla se un utente è online cercandolo per nickname
	@param: lista utenti online
			mutex sulla lista
			nickname da cercare
	@return: 1 se lo trova 0 altrimenti

*/

int is_online(list*l,pthread_mutex_t*qlock,const char nik[]);

/*
	@effects: crea ed inizializza una tabella hash
	@return: puntatore alla tabella hash, NULL in caso di errore ERRNO opportunamente settato
	@param: hash_function funzione hash, hash_key_compare funzione di comparazione
*/
icl_hash_t *icl_hash_create(unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );


/*
	@effects: cerca key all'interno della struttura dati
	@return: il valore associato a key o NULL in caso la key non viene trovata
	@param: tabella hash, key da cercare, index indice che identifica la mutex

*/
void* icl_hash_find(icl_hash_t * ht, void* key, unsigned int index);

/*
	@effects: inseirisce in ht la coppia <key,data> se key non è presente,incrementa il numero di utenti registrati
	@param: tabella hash, <key,data> da inserire
	@return: NULL se key è stata trovata, puntore alla entry inserita
*/

icl_entry_t* icl_hash_insert(icl_hash_t *ht, void*key, void*data);
/*

	@effects : libera la memoria occupata dalla tabella hash
	@param : tabella hash, funzione per liberare la memoria occupata da una key, funzione per liberare la memoria occupata dalla parte data
*/
int icl_hash_destroy(icl_hash_t *, void (*)(void*), void (*)(void*));
/*
   @effects : rimuove dalla tabella hash key ed il suo valore associato
   @param :  tabella hash,chiave,funzione per liberare la memoria occupata da una key, funzione per liberare la memoria occupata dalla parte data
 
*/
int icl_hash_delete( icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*) );

/*
	@effects : inizializza le variabili di mutua esclusione(una per ogni lista di trabocco)

*/
void mutex_initializer(void);
 /*
	@param: key di una coppia <key,val> della hash table
	@effects : libera la memoria occupata da key;
*/
void free_key(void* key);
/*
   @param : data di una coppia <key,data> della hash table
   @effects : libera la memoria occupata da data

*/
void free_data(void* data);
/*
  @ovweview : funzione eseguita dal thread addetto alla gestione segnali
  @parm: arg_handler
*/
void* signal_handler(void* arg);
/*
	@overview : funzione eseguita dal master thread della pool
	@param : unixpath per la crezione del socket unix

*/
void* master(void*arg);
/*

	@overview : funzione eseguita dal worker della pool 
	@param ://////

*/
void* worker(void*arg);

/*
	@overview: funzione installata per la gestione di SIGUSR2
*/
void handler_sigusr2(int signum);


/*
	@effects: trova il nuovo massimo in set
	@param: set, massimo corrente
	@return: nuovo massimo on succes, -1 on error

*/
int updateMax(fd_set set, int fd_max);
void print_list(Queue_t*);
/*
	@param: lato scrittore della pipe, puntatore da scrivere sulla pipe
	@return : 1 se la scrittura è andata a buon fine, 0 altrimenti
*/
int write_on_pipe(int fd, int* fd_c);

/*

 @effects: rende l'utente identificato da fd_c offile, chiude il socket
 @param: descritore del client che si è disconnesso
*/

void disconect_user(icl_hash_t *ht,int fd_c);
/*
	@effects: dealloca la memoria occupata dal vettore circolare
	@param: puntatore al vettore circolare
*/

void destory_circular_vector(circular_vector*);
/*
	@effects: alloca ed inzializza un vettore circolare
	@param: puntatore al vettore circolare, dimensione del vettore.
	@return: NULL in caso di errore (errno settato), puntatore al vettore circolare allocato ed inizializzato

*/

circular_vector* init_circular_vector(circular_vector* ,int size);
/*
	@effects: inserisce un messaggio nel vettore
	@param: puntatore al vettore, messaggio da inserire
	@return: 0 in caso di errore (errno settato) , 1 in caso di successo

*/

int insert_msg(circular_vector*,message_t*);
//debug
void print_history(circular_vector*);

#endif //LIBCHATTY
