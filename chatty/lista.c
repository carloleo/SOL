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
extern pthread_mutex_t mtx_stats;
extern struct statistics  chattyStats;
pthread_mutex_t qlock_online = PTHREAD_MUTEX_INITIALIZER;//lista degli utenti online


list* initList(list*l){
    l = (list*) calloc(1,sizeof(list));
    if(!l) return   NULL;
    l->head = NULL;
    l->len = 0;
    return l;
}
int add_user_online(list* l, online_user* u,pthread_mutex_t*lock){
    node* tmp = (node*) calloc(1,sizeof(node));
    if(!tmp) return 0;
    memset(tmp->usr.nickname,'\0',MAX_NAME_LENGTH+1);
    strncpy(tmp->usr.nickname,u->nickname,MAX_NAME_LENGTH+1);
    tmp->usr.fd = u->fd;
    free(u);//libero la memoria allocata dalla struct u
    pthread_mutex_lock(lock);
    if(l->head == NULL){//lista vuolta
        l->head = tmp;
    }
    else{//lista non vuota
        tmp->next = l->head;
        l->head = tmp;
    }
    l->len += 1;
    pthread_mutex_unlock(lock);
    pthread_mutex_lock(&mtx_stats);
    chattyStats.nonline += 1;
    pthread_mutex_unlock(&mtx_stats);

    return 1;
}

online_user* remove_user_online(list* l,pthread_mutex_t*qlock,int fd_usr){
    pthread_mutex_lock(qlock);
    node*curr = l->head;
    node* prev = NULL;
    while(curr){
        if(curr->usr.fd == fd_usr){
            if(prev) prev->next = curr->next;
            else
                l->head = curr->next;
        l->len -= 1;
        pthread_mutex_unlock(qlock);
        online_user * copy = (online_user*) calloc(1,sizeof(online_user));
        if(!copy) return NULL;
        strncpy(copy->nickname,curr->usr.nickname,MAX_NAME_LENGTH);
        copy->fd = curr->usr.fd;
        free(curr);
        pthread_mutex_lock(&mtx_stats);
        chattyStats.nonline -= 1;
        pthread_mutex_unlock(&mtx_stats);
        return copy;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(qlock);
    return NULL;
}


int user_is_online(list*l,pthread_mutex_t*qlock,int fd_usr){
    pthread_mutex_lock(qlock);
    node* curr = l->head;
    int found = 0;
    while(curr && !found){
        if(curr->usr.fd == fd_usr) found = 1;
        curr = curr->next;
    }
    pthread_mutex_unlock(qlock);
    if(found) return 1;
    return 0;
}


//chiamata solo dal thread main al momento dell'uscita
void deleteList(list*l){
    node* curr = l->head;
    node* next = NULL;
    while(curr){ //dealloco gli elementi della lista
        next = curr->next;
        free(curr);
        curr=next;
    }
    if(l) free(l); //dealloco la lista
}

int is_online(list*l,pthread_mutex_t*qlock,const char nik[]){
    pthread_mutex_lock(qlock);
    node* curr = l->head;
    int found = 0;
    while(curr && !found){
        if(!strcmp(curr->usr.nickname,nik)) found = 1;
        curr = curr->next;
    }
    pthread_mutex_unlock(qlock);
    if(found) return 1;
    return 0;
}

char* get_nickname_online(list* l,unsigned int *len_buf,pthread_mutex_t* mtx){
    char* buf = NULL;
    char* tmp = NULL;
    node* curr = NULL;
    int offset = (MAX_NAME_LENGTH + 1);
    pthread_mutex_lock(mtx);//metro creo il buffer, la lunghezza della lista deve rimanere costante
    buf = (char*) malloc((l->len*offset)*sizeof(char));
    memset(buf,'\0',l->len*offset);
    if(!buf) return NULL;
    tmp = buf; //salvo l'indirizzo di partenza
    curr = l->head;
    while(curr != NULL){
        assert(curr);
        strncpy(buf,curr->usr.nickname,MAX_NAME_LENGTH);
        buf += offset; //incremento l'indizzo di partenza per scrivere il prossimo nickname
        curr = curr->next;
    }
    *len_buf = l->len * offset;
    pthread_mutex_unlock(mtx);
    buf = tmp;
    return buf;

}
