

/*
 *  Título: Encuentra Primos
 *  Nombre: Héctor Paredes Benavides
 *  Descripción: Creamos un programa que encuentre primos mediante la paralelización de procesos y un algoritmo de fuerza bruta
 *  Fecha: 5/1/2022
 */

/* Instrucciones de Preprocesado */
// Includes del enunciado
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

// Includes adicionales
#include <string.h>
#include <math.h>

// Defines del enunciado
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

// RANGO Y BASE AUXILIARES PARA PRUEBAS
//#define BASE 3
//#define RANGO 1000

// Intervalo del temporizador para RAIZ
#define INTERVALO_TIMER 5

// Códigos de mensaje para el campo mesg_type del tipo T_MESG_BUFFER
#define COD_ESTOY_AQUI 5           // Un calculador indica al SERVER que está preparado
#define COD_LIMITES 4              // Mensaje del SERVER al calculador indicando los límites de operación
#define COD_RESULTADOS 6           // Localizado un primo
#define COD_FIN 7                  // Final del procesamiento de un calculador

/* Declaraciones Globales */
// Estructuras
// Mensaje que se intercambia
typedef struct {
    long mesg_type;
    char mesg_text[LONGITUD_MSG];
} T_MESG_BUFFER;

// Prototipado de funciones del enunciado
int Comprobarsiesprimo(long int numero);
void Informar(char *texto, int verboso);
void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos);
int ContarLineas();
static void alarmHandler(int signo);

// Prototipado de funciones adicionales
int transformaStringAEntero(char *cadenaCaracteres);
void mensajesHandler(int signal);
char *formateaPrimoEncontrado(char *cadenaCaracteres, int pidCalculador, char *cadenaCaracteres2, int primoEncontrado);
int vuelcaPrimoAFichero(long int numeroPrimo, FILE *fichero);
int vuelcaCantidadPrimos(long int cantidadPrimos);

// Variables Globales
int cuentasegs;                   // Variable para el cómputo del tiempo total
T_MESG_BUFFER message;
int msgid;
int inicioRango;
int finalRango;
FILE *ficheroNumerosPrimos;
int *pidhijos;
int *numerosParaCalculadores;
int pidraiz;
clock_t tInicioCalculo;
clock_t tFinalCalculo;
double tCalculo;

