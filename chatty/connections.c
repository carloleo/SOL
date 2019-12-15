
/*
 membox Progetto del corso di LSO 2017/2018

 Dipartimento di Informatica Università di Pisa
 Docenti: Prencipe, Torquati
 
 Studente: Carlo Leo
 Matricola: 546155
 
 Si dichiara che il contenuto di questo file è in ogni sua parte 
 opera originale dell'autore
*/




#include <sys/types.h>          
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <connections.h>
#include <libchatty.h>




int openConnection(char* path, unsigned int ntimes, unsigned int secs){

	if(secs > MAX_SLEEPING) return -1;
	if(ntimes > MAX_RETRIES) return -1;  //verifico la precondizione
	int fd_conn = -1;
	int error = 0;
	struct sockaddr_un sa;
	memset(&sa,0,sizeof(sa));
	strncpy(sa.sun_path,path,strlen(path)+1);
	sa.sun_family = AF_UNIX;
	
	if((fd_conn = socket(AF_UNIX,SOCK_STREAM,0))== -1){
		perror("SOCKET");
		return -1;
	}
	while(ntimes > 0 && !error){
		ntimes--;
		if(connect(fd_conn,(struct sockaddr*)&sa,sizeof(sa)) == -1){
			if(errno== ENOENT)sleep(secs);
			else return -1;
		}
		else break;
	}
	if(ntimes == 0) return -1;
	else return fd_conn;
}
int  readHeader(long connfd, message_hdr_t *hdr){
	int l;
	int operazione; //quando la funzione viene usata dal client operazione contiene l'esito della operazione richiesta
	//leggo operazione rihiesta(ops.h)
	if((l=readn(connfd,&operazione,sizeof(int))) == -1) return -1;
	
	if(l == 0) return 0;  //connessione chiusa
	hdr->op =(op_t)  operazione;
	//leggo chi richiede l'operazione
	l = readn(connfd,hdr->sender,MAX_NAME_LENGTH+1);
	if(l < 0) return l;
	return 1;
}
//legge solo il buffer della parte data
int readData(long fd, message_data_t *data){
	int l=0;
	unsigned int len_buf = 0;
	//mi aspetto di leggere prima l'header della parte dati
	if((l = readn(fd,&len_buf,sizeof(unsigned int))) == -1){ //leggo la lunghezza del buffer dati
		fprintf(stderr, "prima\n");
		return -1;
	}

	if(l == 0) return 0; //connessione chiusa
	if(readn(fd,data->hdr.receiver,MAX_NAME_LENGTH+1) == -1){
		fprintf(stderr, "seconda\n");
		return -1;
	}
	data->hdr.len = len_buf;
	if(len_buf == 0) return 1;
	data->buf = (char*) calloc(len_buf,sizeof(char));
	if(!data->buf) return -1; //calloc fail
	//leggo il body del messaggio
	fflush(stderr);
	l = readn(fd,data->buf,(size_t)len_buf);
	if(l <= 0){ //qualcosa nella readn è andato storto
		fprintf(stderr, "terza\n");
		return -1;
	} 
	//body letto con successo
	return 1;
}

int readMsg(long fd, message_t *msg){
	int tmp;
	tmp = readHeader(fd,(&(msg->hdr)));
	if(tmp <= 0) return tmp; //connesione chiusa o errori durante la lettura del header
	tmp = readData(fd,(&(msg->data)));
	return tmp;
}

//invia messaggio di richiesta
int sendRequest(long fd, message_t *msg){
	int l = 0;
	//mando il codice dell'operazione
	if((l = writen(fd,&(msg->hdr.op),sizeof(int))) <= 0) return l; //write non a buon termine
	//mando il nikname del sender,chi fa la richiesta
	if((l = writen(fd, msg->hdr.sender,MAX_NAME_LENGTH+1)) <= 0) return l;
	//mando la lunghezza del dell msg->data.buf
	if((l = writen(fd,&(msg->data.hdr.len),sizeof(int))) <= 0) return l;
	//mando il nikname del ricevente (receiver)
	if((l = writen(fd,msg->data.hdr.receiver,MAX_NAME_LENGTH +1)) <= 0)  return l;
	//invio il buffer del messaggio di richiesta che contiene il testo di un messaggio oppure il nome del file
	if(msg->data.hdr.len == 0) return 1; //se launghezza del buffer da inviare è zero evito la writen
	l = writen(fd,msg->data.buf,msg->data.hdr.len);
	if(l < 0) return l;
	return 1;
}
//invia il contenuti del file
int sendData(long fd, message_data_t *msg){
	int l = 0;
	//mando la lunghezza del buffer contenete i dati
	if((l = writen(fd,&(msg->hdr.len),sizeof(int))) < 0){
		fprintf(stderr, "prima write errno %d \n",errno);
		return l;
	}
	//quando nreciver è la stringa vuota la write torna 0 ma non significa che è fallita quindi controllo che non sia minore strettamente di 0
	if((l = writen(fd,&(msg->hdr.receiver),MAX_NAME_LENGTH+1)) < 0){
		fprintf(stderr, "secoda write\n");
		return l;
	}
	//mando i dati del messaggio
	if(msg->hdr.len == 0) return 1; //se il buffer 
	l = writen(fd,msg->buf,msg->hdr.len);
	if(l < 0) return l;
	return 1;
}

int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR ) continue;
	    else if( errno == EPIPE) return 0; //ignoro epipe
	    	else return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
		bufptr  += r;
    }
    return 1;
}

int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    else if(errno == ECONNRESET) return 0; //ignoro
	    return -1;
	}
	if (r == 0) return 0;   // gestione chiusura socket
     	left    -= r;
		bufptr  += r;
    }

    return size;
}
