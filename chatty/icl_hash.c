
/*
 membox Progetto del corso di LSO 2017/2018

 Dipartimento di Informatica Università di Pisa
 Docenti: Prencipe, Torquati
 
 Studente: Carlo Leo
 Matricola: 546155
 
 Si dichiara che il contenuto di questo file è in ogni sua parte 
 opera originale dell'autore
*/


#include <libchatty.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>


#include <limits.h>

pthread_mutex_t mtx[NBUCKETS]; //variabili di mutua esclusione per accedere alla tabella concorrentemente
extern pthread_mutex_t mtx_stats;
extern struct statistics  chattyStats;
extern pthread_mutex_t qlock_online; //
extern list*users_online;

/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberger's (PJW) generic hashing
 * algorithm based on Allen Holub's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
static unsigned int hash_pjw(void* key)
{
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

static int string_compare(void* a, void* b) 
{
    assert(a);
    assert(b);
    return (strncmp( (char*)a, (char*)b,MAX_NAME_LENGTH ) == 0);
}


/**
 * Create a new hash table.
 *
 *
 * @param[in] hash_function -- pointer to the hashing function to be used
 * @param[in] hash_key_compare -- pointer to the hash key comparison function to be used
 *
 * @returns pointer to new hash table.
 */

icl_hash_t *icl_hash_create(unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) )
{
    icl_hash_t *ht;
    int i;

    ht = (icl_hash_t*) calloc(1,sizeof(icl_hash_t));
    if(!ht) return NULL;

    ht->nentries = 0;
    ht->buckets = (icl_entry_t**)calloc(NBUCKETS,sizeof(icl_entry_t*));
    if(!ht->buckets) return NULL;

    ht->nbuckets = NBUCKETS;
    for(i=0;i<ht->nbuckets;i++)
        ht->buckets[i] = NULL;

    ht->hash_function = hash_function ? hash_function : hash_pjw;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare;

    return ht;
}

/**
 * Search for an entry in a hash table.
 *
 * @param ht -- the hash table to be searched
 * @param key -- the key of the item to search for
 *
 * @returns pointer to the data corresponding to the key.
 *   If the key was not found, returns NULL.
 */

void *icl_hash_find(icl_hash_t *ht, void* key, unsigned int index)
{
    icl_entry_t* curr = NULL;
    if(!ht || !key) return NULL;
    for (curr=ht->buckets[index]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->key, key)){
            return(curr->data);
        }
   return NULL;
}

/**
 * Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */

icl_entry_t *icl_hash_insert(icl_hash_t *ht, void* key, void *data)
{
    icl_entry_t *curr;
    unsigned int hash_val;
    if(!ht || !key) return NULL;
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    pthread_mutex_lock(&mtx[hash_val]);
    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->key, key)){
        	pthread_mutex_unlock(&mtx[hash_val]);
            return(NULL); /* key already exists */
        }

    /* if key was not found */
    curr = (icl_entry_t*)calloc(1,sizeof(icl_entry_t));
    if(!curr) return NULL;

    //curr->key = (void*) strdup((const char*)key); //faccio la copia del nickname. La memoria sarà liberata con free_key
    curr->key = (char*)calloc(1,sizeof(char)*(MAX_NAME_LENGTH+1));
    if(!curr->key) return NULL;
    strncpy(curr->key,(char*)key,MAX_NAME_LENGTH+1);
    curr->data = data;
    curr->next = ht->buckets[hash_val]; /* add at start */

    ht->buckets[hash_val] = curr;
    ht->nentries++;
    pthread_mutex_unlock(&mtx[hash_val]);
    pthread_mutex_lock(&mtx_stats); //uso l'insert solo quando un utente puo' essere registrato. Incremento il numero di utenti registrati
    chattyStats.nusers += 1;
    pthread_mutex_unlock(&mtx_stats);

    return curr;
}