/* Función Principal Main */
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
    int pid, pidservidor, parentpid, mypid, pidcalc;
    int intervalo,inicuenta;
    int verbosity;
    char info[LONGITUD_MSG_ERR];
    FILE *fsal, *fc;
    int numhijos;

    int numerosAComprobar;
    int numerosPorCalculador;
    int msgErr;
    int pidCalculador;
    long int primoEncontrado;
    int numeroCalculadoresFinalizados;
    int numerosPrimosEncontrados;
    char *informes;
    FILE *ficheroCuentaPrimos;
    double tCalculoAuxiliar;

    // Control de entrada, después del nombre del script debe figurar el número de hijos y el parámetro verbosity
    if(argc <= 1){
        printf("ERROR (COD: %d): No se ha introducido el número de calculadores a crear como parámetro del programa, también se puede introducir -v después para ver información más de tallada.\n", ERR_ENTRADA_ERRONEA);
        exit(ERR_ENTRADA_ERRONEA);
    }

    verbosity = 0;

    if(argc > 2)
        if(strcmp(argv[2], "-v") == 0)
            verbosity = 1;

    for(int i = 0; i < strlen(argv[1]); i++){
        if(argv[1][i] < '0' || argv[1][i] > '9'){
            printf("ERROR (COD: %d): Se han introducido caracteres no numéricos en el parámetro del número de calculadores.", ERR_ENTRADA_ERRONEA);
            exit(ERR_ENTRADA_ERRONEA);
        }
    }

    if(signal(SIGINT, mensajesHandler) == SIG_ERR)
        printf("\nERROR: Ha ocurrido un error al intentar capturar la señal SIGINT.\n");

    numhijos = transformaStringAEntero(argv[1]);

    //numhijos = 2;     // SOLO para el esqueleto, en el proceso  definitivo vendrá por la entrada

    ficheroCuentaPrimos = fopen(NOMBRE_FICH_CUENTA, "w");
    msgErr = fprintf(ficheroCuentaPrimos, "%d", 0);
    fclose(ficheroCuentaPrimos);
    if(msgErr < 0)
        printf("ERROR: Ha ocurrido un error al limpiar el fichero %s, su valor podría no ser fiable hasta su próxima escritura.", NOMBRE_FICH_CUENTA);

    pidraiz = getpid(); // Recogemos el PID del proceso RAÍZ
    pid=fork();         // Creación del SERVER
    
    if (pid == 0)       // Rama del hijo de RAIZ (SERVER)
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

            if(signal(SIGUSR1, mensajesHandler) == SIG_ERR)
                printf("\nERROR (COD: %d): Error al capturar SIGUSR1\n", ERR_RECV);

            // Mandamos el código de listo al servidor
			message.mesg_type = COD_ESTOY_AQUI;
			sprintf(message.mesg_text,"%d",mypid);
			msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);

		
			// Un montón de código por escribir
            // Recogemos la asignación de rango por parte del servidor

			sleep(60); // Esto es solo para que el esqueleto no muera de inmediato, quitar en el definitivo

			//exit(0);
		}
		
		// SERVER
		
		else
		{

		    // Pide memoria dinámica para crear la lista de pids de los hijos CALCuladores
            pidhijos = (int*)malloc(sizeof(int) * numhijos);
            numerosParaCalculadores = (int*)malloc(sizeof(int) * numhijos);
		  
		    //Recepción de los mensajes COD_ESTOY_AQUI de los hijos
		    for (j=0; j <numhijos; j++)
		    {
			    msgrcv(msgid, &message, sizeof(message), 0, 0);
			    sscanf(message.mesg_text,"%d",&pid); // Tendrás que guardar esa pid
                pidhijos[j] = pid;
			    printf("\nMe ha enviado un mensaje el hijo %d\n",pid);
		    }
		  
			sleep(5); // Esto es solo para que el esqueleto no muera de inmediato, quitar en el definitivo

		  
		    // Mucho código con la lógica de negocio de SERVER
            // Mostramos la jerarquía de procesos creada
		    Imprimirjerarquiaproc(pidraiz, pidservidor, pidhijos, numhijos);

            // Le pasamos a cada calculador su rango de acción
            // Calculamos la cantidad de números que tienen que ser comprobados
            numerosAComprobar = RANGO + 1;
            numerosPorCalculador = numerosAComprobar / numhijos;
            
            for(int j = 0; j < numhijos; j++)
                numerosParaCalculadores[j] = numerosPorCalculador;

            for(int j = 0; j < (numerosAComprobar % numhijos); j++)
                numerosParaCalculadores[j] += 1;

            // Mandamos a cada calculador su rango para calcular
            inicioRango = BASE;
         
            tCalculo = 0;
            tCalculoAuxiliar = 0;
            tInicioCalculo = clock();

            for(int j = 0; j < numhijos; j++){

                //printf("Enviando rango para %d: %d - %d\n", pidhijos[j], inicioRango, inicioRango + numerosParaCalculadores[j]);

                // Preparamos el mensaje
                message.mesg_type = COD_LIMITES;
                sprintf(message.mesg_text, "%d %d", inicioRango, inicioRango + numerosParaCalculadores[j] - 1);
                msgErr = msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                if(msgErr == -1)
                    perror("Fallo al mandar mensaje de límites: ");
                //else
                    //printf("Mensaje enviado...\n");

                // Mandamos al proceso destino la señal de que le hemos mandado un mensaje
                kill(pidhijos[j], SIGUSR1);

                inicioRango += numerosParaCalculadores[j];

            }

            sleep(5);

            pidCalculador = 0;
            primoEncontrado = 0;
            numeroCalculadoresFinalizados = 0;
            numerosPrimosEncontrados = 0;
            informes = NULL;
            ficheroNumerosPrimos = fopen(NOMBRE_FICH, "w+");    // Abrimos el fichero para lectura y escritura sobrescribiendo lo que ya contiene
            
            do{

                msgrcv(msgid, &message, sizeof(message), 0, 0);
                switch(message.mesg_type){

                    case COD_RESULTADOS:
                        sscanf(message.mesg_text, "%d %d", &pidCalculador, &primoEncontrado);
                        numerosPrimosEncontrados++;
                        informes = formateaPrimoEncontrado("El Calculador ", pidCalculador, " ha encontrado el primo: ", primoEncontrado);
                        Informar(informes, verbosity);
                        free(informes);
                        if(vuelcaPrimoAFichero(primoEncontrado, ficheroNumerosPrimos))
                            printf("ERROR: Debido a un error, no se ha volcado el número primo %d al fichero %s.", primoEncontrado, NOMBRE_FICH);
                        if(numerosPrimosEncontrados % CADA_CUANTOS_ESCRIBO == 0)
                            vuelcaCantidadPrimos(numerosPrimosEncontrados);
                        break;

                    case COD_FIN:
                        sscanf(message.mesg_text, "%d %lf", &pidCalculador, &tCalculoAuxiliar);
                        numeroCalculadoresFinalizados++;
                        informes = formateaPrimoEncontrado("El Calculador ", pidCalculador, " ha finalizado su tarea, restantes: ", numhijos - numeroCalculadoresFinalizados);
                        Informar(informes, verbosity);
                        free(informes);
                        tCalculo += tCalculoAuxiliar;
                        tCalculoAuxiliar = 0;
                        break;

                    default:
                        printf("ERROR: El receptor de mensajes del server se ha encontrado con un mensaje que no ha sabido tratar.\n");
                        break;

                }

            }while(numeroCalculadoresFinalizados < numhijos);

            tFinalCalculo = clock();
            tCalculo += ((double)(tFinalCalculo - tInicioCalculo)) / CLOCKS_PER_SEC;

		    // Borrar la cola de mensajería, muy importante. No olvides cerrar los ficheros
            printf("INFO: Todos los Calculadores han finalizado su trabajo.\n");
            printf("INFO: Tiempo total de cálculo: %lf segundos.\n", tCalculo);
            //printf("SE CIERRA LA COLA DE MENSAJERÍA.\n");
		    msgctl(msgid,IPC_RMID,NULL);

            fclose(ficheroNumerosPrimos);

            free(pidhijos);
            free(numerosParaCalculadores);

            exit(0);
		  
	   }
    }

    // Rama de RAIZ, proceso primigenio
    
    else
    {
	  
        alarm(INTERVALO_TIMER);
        signal(SIGALRM, alarmHandler);

        wait(NULL);

        printf("INFO: El programa ha finalizado la tarea, número de líneas del fichero '%s' (números primos encontrados): %d\n", NOMBRE_FICH, ContarLineas());

    }
}

