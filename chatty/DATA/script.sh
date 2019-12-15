# membox Progetto del corso di LSO 2017/2018

# Dipartimento di Informatica Università di Pisa
# Docenti: Prencipe, Torquati
 
# Studente: Carlo Leo
# Matricola: 546155
 
# Si dichiara che il contenuto di questo file è in ogni sua parte 
# opera originale dell'autore

#!/bin/bash

function usage {
	echo "il comando va lanciato -----> $0 file_di_configurazione tempo" 1>&2 #ridirigo l'output su stderr
	echo "istruzioni per l'uso specificare opzione -help" 1>&2
}

if [[ $# -lt 2 || $# -ge 3  ]]; then #caso in cui lo script venga lanciato senza aromenti o con un numero minore di 2, e caso con un numero maggiore uguale di 3
    $(usage)
	exit 1
fi
#comando lanciato con 2 argomenti
#verifico che il primo argomento sia un file
if [ ! -f $1 ]; then
	if [ "$1" = "-help" ]; then  #caso in cui il primo sia -help
		$(usage)
	else
		echo "$1 non è un file" 1>&2
	fi
	exit 1
fi

if [ "$2" = "-help" ]; then
	$(usage)                  #caso in cui il secondo sia -help
	exit 1
fi

#controllo che il secondo parametro sia un tempo valido
if [  $2 -lt 0 ]; then
	echo "$2 non è un tempo valido" 1>&2
	exit 1
fi

TarName=chatty.tar
Dname=$(grep -v '^#' $1 |grep DirName |cut -d= -f2 | tr -d '\n' | tr -d ' ' ) #trovo il valore indicato da DirName nel file di configurazione



if [ $2 -eq 0 ]; then ls $Dname # mostro il contenuto della cartella

	else
		 tar -cf $TarName --files-from /dev/null #creo archivio vuoto
		 find  $Dname -type f -mmin $((-$2)) ! -path $Dname  -exec tar -rf $TarName  {} + #aggiungo tutti i file prenseti in Dname al tar
		 echo "**********File presenti in $TarName e rimossi in $Dname*********"
		 tar -tf $TarName | cut -d'/' -f3
		 cd $Dname #mi sposto nella cartella
		 rm  $(find $Dname -type f -mmin -$2) #elimino tutti i file aggiunti al tar
	fi 

exit 0