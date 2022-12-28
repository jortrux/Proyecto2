#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

#define LONGITUD_MSG 100           // Payload del mensaje
#define LONGITUD_MSG_ERR 200       // Mensajes de error por pantalla
// CÃ³digos de exit por error
#define ERR_ENTRADA_ERRONEA 2
#define ERR_SEND 3
#define ERR_RECV 4
#define ERR_FSAL 5

#define NOMBRE_FICH "primos.txt"
#define NOMBRE_FICH_CUENTA "cuentaprimos.txt"
#define CADA_CUANTOS_ESCRIBO 5
// rango de bÃºsqueda, desde BASE a BASE+RANGO
#define BASE 800000000
#define RANGO 2000
// Intervalo del temporizador para RAIZ
#define INTERVALO_TIMER 5
// CÃ³digos de mensaje para el campo mesg_type del tipo T_MESG_BUFFER
#define COD_ESTOY_AQUI 5           // Un calculador indica al SERVER que estÃ¡ preparado
#define COD_LIMITES 4              // Mensaje del SERVER al calculador indicando los lÃ­mites de operaciÃ³n
#define COD_RESULTADOS 6           // Localizado un primo
#define COD_FIN 7                  // Final del procesamiento de un calculador
// Mensaje que se intercambia
typedef struct {
    long mesg_type;
    char mesg_text[LONGITUD_MSG];
} T_MESG_BUFFER;


int Comprobarsiesprimo(long int numero);
void Informar(char *texto, int verboso);
void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos);
int ContarLineas();
static void alarmHandler(int signo);

int cuentasegs;                   // Variable para el cÃ³mputo del tiempo total

