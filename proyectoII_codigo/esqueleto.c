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

int main(int argc, char* argv[])
{
	int i,j;
	long int numero;
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

    // Control de entrada, después del nombre del script debe figurar el número de hijos y el parámetro verbosity

    numhijos = 2;     // SOLO para el esqueleto, en el proceso  definitivo vendrá por la entrada

    pid=fork();       // Creación del SERVER
    
    if (pid == 0)     // Rama del hijo de RAIZ (SERVER)
    {
		pid = getpid();
		pidservidor = pid;
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
		while(i < numhijos) {
		 if (pid > 0) { // Solo SERVER creará hijos
			 pid=fork(); 
			 if (pid == 0) 
			   {   // Rama hijo
				parentpid = getppid();
				mypid = getpid();
			   }
			 }
		 i++;  // Número de hijos creados
		}

        // AQUI VA LA LOGICA DE NEGOCIO DE CADA CALCulador. 
		if (mypid != pidservidor)
		{

			message.mesg_type = COD_ESTOY_AQUI;
			sprintf(message.mesg_text,"%d",mypid);
			msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
		
			// Un montón de código por escribir
			sleep(60); // Esto es solo para que el esqueleto no muera de inmediato, quitar en el definitivo

			exit(0);
		}
		
		// SERVER
		
		else
		{ 
		  // Pide memoria dinámica para crear la lista de pids de los hijos CALCuladores
		  
		  
		  //Recepción de los mensajes COD_ESTOY_AQUI de los hijos
		  for (j=0; j <numhijos; j++)
		  {
			  msgrcv(msgid, &message, sizeof(message), 0, 0);
			  sscanf(message.mesg_text,"%d",&pid); // Tendrás que guardar esa pid
              
		  }
		  
			sleep(60); // Esto es solo para que el esqueleto no muera de inmediato, quitar en el definitivo

		  
		  // Mucho código con la lógica de negocio de SERVER
		  
		  // Borrar la cola de mensajería, muy importante. No olvides cerrar los ficheros
		  msgctl(msgid,IPC_RMID,NULL);
		  
	   }
    }

    // Rama de RAIZ, proceso primigenio
    
    else
    {
	  
      alarm(INTERVALO_TIMER);
      signal(SIGALRM, alarmHandler);
      for (;;)    // Solo para el esqueleto
		sleep(1); // Solo para el esqueleto
	  // Espera del final de SERVER
      // ...
      // El final de todo
    }
}

// Manejador de la alarma en el RAIZ
static void alarmHandler(int signo)
{
//...
    printf("SOLO PARA EL ESQUELETO... Han pasado 5 segundos\n");
    alarm(INTERVALO_TIMER);

}