/* Codificación de Funciones */
// Función con la que comprobamos si un número pasado como argumento es primo o no, retorna 1 si es primo y 0 si no
int Comprobarsiesprimo(long int numero){

    int esPrimo = 1;

    for(int i = 2; esPrimo && i <= (numero / 2); i++)
        if(numero % i == 0)
            esPrimo = 0;

    return esPrimo;

}

// Función con la que mostramos que un nuevo primo se ha encontrado si el parámetro verbosity ha sido introdicido
void Informar(char *texto, int verboso){

    if(verboso){
        printf(texto);
        printf("\n");
    }

}

// Función que se encarga de imprimir la jerarquía de procesos
void Imprimirjerarquiaproc(int pidraiz, int pidservidor, int *pidhijos, int numhijos){

    printf("JERARQUÍA DE PROCESOS:\nRAÍZ\tSERVER\tCALCULADORES\n");

    for(int i = 0; i < numhijos; i++){
        if(i == 0)
            printf("%d\t%d\t%d\n", pidraiz, pidservidor, pidhijos[i]);
        else
            printf("\t\t%d\n", pidhijos[i]);
    }

}

// Manejador de la alarma en el RAIZ
static void alarmHandler(int signo)
{

    int nCuentaPrimos = 0;
    int errCode = 0;

    FILE *fichero = fopen(NOMBRE_FICH_CUENTA, "r");

    errCode = fscanf(fichero, "%d", &nCuentaPrimos);

    fclose(fichero);

    if(errCode <= 0)
        printf("ERROR: Ha ocurrido un error al intentar leer el fichero %s\n", NOMBRE_FICH_CUENTA);
    else
        printf("INFO: Valor del fichero '%s': %d\n", NOMBRE_FICH_CUENTA, nCuentaPrimos);

    alarm(INTERVALO_TIMER);

}

