/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <libchatty.h>
#include <stats.h>
#include <fcntl.h>          
#include <sys/stat.h>
#include <dirent.h>







//MUTEX
pthread_mutex_t mtx_stats = PTHREAD_MUTEX_INITIALIZER; //per aggiornare concorrentemente le statitistiche
pthread_mutex_t mtx_pipe = PTHREAD_MUTEX_INITIALIZER; //per scrivere sulla pipe concorrentemente 


//DATI CONDIVISI NEL POOL
struct statistics  chattyStats = {0,0,0,0,0,0,0};
sig_atomic_t flag = 0;
icl_hash_t *hash_table = NULL; //struttura dati utilizzata per la memorizzazione degli utenti
int fd[2]; //pipe tra thread master e worker, dove worker è scrittore e master è lettore. Serve per comunicare al master quali fd(relativi ai client) deve reinseire nella maschera della select
Queue_t *job_queque = NULL; //coda dove il master produce lavori (file descriptor su cui ascolatore la richiesta del client) e il worker + consumatore.
list *users_online = NULL; //lista degli utenti online
configuration param; //struct che contine i paramatri configurazione del server
static void usage(const char *progname){
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	//CONFIGURAZIONE
	if(argc != 3) usage(argv[0]);
	if(strncmp(argv[1],"-f",2)) usage(argv[0]);
	int notused = 0;
	memset((char*)&param,0,sizeof(configuration)); //inizializzo la struct che contiene i parametri di configurazione
	SYSCALL(notused,parser(&param,argv[2]),"PARSER",0);
	//GESTIONE DEI THREAD
	struct sigaction s,ss;
	sigset_t set;
	DIR* dfiles = NULL;
	memset(&s,0,sizeof(s));
	memset(&ss,0,sizeof(ss));
	s.sa_handler = SIG_IGN; //ignoro il segnale sigpipe per evitare che il server termini per la lettura su socket chiuese da un client che si è disconesso(dato che tale operazione è implicita)
	ss.sa_handler = handler_sigusr2;
	SYSCALL(notused,sigaction(SIGPIPE,&s,NULL),"SIGCATION SIGPIPE FAIL",-1);//installo gestore per SIGPIPE(ignorato)
	//installo gestore per SIGUSR2 è unico per tutto il processo
	SYSCALL(notused,sigaction(SIGUSR2,&ss,NULL),"SIGCATION SIGUSR2 FAIL",-1);//installo gestore per SIGUSR2, segnale utilizzato internamente al server per far terminare il pool
	//azzero la maschera 
	SYSCALL(notused,sigemptyset(&set),"SIGEMPTYSET FAIL",-1);
	//aggiungo alla maschera i segnali che deve gestire il thread signal hander
	if(sigaddset(&set,SIGUSR1) || sigaddset(&set,SIGINT) || sigaddset(&set,SIGTERM) || sigaddset(&set,SIGQUIT)){
		fprintf(stderr, "SIGADDSET FAIL \n");
		exit(EXIT_FAILURE);
	}
	//installo la maschera del processo
	if(pthread_sigmask(SIG_SETMASK,&set,NULL)){//Viene ereditata da tutti i thread
		fprintf(stderr, "PTHREAD_SIGMASK FAIL\n");
		exit(EXIT_FAILURE);
	}

   pthread_t tid[param.threadsinpool]; //array di tid relativi ai thread della pool
   pthread_t handelr;
   //ALLOCAZIONE STRUTTURE DATI CONDIVISE NEL POOL
   SYSCALL(hash_table,icl_hash_create(NULL,NULL),"HASH CREATE",NULL);
   SYSCALL(job_queque,initQueue(job_queque),"JOB QUEUE INIT",NULL);
   SYSCALL(users_online,initList(users_online),"USERS_ONLINE QUEUE INIT",NULL);
   //CREO LA CARTELLA PER LA POSTFILE_OP
   if(mkdir(param.dirname,0700) == -1 && errno != EEXIST){
   		perror("MKDIR");
   		exit(EXIT_FAILURE);
   }
   //MI SPOSTO NELLA CARTELLA
   SYSCALL(notused,chdir(param.dirname),"CHDIR",-1);
	SYSCALL(notused,pipe(fd),"PIPE",-1);;
	//creo il master thread
	SYSCALL(notused,pthread_create(&tid[0],NULL,master,(void*)param.unixpath),"CREATING MASTER",-1);
	//creo i worker
	for(int i = 1 ; i < param.threadsinpool ; i++){
	SYSCALL(notused,pthread_create(&tid[i],	NULL,worker,(void*)NULL),"CREATING WORKER",-1);
	}
	arg_handler *arg = NULL;
	SYSCALL(arg,(arg_handler*)calloc(1,sizeof(arg_handler)),"CALLOC FAIL ARG HANDLER",NULL);
	arg->set = set;
	arg->tid = tid[0];
	arg->statfilename = param.statfilename; 
	SYSCALL(notused,pthread_create(&handelr,NULL,signal_handler,(void*)arg),"CREATING HANDLER SIGNAL THREAD",-1)


	pthread_join(handelr,NULL); //il thread handler termina solo quando arrivano segnali che implicano la terminazione
	pthread_join(tid[0],NULL);  //poi aspetto il pool che deve terminare
	for(int i = 1; i < param.threadsinpool; i++){
	pthread_join(tid[i],NULL);
	}
	close(fd[0]);
	close(fd[1]);
	deleteQueue(job_queque);
	deleteList(users_online);
	icl_hash_destroy(hash_table, free_key ,free_data);
	free(param.unixpath);
	free(param.statfilename);
	free(param.dirname);
	free(arg);
	exit(EXIT_SUCCESS);
}
