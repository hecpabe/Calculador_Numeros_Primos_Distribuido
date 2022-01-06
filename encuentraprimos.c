

/*
    Título: Encuentra Primos
    Nombre: Héctor Paredes Benavides
    Descripción: Creamos un programa que calcule los números primos de un rango dado mediante la paralelización de la tarea en
                diferentes instancias de un mismo proceso (este programa)
    Fecha: 5/1/2022
*/

/* Instrucciones de Preprocesado */
// Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string.h>
#include <math.h>

// Defines
#define LONGITUD_MSG 100           // Payload del mensaje
#define LONGITUD_MSG_ERR 200       // Mensajes de error por pantalla

// Códigos de exit por error
#define ERR_ENTRADA_ERRONEA 2
#define ERR_SEND 3
#define ERR_RECV 4
#define ERR_FSAL 5
#define ERR_FORK 6
#define ERR_TOKEN 7
#define ERR_MSGQUEUE_CREATION 8

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

#define MIN_CALCULADORES 1
#define MAX_CALCULADORES 10

/* Declaraciones Globales */
// Estructuras
typedef struct {
    long mesg_type;
    char mesg_text[LONGITUD_MSG];
} T_MESG_BUFFER;

typedef struct ProcesoCalculador_t{

    int pid;
    int codigoEstado;

}ProcesoCalculador_t;

// Prototipado de Funciones
int Comprobarsiesprimo(long int numero);
void Informar(char *texto, int verboso);
void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos);
int ContarLineas();
static void alarmHandler(int signo);

int transformaStringAEntero(char *cadenaCaracteres);

// Variables Globales
int cuentasegs;

/* Función Principal Main */
int main(int argc, char **argv){

    int parametroVerbosity = 0;
    int numeroCalculadores = 0;

    int pidRaiz;
    int pidServidor;
    int *pidCalculador;

    // Comprobamos que por lo menos haya introducido el número de calculadores por parámetro del programa
    if(argc <= 1){
        printf("\nERROR (COD: %d): Tiene que introducir como parámetro el número de calculadores a crear, y opcionalmente -v detrás para mostrar la salida de datos secundarios.", ERR_ENTRADA_ERRONEA);
        exit(ERR_ENTRADA_ERRONEA);
    }
    
    // Comprobamos que en el parámetro del número de calculadores no haya caracteres no numéricos
    for(int i = 0; i < strlen(argv[1]); i++){
        if(argv[1][i] < '0' || argv[1][i] > '9'){
            printf("\nERROR (COD: %d): Se han introducido caracteres no numéricos como parámetro de número de calculadores.", ERR_ENTRADA_ERRONEA);
            exit(ERR_ENTRADA_ERRONEA);
        }
    }

    // Transformamos el número de calculadores introducido como parámetro a entero y lo almacenamos en su variable
    numeroCalculadores = transformaStringAEntero(argv[1]);

    // Comprobamos la presencia del parámetro verbosity
    if(argc > 2)
        if(strcmp(argv[2], "-v") == 0)
            parametroVerbosity = 1;

    pidRaiz = getpid();
    pidServidor = fork();

    if(pidServidor < 0){    // ERROR al hacer fork
        printf("\nERROR (COD: %d): Ha ocurrido un error al crear el proceso servidor.", ERR_FORK);
        exit(ERR_FORK);
    }
    else if(pidServidor == 0){  // Proceso Hijo (SERVER)

        key_t token;
        int msgQueueID = 0;

        pidServidor = getpid();

        // Creamos el token para crear la cola de mensajería
        if((token = ftok("/tmp", 'C')) == -1){
            printf("\nERROR (COD: %d): Ha ocurrido un error al generar el token de la cola de mensajería.", ERR_TOKEN);
            exit(ERR_TOKEN);
        }

        printf("\nSERVER: System V IPC token: %u", token);

        // Creamos la cola de mensajería
        if((msgQueueID = msgget(token, IPC_CREAT | 0666)) == -1){
            printf("\nERROR (COD: %d): Ha ocurrido un error al generar la cola de mensajería System V", ERR_MSGQUEUE_CREATION);
            exit(ERR_MSGQUEUE_CREATION);
        }

    }
    else{   // Proceso Padre (RAÍZ)



    }

    return 0;

}

/* Codificación de Funciones */
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
    for(int i = 0; i <= longitudCadena; i++){

        numeroEntero += ((cadenaInvertida[i] - '0') * pow(10, i));

    }

    return numeroEntero;

}