// Función con la que contamos las líneas del fichero que contiene los números primos encontrados
int ContarLineas(){

    int nLineasFichero = 0;

    FILE *fichero = fopen(NOMBRE_FICH, "r");
    char caracterAux = '\0';

    while(!feof(fichero)){

        caracterAux = fgetc(fichero);
        if(caracterAux == '\n')
            nLineasFichero++;

    }

    fclose(fichero);

    return nLineasFichero;

}

// Función con la que transformamos una cadena de caracteres en un entero
int transformaStringAEntero(char *cadenaCaracteres){

    // Variables en las que almacenaremos la longitud de la cadena pasada por argumento, el índice de la nueva cadena, el número resultante y la cadena inversa
    int longitudCadena = strlen(cadenaCaracteres) - 1;
    int indiceNuevaCadena = 0;
    int numeroEntero = 0;
    char *cadenaInvertida = (char*)malloc(sizeof(char) * (strlen(cadenaCaracteres) + 1));

    // Creamos la cadena invertida
    for(int i = longitudCadena; i >= 0; i--){

        cadenaInvertida[indiceNuevaCadena] = cadenaCaracteres[i];
        indiceNuevaCadena++;

    }

    cadenaInvertida[indiceNuevaCadena] = '\0';

    // Algoritmo en el que multiplicamos el valor por su posición decimal
    for(int i = 0; i <= longitudCadena; i++)
        numeroEntero += ((cadenaInvertida[i] - '0') * pow(10, i));

    return numeroEntero;

}

// Función manejadora de señales
void mensajesHandler(int signal){

    int msgErr;
    int esPrimo;
    int pidEmisor;

    //printf("Entra en mensajesHandler.\n");

    if(signal == SIGUSR1){  // Mensaje para calculadores

        msgErr = msgrcv(msgid, &message, sizeof(message), 0, 0);

        //printf("Mensaje recibido para %d.\n", getpid());

        if(msgErr == -1)
            perror("ERROR: Error al recibir el mensaje: ");
        else if(msgErr == 0)
            printf("ERROR: Solo se recibió el tipo de mensaje.\n");
        //else
            //printf("Se leeyeron %d bytes del mensaje.\n", msgErr);

        if(message.mesg_type == COD_LIMITES){

            sscanf(message.mesg_text, "%d %d", &inicioRango, &finalRango);

            //printf("Rango recibido para %d: %d - %d\n", getpid(), inicioRango, finalRango);

            sleep(5);

            tCalculo = 0;
            tInicioCalculo = clock();

            for(long int i = inicioRango; i <= finalRango; i++){

                esPrimo = Comprobarsiesprimo(i);
                if(esPrimo){

                    //printf("Primo encontrado: %d\n", i);

                    do{
                        message.mesg_type = COD_RESULTADOS;
                        sprintf(message.mesg_text, "%d %d", getpid(), i);
                        msgErr = msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                        if(msgErr == -1)
                            perror("ERROR: Ha ocurrido un error al enviar el mensaje de nuevo número primo al servidor:");
                    }while(msgErr == -1);

                    //printf("Mandamos desde el calculador %d al servidor %d el primo %d\n", getpid(), getppid(), i);

                }

            }

            tFinalCalculo = clock();
            tCalculo = ((double)(tFinalCalculo - tInicioCalculo)) / CLOCKS_PER_SEC;

            do{
                message.mesg_type = COD_FIN;
                sprintf(message.mesg_text, "%d %lf", getpid(), tCalculo);
                msgErr = msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                if(msgErr == -1)
                    perror("ERROR: Ha ocurrido un error al enviar el mensaje de finalización al servidor: ");
            }
            while(msgErr == -1);

            exit(0);

        }
        else if(signal == SIGINT){

            printf("\nFINALIZANDO EJECUCIÓN POR LA DETECCIÓN DE CTRL + C\n");
            // Borramos la cola de mensajería
            msgctl(msgid, IPC_RMID, NULL);

            fclose(ficheroNumerosPrimos);

            free(pidhijos);
            free(numerosParaCalculadores);

            if(pidraiz != getpid())
                kill(getppid(), SIGINT);
            else
                kill(getpid(), SIGKILL);

            exit(0);

        }
        else
            printf("ERROR: El proceso calculador %d recibió un mensaje para el que no estaba establecido un código de recepción.", getpid());

    }

}

