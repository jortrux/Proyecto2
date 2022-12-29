//JORGE GARRIDO 2ºINSO B
#include <stdio.h>
#include <string.h>
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

// Códigos de exit por error
#define ERR_ENTRADA_ERRONEA 2
#define ERR_SEND 3
#define ERR_RECV 4
#define ERR_FSAL 5

#define NOMBRE_FICH "primos.txt"
#define NOMBRE_FICH_CUENTA "cuentaprimos.txt"
#define CADA_CUANTOS_ESCRIBO 5

// rango de búsqueda, desde BASE a BASE+RANGO
#define BASE 800000000
#define RANGO 2000

// Intervalo del temporizador para RAIZ
#define INTERVALO_TIMER 5

// Códigos de mensaje para el campo mesg_type del tipo T_MESG_BUFFER
#define COD_ESTOY_AQUI 5           // Un calculador indica al SERVER que está preparado
#define COD_LIMITES 4              // Mensaje del SERVER al calculador indicando los límites de operación
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


int cuentasegs;                   // Variable para el cómputo del tiempo total


//pido perdon por mis comentarios, a veces me vuelvo loco y hablo con migo mismo, y acabo a insultos, y se me olvida borrar algunos, que son ya las 4 de la mañana, esto es lo que pasa por ponerme una entrega en navidades _|(-.-)|_
int main(int argc, char* argv[]){
        int i,j;
        long int numero;
        long int numprimrec;
        long int nbase;
        int nrango;
        int nfin;
        time_t tstart,tend;
        char miscamusca[50];//perdon, son las 5am solo quiero dormir, bueno no
        key_t key;
        int msgid;
        int pid, pidservidor, pidraiz, parentpid, mypid, pidcalc;
        int *pidhijos;
        int intervalo,inicuenta;
        int verbosity;
        T_MESG_BUFFER message;
        char info[LONGITUD_MSG_ERR];
        int numhijos;
        int contador=0;//Vatiable para contar el numero de mensage que se envia a Informar
        // Control de entrada, después del nombre del script debe figurar el número de hijos y el parámetro verbosity
        FILE *fd;//esto es una miquey herramienta que se usara mas tarde

        if(argc!=3){
        printf("Error, hay mas elementos de los necesarios en la entrada\n");
        }else{
                numhijos=atoi(argv[1]);
                verbosity=atoi(argv[2]);    

                //Guardamos el pid del padre asi ya lo conoce el hijo server
                pidraiz=getpid();

                pid=fork();       // Creación del SERVER
                pidservidor=pid;


                if (pid == 0){
                        pid = getpid(); //doy gracias a la existencia de esta funcion, porque que pereza de hacer el casteo
                        pidservidor=pid;        
                        mypid = pidservidor;       

                        // Petición de clave para crear la cola
                        if ( ( key = ftok( "/tmp", 'C' ) ) == -1 ) {
                          perror( "Fallo al pedir ftok" );
                          exit( 1 );
                        }
                        printf( "Server: System V IPC key = %u\n", key );
                                // Creación de la cola de mensajería
                        if ( ( msgid = msgget( key, IPC_CREAT | 0666 ) ) == -1 ) {
                          perror( "Fallo al crear la cola de mensajes" );
                          exit( 2 );
                        }
                        printf("Server: Message queue id = %u\n", msgid );
                                i = 0;
                                // Creación de los procesos CALCuladores
                        while(i < numhijos){
                         if (pid > 0) { // Solo SERVER creará hijos
                                 pid=fork(); 
                                 if (pid == 0){   // Rama hijo
                                        parentpid = getppid();
                                        mypid = getpid();
                                 }
                         }
                         i++;  // Número de hijos creados
                        }

                                        // AQUI VA LA LOGICA DE NEGOCIO DE CADA CALCulador.
                        if (mypid != pidservidor){//MUCHISIMAS GRACIOS CARLOS ERES EL MEJOR, DE VERDAD QUE ESTO ME MATA
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
                                // Pide memoria dinámica para crear la lista de pids de los hijos CALCuladores
                                pidhijos=(int*)malloc(numhijos*sizeof(int));
                                //Recepción de los mensajes COD_ESTOY_AQUI de los hijos
                                for (j=0; j <numhijos; j++){
                                        msgrcv(msgid, &message, sizeof(message), 0, 0);
                                        sscanf(message.mesg_text,"%d",&pidhijos[j]); // Tendrás que guardar esa pid
                                        printf("\nMe ha enviado un mensaje el hijo %d\n",pidhijos[j]);
                                }
                                Imprimirjerarquiaproc(pidraiz,pidservidor,pidhijos,numhijos);

                                nrango=RANGO/numhijos;
                                nbase=BASE;

                                //un bucle que recorre todos los hijos mandando un mensaje para su base y su fin, actualizando la base por cada vez que recorre el bucle y al cambiar la base tambien cambia el fin, a si que divide la busqueda en porciones para cada calculador
                                for(int indice=0;indice<numhijos;indice++){
                                        nfin=nbase+nrango-1;//tenia que haber leido el struct antes, que soy gilipollas
                                        message.mesg_type = COD_LIMITES;
                                        sprintf(message.mesg_text,"%ld %d",nbase,nfin);//empiezo a pillar la funcion esta  JODER
                                        msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
                                        nbase=nbase+nrango;

                                }
                                //abro/creo el fichero primos.txt donde almacenare todos los primos que encuentre
                                //abro/creo el fichero primos.txt donde almacenare todos los primos que encuentre


                                fd=fopen(NOMBRE_FICH,"w");
                                //como abro en escritura no hago comprobacion de error, ya que si no existe se cre
                                //por lo que he leido, cada vez que un hijo termina su rango, numhijos-- si no,lo guardamos en el fichero, y si la cantidad de primos es 5*n entonces guardamos esa cifra en otro fichero
                                while(numhijos!=0){
                                        msgrcv(msgid, &message, sizeof(message), 0, 0);
                                        if(message.mesg_type==COD_FIN){
                                                numhijos--;
                                                sscanf(message.mesg_text,"%d",&pidcalc);
                                                printf("Samorio el hijo calculador nº %d  quedan: %d\n",pidcalc,numhijos);
                                        }else if(message.mesg_type==COD_RESULTADOS){
                                                sscanf(message.mesg_text,"%ld %d",&numprimrec ,&pidcalc); // Tendrás que guardar esa pid

                                                fprintf(fd,"%ld\n",numprimrec);
                                                sprintf(miscamusca,"MSG %d  %ld pid %d\n",contador,numprimrec,pidcalc);
                                                Informar(miscamusca,verbosity);
                                                contador++;
                                        }
                                        if((contador%5)==0){//me gustaba mas la version que tenia sin abrir y cerrar continuamente T.T pero no me printava la cantidad que iba encontrando porque tu otro regalo estaba hecho leyendo del fichero
                                                FILE *fd2=fopen(NOMBRE_FICH_CUENTA,"w+");//fua, que original soy con los nombres, increible
                                                fprintf(fd2,"%d\n",contador); 
                                                fclose(fd2);
                                        }
                                }
                                fclose(fd);
                                tend=time(NULL);
                                printf("TIEMPO TOTAL: %f\n",difftime(tend,tstart));

                                FILE *fd2=fopen(NOMBRE_FICH_CUENTA,"w+");//FUAAAA, lo tenia que haber llamado fdos JAJAJAJAJ
                                fprintf(fd2,"%d\n",contador); 
                                fclose(fd2);
                                // Borrar la cola de mensajería, muy importante. No olvides cerrar los ficheros
                                msgctl(msgid,IPC_RMID,NULL);
                                exit(1);//casi nunca uso el exit, no sabes la de problemas que me ha dado esto
                        }
                }
                // Rama de RAIZ, proceso primigenio
                else{
                        //ENcviamos la señal de alarma y su controlador
                        alarm(INTERVALO_TIMER);
                        signal(SIGALRM, alarmHandler);
                        while(pidservidor!=wait(NULL)){//esto me lo han dicho, yo lo iba a dejar como en el esqueleto, esta en proceso de estudio, porfavor no te enfades, no doy a basto
                        }

                        //LLamamos a la funcion para contar las lineas
                        int contador=ContarLineas();
                        printf("HAY %d PRIMOS\n\n\n",contador);

                }
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

int ContarLineas(){//diooosss odio tabular en nano AAAAAAAAAAAA
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

void Informar(char *texto, int verboso){//de verdad que lo del verbosity me estaba rallando muchisimo, para esta tonteria
        if(verboso==1){
                printf("%s",texto);
        }


}