/**
 * Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_delete(icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*))
{
    icl_entry_t *curr, *prev;
    unsigned int hash_val;

    if(!ht || !key) return -1;
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    pthread_mutex_lock(&mtx[hash_val]);

    prev = NULL;
    for (curr=ht->buckets[hash_val]; curr != NULL; )  {
        if ( ht->hash_key_compare(curr->key, key)) {
            if (prev == NULL) {
                ht->buckets[hash_val] = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            ht->nentries++;
            free(curr->data);//libero la memoria allocata per i campi della struct user
            free(curr);//libero la memoria occupata dalla entry della hash table
            pthread_mutex_unlock(&mtx[hash_val]);
            pthread_mutex_lock(&mtx_stats); //aggiorno il numero di utenti registrati
            chattyStats.nusers -= 1;
            pthread_mutex_unlock(&mtx_stats);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&mtx[hash_val]);
    return -1;
}

/**
 * Free hash table structures (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
//non lavora in mutua esclusione viene chiamata dal thread main quando si terimina
int icl_hash_destroy(icl_hash_t *ht, void (*free_key)(void*), void (*free_data)(void*))
{
    icl_entry_t *bucket, *curr, *next;
    int i;

    if(!ht) return -1;

    for (i=0; i<ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        if(bucket == NULL) continue;
        for (curr=bucket; curr!=NULL;){
            next=curr->next;
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data){
                free_data(curr->data);//libero la memoria allocata per i campi della struct user
                free(curr->data);//libero la memoria allocata per la struct user
                   }
           free(curr); //libero la memoria allocata per la entry della hash_table
           curr=next;
        }
    }

    if(ht->buckets) free(ht->buckets);
    if(ht) free(ht);

    return 0;
}

/**
 * Dump the hash table's contents to the given file pointer.
 *
 * @param stream -- the file to which the hash table should be dumped
 * @param ht -- the hash table to be dumped
 *
 * @returns 0 on success, -1 on failure.
 */

int icl_hash_dump(FILE* stream, icl_hash_t* ht)
{
    icl_entry_t *bucket, *curr;
    int i;

    if(!ht) return -1;

    for(i=0; i<ht->nbuckets; i++){
        bucket = ht->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->key)
                fprintf(stream, "icl_hash_dump: %s: %p\n", (char *)curr->key, curr->data);
            curr=curr->next;
        }
    }

    return 0;
}

void mutex_initializer(void){
    for(int i = 0; i < NBUCKETS ; i++){
        mtx[i] = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    }
}
//@overview : chiude il descrittore associato a un utente. Se è online lo rimuove dalla users_online
void disconect_user(icl_hash_t* ht,int fd_c){
    icl_entry_t* curr;
    unsigned int hash_val;
  if(user_is_online(users_online,&qlock_online,fd_c)){
        online_user * u = remove_user_online(users_online,&qlock_online,fd_c);
        assert(u);
        hash_val = (* ht->hash_function)(u->nickname) % ht->nbuckets;
        pthread_mutex_lock(&mtx[hash_val]);
        for(curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next){
            assert(curr->key);
             if (ht->hash_key_compare(curr->key,u->nickname)){
                user* usr = (user*) curr->data;
                usr->fd = -1;
                pthread_mutex_unlock(&mtx[hash_val]);
                free(u);
                close(fd_c);
                //printf(stderr,"chiuso fd: [%d] \n",fd_c );
                return;
            }
        }
     free(u); //quando disconnette un utente che si è deregistrato non lo trova nella hash table
     pthread_mutex_unlock(&mtx[hash_val]);
   }
else close(fd_c); //caso utente non oline poichè è fallita la prima richiesta oppure descrittore associato ad un'altra sessione
return;
}  



void free_key(void* key){
    char* s = (char*) key;
    free(s);
    return;
}

void free_data(void*data){
    user* usr = (user*) data; //casto il puntatore void*
    destory_circular_vector(usr->history); //libero la memoria occupata dai messaggi nella sua history;
    free(usr->history); //libero la memoria occupata dal ADT history
}