char *formateaPrimoEncontrado(char *cadenaCaracteres, int pidCalculador, char *cadenaCaracteres2, int primoEncontrado){

    char *cadenaFinal = (char*)malloc(sizeof(char));

    char varAuxCalculador[LONGITUD_MSG];
    char varAuxPrimo[LONGITUD_MSG];

    int indiceCadenaFinal = 0;
    int indiceCadenaCaracteres = 0;
    int indiceCalculador = 0;
    int indiceCadenaCaracteres2 = 0;
    int indicePrimo = 0;

    sprintf(varAuxCalculador, "%d", pidCalculador);
    sprintf(varAuxPrimo, "%d", primoEncontrado);

    while(cadenaCaracteres[indiceCadenaCaracteres] != '\0'){

        cadenaFinal[indiceCadenaFinal] = cadenaCaracteres[indiceCadenaCaracteres];
        indiceCadenaFinal++;
        indiceCadenaCaracteres++;
        cadenaFinal = (char*)realloc(cadenaFinal, sizeof(char) * (indiceCadenaFinal + 1));

    }

    while(varAuxCalculador[indiceCalculador] != '\0'){

        cadenaFinal[indiceCadenaFinal] = varAuxCalculador[indiceCalculador];
        indiceCadenaFinal++;
        indiceCalculador++;
        cadenaFinal = (char*)realloc(cadenaFinal, sizeof(char) * (indiceCadenaFinal + 1));

    }

    while(cadenaCaracteres2[indiceCadenaCaracteres2] != '\0'){

        cadenaFinal[indiceCadenaFinal] = cadenaCaracteres2[indiceCadenaCaracteres2];
        indiceCadenaFinal++;
        indiceCadenaCaracteres2++;
        cadenaFinal = (char*)realloc(cadenaFinal, sizeof(char) * (indiceCadenaFinal + 1));

    }

    while(varAuxPrimo[indicePrimo] != '\0'){

        cadenaFinal[indiceCadenaFinal] = varAuxPrimo[indicePrimo];
        indiceCadenaFinal++;
        indicePrimo++;
        cadenaFinal = (char*)realloc(cadenaFinal, sizeof(char) * (indiceCadenaFinal + 1));

    }

    cadenaFinal[indiceCadenaFinal] = '\0';

    return cadenaFinal;

}

// Función con la que volcamos un número primo encontrado a un fichero, retorna 0 si todo ha ido bien y 1 si ha habido algún error
int vuelcaPrimoAFichero(long int numeroPrimo, FILE *fichero){

    int errCode = 0;
    int retorno = 0;

    errCode = fprintf(fichero, "%d\n", numeroPrimo);

    if(errCode < 0){
        printf("ERROR: Ha ocurrido un error al intentar volcar el primo %d al fichero %s.\n", numeroPrimo, NOMBRE_FICH);
        retorno = 1;
    }

    return retorno;

}

int vuelcaCantidadPrimos(long int cantidadPrimos){

    int errCode = 0;
    int retorno = 0;

    FILE *fichero = fopen(NOMBRE_FICH_CUENTA, "w");

    errCode = fprintf(fichero, "%d", cantidadPrimos);

    fclose(fichero);

    if(errCode < 0){
        printf("ERROR: Ha ocurrido un error al intentar volcar la cantidad de números primos (%d) al fichero %s.\n", cantidadPrimos, NOMBRE_FICH_CUENTA);
        retorno = 1;
    }

    return retorno;

}