int main(int argc, char* argv[]){
	int i,j;
	long int numero;
	int contador;
	long int numprimrec;
    long int nbase;
    int nrango;
    int nfin;
    time_t tstart,tend;
	key_t key;
    int msgid;
    int pid, pidservidor, pidraiz, parentpid, mypid, pidcalc;
    int *pidhijos;
    int intervalo,inicuenta;
    int verbosity;
    T_MESG_BUFFER message;
    char info[LONGITUD_MSG_ERR];
    FILE *fsal, *fc;
    int numhijos;

    FILE *fd;//para cuando abra los ficheros correspondientes
    FILE *fd2;
    // Control de entrada, despuÃ©s del nombre del script debe figurar el nÃºmero de hijos y el parÃ¡metro verbosity

    if(argc !=3){
	printf("Error, hay mas elementos de los necesarios en la entrada\n");
    }else{
	    numhijos = atoi(argv[1]);     //doy gracias a la existencia de esta funcion, porque que pereza de hacer el parseo, por dioh
	    verbosity = atoi(argv[2]);
	    pid=fork();       // CreaciÃ³n del SERVER
	    if (pid == 0){     // Rama del hijo de RAIZ (SERVER)
			pid = getpid();
			pidservidor = pid;
			mypid = pidservidor;
			// PeticiÃ³n de clave para crear la cola
			if ( ( key = ftok( "/tmp", 'C' ) ) == -1 ) {
			  perror( "Fallo al pedir ftok" );
			  exit( 1 );
			}
			printf( "Server: System V IPC key = %u\n", key );
	        // CreaciÃ³n de la cola de mensajerÃ­a
			if ( ( msgid = msgget( key, IPC_CREAT | 0666 ) ) == -1 ) {
			  perror( "Fallo al crear la cola de mensajes" );
			  exit( 2 );
			}
			printf("Server: Message queue id = %u\n", msgid );
	        i = 0;
	        // CreaciÃ³n de los procesos CALCuladores
			while(i < numhijos) {
			 if (pid > 0) { // Solo SERVER crearÃ¡ hijos
				 pid=fork();
				 if (pid == 0)
				   {   // Rama hijo
					parentpid = getppid();
					mypid = getpid();
				   }
			}
			 i++;  // NÃºmero de hijos creados
			}

	        // AQUI VA LA LOGICA DE NEGOCIO DE CADA CALCulador.
			if (mypid != pidservidor){
				message.mesg_type = COD_ESTOY_AQUI;
				sprintf(message.mesg_text,"%d",mypid);
				msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);

				msgrcv(msgid, &message, sizeof(message), COD_LIMITES, 0);
				sscanf(message.mesg_text,"%ld %d",&nbase,&nrango);

				for(numero=nbase;numero<nbase+nrango;numero++){
					if(Comprobarsiesprimo(numero)==1){
						message.mesg_type=COD_RESULTADOS;
						sprintf(message.mesg_text,"%d %ld",mypid,numero);
						msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
					}
				}
				message.mesg_type = COD_FIN;
				sprintf(message.mesg_text,"%d",mypid);
				msgsnd(msgid,&message, sizeof(message), IPC_NOWAIT);
				exit(0);
			}
			// SERVER
			else{
			  tstart=time(NULL);
			  // Pide memoria dinÃ¡mica para crear la lista de pids de los hijos CALCuladores
			  pidhijos=(int*)malloc(numhijos*sizeof(int));
			  //RecepciÃ³n de los mensajes COD_ESTOY_AQUI de los hijos
			  for (j=0; j <numhijos; j++){
				  msgrcv(msgid, &message, sizeof(message), 0, 0);
				  sscanf(message.mesg_text,"%d",&pidhijos[j]); // TendrÃ¡s que guardar esa pid
				  printf("\nMe ha enviado un mensaje el hijo %d\n",pidhijos[j]);
			  }
			  Imprimirjerarquiaproc(pidraiz,pidservidor,pidhijos,numhijos);
				//saco el rango para despues de haber recibido todos los mensajes, indicar a cada calculador, de donde a donde buscar primos
			  nrango=RANGO/numhijos;
			  nbase=BASE;

			  //un bucle que recorre todos los hijos mandando un mensaje para su base y su fin, actualizando la base por cada vez que recorre el bucle y al cambiar la base tambien cambia el fin, a si que divide la busqueda en porciones para cada calculador
			  for(int i=0; i<numhijos;i++){
				nfin=nbase+nrango-1;
				message.mesg_type=COD_LIMITES;
				sprintf(message.mesg_text,"%ld %d",nbase,nfin);
				msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
				nbase=nbase+nrango;
			  }
			  //abro/creo el fichero primos.txt donde almacenare todos los primos que encuentre
			  fd=fopen(NOMBRE_FICH,"w");
		//como abro en escritura no hago comprobacion de error, ya que si no existe se crea
			  //por lo que he leido, cada vez que un hijo termina su rango, numhijos-- si no,lo guardamos en el fichero, y si la cantidad de primos es 5*n entonces guardamos esa cifra en otro fichero
			  while(numhijos!=0){
				msgrcv(msgid,&message,sizeof(message),0,0);//admito, que he pedido ayuda para esa funcion porque sigo sin tener claras las finciones de signal ni de shared memory
				if(message.mesg_type==COD_FIN){
					sscanf(message.mesg_text,"%d",&pidcalc);
					numhijos--;
				}else if(message.mesg_type==COD_RESULTADOS){
					sscanf(message.mesg_text,"%ld %d",&numprimrec,&pidcalc);
					fprintf(fd,"%ld\n",numprimrec);//leemos el valor del primo, y lo metemos en el fichero
					//sprintf(fd,"MSG %d | %ld PID %d\n",contMSG,numprimrec,pidcalc);
					contador++;
				}
				if(contador%5==0){
					fd2=fopen(NOMBRE_FICH_CUENTA,"w+");
					fprintf(fd2,"%d\n",contador);//si el contador es multiplo de cinco pal otro fichero que va
					fclose(fd2);
				}
			  }
			  fclose(fd);
			  // Borrar la cola de mensajerÃ­a, muy importante. No olvides cerrar los ficheros
			  msgctl(msgid,IPC_RMID,NULL);
			  exit(1);
		   }
    }

    // Rama de RAIZ, proceso primigenio
    else{
      cuentasegs=0;
      alarm(INTERVALO_TIMER);
      signal(SIGALRM, alarmHandler);
      wait(NULL);
	if(ContarLineas()!=-1){
      	    printf("RESULTADO: %d primos detectados\n",ContarLineas());
        }
    }
    free(pidhijos);
}
}
// Manejador de la alarma en el RAIZ
static void alarmHandler(int signo){
	FILE *fc;
	int nprim;
	cuentasegs=cuentasegs+INTERVALO_TIMER;
	if((fc=fopen(NOMBRE_FICH_CUENTA,"r"))!=NULL){
		fscanf(fc,"%d",&nprim);
		fclose(fc);
		printf("%02d (segs): %d primos encontrados\n",cuentasegs,nprim);
	}else{
		printf("%02d (segs)\n",cuentasegs);
	}
	alarm(INTERVALO_TIMER);
}

int Comprobarsiesprimo(long int numero){
	int contador=0;
	int esPrimo=0;//0 si no es primo
	for(int i=1; i<=numero; i++){
		if(numero%i==0){
			contador++;
		}
	}
	if(contador==2){
		esPrimo=1;//1 si es primo
	}
	return esPrimo;
}

void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos){
	printf("Raiz\tServ\tCalc\n");
	for(int i=0;i<numhijos;i++){
		if(i==0){
			printf("%i\t%i\t%i\n",pidraiz,pidservidor,pidhijos[i]);
		}else{
			printf("\t\t%i\n",pidhijos[i]);
		}
	}
}


int ContarLineas(){
	int c=0;
	int primo=0;
	FILE *fd = fopen(NOMBRE_FICH,"r");
	if(fd==NULL){//ahora si que hago prueba de error porque es lecura
		printf("error, el fichero no existe\n");
		return -1;
	}else{
		while(!feof(fd)){
			c++;
			fscanf(fd,"%i",&primo);
		}
		fclose(fd);
		return c;
	}
}
void informar(char *texto, int verboso){
}
