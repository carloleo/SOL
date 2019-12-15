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

extern volatile sig_atomic_t flag;
extern pthread_mutex_t mtx_pipe;
extern struct statistics  chattyStats;
extern pthread_mutex_t mtx_stats;

int parser(configuration *parm, const char filename[]){
	FILE *fp = NULL;
	if((fp = fopen(filename,"r")) == NULL) return 0;
	int i = 0;
	int j = 0;
	char line[MAX_LINE_READ];
	char tmp[MAX_LINE_READ];
	char *token = NULL;
	memset(line,0,MAX_LINE_READ); //valgrind happy
	memset(tmp,'\0',MAX_LINE_READ);
	while(fgets(line,MAX_LINE_READ,fp) != NULL){
		if(line[0] == '#') continue; //scarto i commenti
		if(isspace(line[0])) continue;// le linee contenti solo spazzi
	     line[strlen(line)-1] = '\0'; //tolgo lo /n inserito da fgets
	    //tolgo tutti gli spazzi dalla linea letta
	     while(line[i] != '\0' ){
	     	if(isspace(line[i])){
	     		i++;
	     		continue;
	     	}
	     	tmp[j++] = line[i++];
	    }
	    //cerco le parole chiavi, il secondo token sarà il parametro per configurare il server. Capisco qual è in base al primo
	    token = strtok(tmp,"=");;
	    if(!strcmp(token,"UnixPath")){
	    	token = strtok(NULL,"=");
	    	parm->unixpath = (char*) malloc(sizeof(char)*(strlen(token)+1));
	    	if(!parm->unixpath){
	    		fclose(fp);
	    		return -1;
			} 
	    	strcpy(parm->unixpath,token);
		}
		else if(!strcmp(token,"ThreadsInPool")){
				token = strtok(NULL,"=");
				parm->threadsinpool = atoi(token);
			}
			else if(!strcmp(token,"MaxConnections")){
					token = strtok(NULL,"=");
					parm->maxconnections = atoi(token);
				}
				else if(!strcmp(token,"MaxMsgSize")){
						token = strtok(NULL,"=");
						parm->maxmessagesize = atoi(token);
						}
						else if(!strcmp(token,"MaxFileSize")){
							token = strtok(NULL,"=");
							parm->maxfilesize = atoi(token);

							}
						else if(!strcmp(token,"MaxHistMsgs")){
								token = strtok(NULL,"=");
								parm->maxhits = atoi(token);
							}
							else if(!strcmp(token,"DirName")){
								   token = strtok(NULL,"=");
								   parm->dirname = (char*) malloc(sizeof(char)*(strlen(token)+1));
								   if(!parm->dirname){
								   		fclose(fp);
								   		return -1;
								   } 
								   strcpy(parm->dirname,token);
								}
								else if(!strcmp(token,"StatFileName")){
									token = strtok(NULL,"=");
									parm->statfilename = (char*) malloc(sizeof(char)*(strlen(token)+1));
									if(!parm->statfilename){
										fclose(fp);
										return -1;
                                    } 
									strcpy(parm->statfilename,token);
									}

		memset(token,'\0',strlen(token)); //inizializzo per la prossima iterazione
	    memset(tmp,'\0',strlen(tmp));
	    memset(line,'\0',strlen(line));
	    i = 0;
	    j = 0;
	}
	fclose(fp);
	return 1;
}

void handler_sigusr2(int signum){
	flag = 1;
	return;
}

int updateMax(fd_set set,int fd_max){
	for(int i = fd_max; i>= 0 ; i--){
		if(FD_ISSET(i,&set)) return i;
	}

return -1;
}


int write_on_pipe(int fd,int* fd_c){
	if(fd < 0) return 0;
	pthread_mutex_lock(&mtx_pipe); //prendo la lock sulla pipe
	if(write(fd,fd_c,sizeof(int)) <= 0){ 
		perror("WRITING FD ON PIPE FOR THREAD MASTER");
		return 0;	
	}
	pthread_mutex_unlock(&mtx_pipe);//rilascio la lock sulla pipe
	return 1;
}



int send_ack(long fd,message_hdr_t*hdr){
	int l = 0;

	switch(hdr->op){
		case OP_FAIL:
		case OP_NICK_ALREADY:
    	case OP_NICK_UNKNOWN:
        case OP_MSG_TOOLONG:
        case OP_NO_SUCH_FILE:{
        	pthread_mutex_lock(&mtx_stats);
        	chattyStats.nerrors += 1;
        	pthread_mutex_unlock(&mtx_stats);
        }break;
        default: break;
    }
	//mando il codice che identifica l'esito dell'operazioneoperazione
	if((l = writen(fd,&(hdr->op),sizeof(int))) <= 0) return l; //write non a buon termine
	//fprintf(stderr, "esito scritto\n");
	//manda il sender
	if((l = writen(fd,hdr->sender,MAX_NAME_LENGTH+1)) <= 0) return l;
	return 1;
}

void print_history(circular_vector* h){
	int i;
	for(i = 0; i < (h->count_msg) ; i++) printf("msg %d [%s] \n",i,h->msgs[i].data.buf );
}


