

/*
 *  Título: Encuentra Primos
 *  Nombre: Héctor Paredes Benavides
 *  Descripción: Creamos un programa que encuentre primos mediante la paralelización de procesos y un algoritmo de fuerza bruta
 *  Fecha: 5/1/2022
 */

/* Instrucciones de Preprocesado */
// Inclusión de Bibliotecas del enunciado
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

// Inclusión de Bibliotecas adicionales
#include <string.h>
#include <math.h>

// Declaraciones Constantes del enunciado
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
int Comprobarsiesprimo(long int numero);                                                // Función con la que comprobamos si un número es primo
void Informar(char *texto, int verboso);                                                // Función con la que lanzamos mensajes por consola
void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos);   // Función con la que mostramos la jerarquía de procesos
int ContarLineas();                                                                     // Función con la que contamos las líneas del fichero de primos
static void alarmHandler(int signo);                                                    // Manejador de alarmas

// Prototipado de funciones adicionales
int transformaStringAEntero(char *cadenaCaracteres);                                                                    // Función con la que pasamos un string numérico a entero
void mensajesHandler(int signal);                                                                                       // Manejador de señales
char *formateaPrimoEncontrado(char *cadenaCaracteres, int pidCalculador, char *cadenaCaracteres2, int primoEncontrado); // Función con la que formateamos los mensajes para la función Informa()
int vuelcaPrimoAFichero(long int numeroPrimo, FILE *fichero);                                                           // Función con la que volcamos un primo encontrado al fichero
int vuelcaCantidadPrimos(long int cantidadPrimos);                                                                      // Función con la que volcamos la cantidad de primos al fichero

// Variables Globales
T_MESG_BUFFER message;          // Variable de mensaje que vamos a intercambiar
FILE *ficheroNumerosPrimos;     // Puntero a ficheros que apunta al fichero en el que almacenaremos los números primos encontraodos
clock_t tInicioCalculo;         // Instante en el que iniciamos el cálculo de números primos
clock_t tFinalCalculo;          // Instante en el que finalizamos el cálculo de números primos
double tCalculo;                // Tiempo que se ha estado calculando números primos
int msgid;                      // Identificador de la cola de mensajería
int inicioRango;                // Variable con la que indicamos el inicio de rango de números a calcular para los calculadores
int finalRango;                 // Variable con la que indicamos el final del rango de números a calcular para los calculadores
int *pidhijos;                  // Puntero a enteros en el que almacenaremos los PID de los calculadores
int *numerosParaCalculadores;   // Vector de enteros con el que le pasaremos a los calculadores el rango que le corresponde a cada uno
int pidraiz;                    // PID del proceso RAÍZ

/* Función Principal Main */
int main(int argc, char* argv[])
{
    // Variables para bucles
	int i,j;
	
    // Identificadores
	key_t key;                                          // Clave para crear la cola de mensajería
    int pid, pidservidor, parentpid, mypid, pidcalc;    // Diferentes PID para los procesos
    int pidCalculador;                                  // Variable en la que recogemos el PID del calculador que nos manda un mensaje

    // Parámetros
    int verbosity;  // Parámetro de información, si se le pasa al programa muestra más información
    int numhijos;   // Cantidad de calculadores que se crean

    // Números y primos
    int numerosAComprobar;              // Cantidad total de números que hay que comprobar si son primos o no (RANGO + 1)
    int numerosPorCalculador;           // Cantidad total de números que le corresponden calcular a cada calculador
    long int primoEncontrado;           // Variable en la que recogemos un nuevo número primo encontrado
    int numeroCalculadoresFinalizados;  // Cantidad de calculadores que han finalizado su trabajo
    int numerosPrimosEncontrados;       // Cantidad de números primos que se han encontrado en total

    // Otras variables
    int msgErr;                 // Código de error que recogemos en el intercambio de mensajes
    char *informes;             // Cadena de caracteres en la que introducimos informes
    FILE *ficheroCuentaPrimos;  // Puntero a fichero que apunta al fichero que lleva la cuenta (de 5 en 5) de los primos encontrados
    double tCalculoAuxiliar;    // Variable en la que recogemos el tiempo que ha tardado un calculador en calcular sus números

    // Control de entrada, después del nombre del script debe figurar el número de hijos y el parámetro verbosity
    if(argc <= 1){
        printf("ERROR (COD: %d): No se ha introducido el número de calculadores a crear como parámetro del programa, también se puede introducir -v después para ver información más de tallada.\n", ERR_ENTRADA_ERRONEA);
        exit(ERR_ENTRADA_ERRONEA);
    }

    verbosity = 0;

    // Comprobamos si se introduce el parámetro verbosity
    if(argc > 2)
        if(strcmp(argv[2], "-v") == 0)
            verbosity = 1;

    // Comprobamos que se haya introducido de forma correcta el parámetro de la cantidad de calculadores a crear
    for(int i = 0; i < strlen(argv[1]); i++){
        if(argv[1][i] < '0' || argv[1][i] > '9'){
            printf("ERROR (COD: %d): Se han introducido caracteres no numéricos en el parámetro del número de calculadores.", ERR_ENTRADA_ERRONEA);
            exit(ERR_ENTRADA_ERRONEA);
        }
    }

    // Asignamos el manejador para la señal CTRL + C (esto es para asegurarnos de que se cierra la cola de mensajería se el usuario manda esta señal)
    if(signal(SIGINT, mensajesHandler) == SIG_ERR)
        printf("\nERROR: Ha ocurrido un error al intentar capturar la señal SIGINT.\n");

    // Recogemos el número de calculadores a crear
    numhijos = transformaStringAEntero(argv[1]);

    // Limpiamos el fichero que lleva la cuenta de cuantos números primos se han encontrado
    ficheroCuentaPrimos = fopen(NOMBRE_FICH_CUENTA, "w");
    msgErr = fprintf(ficheroCuentaPrimos, "%d", 0);
    fclose(ficheroCuentaPrimos);
    if(msgErr < 0)
        printf("ERROR: Ha ocurrido un error al limpiar el fichero %s, su valor podría no ser fiable hasta su próxima escritura.", NOMBRE_FICH_CUENTA);

    pidraiz = getpid(); // Recogemos el PID del proceso RAÍZ
    pid=fork();         // Creación del SERVER
    
    if (pid == 0)       // Rama del hijo de RAIZ (SERVER)
    {
        // Recogemos los pid
		pid = getpid();
		pidservidor = pid;
		mypid = pidservidor;	   
		
		// Petición de clave para crear la cola
		if ( ( key = ftok( "/tmp", 'C' ) ) == -1 ) {
		  perror( "Fallo al pedir ftok" );
		  exit( 1 );
		}
		
        // Mostramos la clave para la cola
		printf( "Server: System V IPC key = %u\n", key );

        // Creación de la cola de mensajería
		if ( ( msgid = msgget( key, IPC_CREAT | 0666 ) ) == -1 ) {
		  perror( "Fallo al crear la cola de mensajes" );
		  exit( 2 );
		}

        // Mostramos la cola creada
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

        // Procesos hijos de SERVIDOR (CALCULADORES)
		if (mypid != pidservidor)
		{

            // Asignamos el manejador para la señal SIGUSR1
            if(signal(SIGUSR1, mensajesHandler) == SIG_ERR)
                printf("\nERROR (COD: %d): Error al capturar SIGUSR1\n", ERR_RECV);

            // Mandamos el código de listo al servidor
			message.mesg_type = COD_ESTOY_AQUI;
			sprintf(message.mesg_text,"%d",mypid);
			msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);

		}
		
		// SERVER
		else
		{

		    // Pedimos memoria dinámica para los hijos
            pidhijos = (int*)malloc(sizeof(int) * numhijos);
            numerosParaCalculadores = (int*)malloc(sizeof(int) * numhijos);
		  
		    //Recepción de los mensajes COD_ESTOY_AQUI de los hijos
		    for (j=0; j <numhijos; j++)
		    {
			    msgrcv(msgid, &message, sizeof(message), 0, 0);
			    sscanf(message.mesg_text,"%d",&pid); 
                pidhijos[j] = pid;
			    printf("\nMe ha enviado un mensaje el hijo %d\n",pid);
		    }
		  
            // Mostramos la jerarquía de procesos creada
		    Imprimirjerarquiaproc(pidraiz, pidservidor, pidhijos, numhijos);

            // Le pasamos a cada calculador su rango de acción
            // Calculamos la cantidad de números que tienen que ser comprobados
            numerosAComprobar = RANGO + 1;
            numerosPorCalculador = numerosAComprobar / numhijos;
            
            // Asignamos a cada calculador que tiene que calcular = (números a calcular) / (número de calculadores)
            for(int j = 0; j < numhijos; j++)
                numerosParaCalculadores[j] = numerosPorCalculador;

            // Si son impares, asignamos 1 más por cada unidad del resto que sobre a los n primeros
            for(int j = 0; j < (numerosAComprobar % numhijos); j++)
                numerosParaCalculadores[j] += 1;

            // Inicio del rango que se le va a pasar a los calculadores
            inicioRango = BASE;
         
            // Iniciamos el contador
            tCalculo = 0;
            tCalculoAuxiliar = 0;
            tInicioCalculo = clock();

            // Le pasamos a cada hijo el rango que debe calcular
            for(int j = 0; j < numhijos; j++){

                // Preparamos el mensaje y lo enviamos
                message.mesg_type = COD_LIMITES;
                sprintf(message.mesg_text, "%d %d", inicioRango, inicioRango + numerosParaCalculadores[j] - 1);
                msgErr = msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                if(msgErr == -1)
                    perror("Fallo al mandar mensaje de límites: ");

                // Mandamos una señal al calculador para indicarle de que ya le hemos pasado su rango y que lo lea
                kill(pidhijos[j], SIGUSR1);

                // Saltamos al siguiente inicio de rango
                inicioRango += numerosParaCalculadores[j];

            }

            // Espermaos para asegurarnos que todos los calculadores hayan leído su rango
            sleep(5);

            // Inicializamos las variables que vamos a usar
            pidCalculador = 0;
            primoEncontrado = 0;
            numeroCalculadoresFinalizados = 0;
            numerosPrimosEncontrados = 0;
            informes = NULL;

            // Abrimos el fichero para lectura y escritura sobrescribiendo lo que ya contiene
            ficheroNumerosPrimos = fopen(NOMBRE_FICH, "w+");
            
            // No paramos de comprobar si recibimos mensajes hasta que redibamos todos los mensajes de finalización de los calculadores
            do{

                // Recibimos el siguiente mensaje en la cola
                msgrcv(msgid, &message, sizeof(message), 0, 0);
                switch(message.mesg_type){

                    // Si es del hallazgo de un nuevo primo
                    case COD_RESULTADOS:
                        /*
                            Recogemos los datos, aumentamos el contador de primos encontrados, mandamos el informe de que se ha encontrado
                            un nuevo primo, volcamos el primo encontrado al fichero correspondiente y en caso de que la cantidad de primos
                            encontrados sea múltiplo de 5 lo volcamos en el fichero de cuenta de primos también
                        */
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

                    // Si es la finalización de un calculador
                    case COD_FIN:
                        /*
                            Recogemos los datos (PID del calculador que ha finalizado y el tiempo que ha tardado en calcular su rango)
                            incrementamos la cantidad de calculadores que han finalizado, mandamos el informe de que ha finalizado y
                            sumamos el tiempo que ha tardado en calcular su rango al tiempo general
                        */
                        sscanf(message.mesg_text, "%d %lf", &pidCalculador, &tCalculoAuxiliar);
                        numeroCalculadoresFinalizados++;
                        informes = formateaPrimoEncontrado("El Calculador ", pidCalculador, " ha finalizado su tarea, restantes: ", numhijos - numeroCalculadoresFinalizados);
                        Informar(informes, verbosity);
                        free(informes);
                        tCalculo += tCalculoAuxiliar;
                        tCalculoAuxiliar = 0;
                        break;

                    // Si es otro tipo de mensaje mostramos error
                    default:
                        printf("ERROR: El receptor de mensajes del server se ha encontrado con un mensaje que no ha sabido tratar.\n");
                        break;

                }

            }while(numeroCalculadoresFinalizados < numhijos);

            // Sumamos el tiempo que ha tardado el proceso servidor
            tFinalCalculo = clock();
            tCalculo += ((double)(tFinalCalculo - tInicioCalculo)) / CLOCKS_PER_SEC;

		    // Mostramos la información de que los calculadores han finalizado y el tiempo total empleado
            printf("INFO: Todos los Calculadores han finalizado su trabajo.\n");
            printf("INFO: Tiempo total de cálculo: %lf segundos.\n", tCalculo);
            
            // Cerramos la cola de mensajería
		    msgctl(msgid,IPC_RMID,NULL);

            // Cerramos el fichero en el que volcamos los primos que encontramos
            fclose(ficheroNumerosPrimos);

            // Liberamos la memoria dinámica pedida
            free(pidhijos);
            free(numerosParaCalculadores);

            // Finalizamos la ejecución del proceso servidor
            exit(0);
		  
	   }
    }

    // Proceso padre, RAÍZ
    else
    {
	  
        // Saltamos la alarma y asignamos su manejador correspondiente
        alarm(INTERVALO_TIMER);
        signal(SIGALRM, alarmHandler);

        // Esperamos a que el servidor finalice su trabajo
        wait(NULL);

        // Mostramos la información final, cuántas líneas tiene el fichero en el que volcamos los primos
        printf("INFO: El programa ha finalizado la tarea, número de líneas del fichero '%s' (números primos encontrados): %d\n", NOMBRE_FICH, ContarLineas());

    }
}

/* Codificación de Funciones */
// Función con la que comprobamos si un número pasado como argumento es primo o no, retorna 1 si es primo y 0 si no (algoritmo bruteforce)
int Comprobarsiesprimo(long int numero){

    int esPrimo = 1;

    // Recorremos todos los números desde el 2 hasta la mitad del número a comprobar, si es divisible por alguno de ellos no es primo
    for(int i = 2; esPrimo && i <= (numero / 2); i++)
        if(numero % i == 0)
            esPrimo = 0;

    return esPrimo;

}

// Función con la que mostramos que un nuevo primo se ha encontrado si el parámetro verbosity ha sido introdicido
void Informar(char *texto, int verboso){

    // Si hay parámetro verbosity mostramos la información
    if(verboso){
        printf(texto);
        printf("\n");
    }

}

// Función que se encarga de imprimir la jerarquía de procesos
void Imprimirjerarquiaproc(int pidraiz, int pidservidor, int *pidhijos, int numhijos){

    printf("JERARQUÍA DE PROCESOS:\nRAÍZ\tSERVER\tCALCULADORES\n");

    // Mostramos la jerarquía, si es la primera línea mostramos el raíz y el servidor a parte del calculador
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

    // Variables que vamos a usar, el número que hay en el fichero de cuenta de primos y el código de error que obtengamos de la operación
    int nCuentaPrimos = 0;
    int errCode = 0;

    // Abrimos el fichero de cuenta de primos en modo lectura
    FILE *fichero = fopen(NOMBRE_FICH_CUENTA, "r");

    // Leemos la cuenta de primos que contiene
    errCode = fscanf(fichero, "%d", &nCuentaPrimos);

    // Cerramos el fichero
    fclose(fichero);

    // Si ha habido error mostramos el error, en caso contrario mostramos el valor de la cuenta de primos
    if(errCode <= 0)
        printf("ERROR: Ha ocurrido un error al intentar leer el fichero %s\n", NOMBRE_FICH_CUENTA);
    else
        printf("INFO: Valor del fichero '%s': %d\n", NOMBRE_FICH_CUENTA, nCuentaPrimos);

    // Volvemos a hacer saltar la alarma
    alarm(INTERVALO_TIMER);

}

// Función con la que contamos las líneas del fichero que contiene los números primos encontrados
int ContarLineas(){

    // Variable en la que almacenaremos el número de líneas que tiene el fichero
    int nLineasFichero = 0;

    // Abrimos el fichero de números primos encontrados
    FILE *fichero = fopen(NOMBRE_FICH, "r");
    char caracterAux = '\0';

    // Recorremos todo el fichero buscando los retornos de carro, cada uno significa que hemos introducido un nuevo número, por lo que llevamos la cuenta
    while(!feof(fichero)){

        caracterAux = fgetc(fichero);
        if(caracterAux == '\n')
            nLineasFichero++;

    }

    // Cerramos el fichero
    fclose(fichero);

    // Mostramos el número de líneas del fichero
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

    // Variables del código de error de mensajería, comprobación de un número primo y PID del proceso que manda el mensaje
    int msgErr;
    int esPrimo;
    int pidEmisor;

    // Si recibimos señal SIGUSR1 (la manda el servidor cuando nos manda el rango a comprobar)
    if(signal == SIGUSR1){  // Mensaje para calculadores

        // Recibimos el mensaje 
        msgErr = msgrcv(msgid, &message, sizeof(message), 0, 0);

        if(msgErr == -1)
            perror("ERROR: Error al recibir el mensaje: ");
        else if(msgErr == 0)
            printf("ERROR: Solo se recibió el tipo de mensaje.\n");

        // Si el mensaje es del tipo de límites
        if(message.mesg_type == COD_LIMITES){

            // Recojoemos los datos del mensaje
            sscanf(message.mesg_text, "%d %d", &inicioRango, &finalRango);

            // Esperamos para no sobrecargar innecesariamente la cola de mensajería (hasta que el servidor comience a leer)
            sleep(5);

            // Iniciamos el contador
            tCalculo = 0;
            tInicioCalculo = clock();

            // Recorremos nuestro rango asignado
            for(long int i = inicioRango; i <= finalRango; i++){

                // Comprobamos si cada número del rango es primo
                esPrimo = Comprobarsiesprimo(i);
                if(esPrimo){

                    // Mandamos el mensaje al servidor de nuevo pirmo encontrado, con el PID de este calculador y el primo encontrado
                    do{
                        message.mesg_type = COD_RESULTADOS;
                        sprintf(message.mesg_text, "%d %d", getpid(), i);
                        msgErr = msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                        if(msgErr == -1)
                            perror("ERROR: Ha ocurrido un error al enviar el mensaje de nuevo número primo al servidor:");
                    }while(msgErr == -1);

                }

            }

            // Al acabar de calcular paramos el contador y comprobamos los segundos que ha tardado
            tFinalCalculo = clock();
            tCalculo = ((double)(tFinalCalculo - tInicioCalculo)) / CLOCKS_PER_SEC;

            // Mandamos al servidor que este calculador ya ha acabado con su PID y el tiempo que ha tardado
            do{
                message.mesg_type = COD_FIN;
                sprintf(message.mesg_text, "%d %lf", getpid(), tCalculo);
                msgErr = msgsnd(msgid, &message, sizeof(message), IPC_NOWAIT);
                if(msgErr == -1)
                    perror("ERROR: Ha ocurrido un error al enviar el mensaje de finalización al servidor: ");
            }
            while(msgErr == -1);

            // Finalizamos la ejecución del calculador
            exit(0);

        }
        // Si la señal es SIGINT es que hemos recibido un CRL C del usuario y nos aseguramos de finalizar la ejecución de forma segura
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

// Función con la que formateamos un informe para la función Informa()
char *formateaPrimoEncontrado(char *cadenaCaracteres, int pidCalculador, char *cadenaCaracteres2, int primoEncontrado){

    // Variable en la que almacenaremos el informe final
    char *cadenaFinal = (char*)malloc(sizeof(char));

    // Variables en las que vamos a almacenar la transformación a cadena de caracteres de los dos enteros que pedimos por parámetro
    char varAuxCalculador[LONGITUD_MSG];
    char varAuxPrimo[LONGITUD_MSG];

    // Índices de las cadenas de caracteres
    int indiceCadenaFinal = 0;
    int indiceCadenaCaracteres = 0;
    int indiceCalculador = 0;
    int indiceCadenaCaracteres2 = 0;
    int indicePrimo = 0;

    // Volcamos los enteros a las variables de cadenas de caracteres
    sprintf(varAuxCalculador, "%d", pidCalculador);
    sprintf(varAuxPrimo, "%d", primoEncontrado);

    // Recorremos la cadena de caracteres, y la incluímos en la cadena final con memoria dinámica (repetimos para las 4 cadenas, las 2 que pedimos y las 2 provenientes de enteros)
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

    // Introducimos el final de cadena
    cadenaFinal[indiceCadenaFinal] = '\0';

    // Retornamos la cadena
    return cadenaFinal;

}

// Función con la que volcamos un número primo encontrado a un fichero, retorna 0 si todo ha ido bien y 1 si ha habido algún error
int vuelcaPrimoAFichero(long int numeroPrimo, FILE *fichero){

    // Variables en las que recogemos el código de error de la operación y en la que retornamos el código de error de nuestra función
    int errCode = 0;
    int retorno = 0;

    // Escribimos el primo en el fichero correspondiente
    errCode = fprintf(fichero, "%d\n", numeroPrimo);

    // Si hay error lo mostramos y activamos el error en la variable de retorno
    if(errCode < 0){
        printf("ERROR: Ha ocurrido un error al intentar volcar el primo %d al fichero %s.\n", numeroPrimo, NOMBRE_FICH);
        retorno = 1;
    }

    // Retornamos si han ocurrido errores
    return retorno;

}

// Función con la que volcamos la cantidad de primos encontrados al fichero que lleva la cuenta de primos encontrados, retorna 0 si no hay errores y 1 si hay errores
int vuelcaCantidadPrimos(long int cantidadPrimos){

    // Variables en las que almacenamos el código de error de la operación y mandamos el código de error de nuestra función
    int errCode = 0;
    int retorno = 0;

    // Abrimos el fichero que lleva la cuenta de los primos encontrados
    FILE *fichero = fopen(NOMBRE_FICH_CUENTA, "w");

    // Escribimos la cuenta de primos que llevamos
    errCode = fprintf(fichero, "%d", cantidadPrimos);

    // Cerramos el fichero
    fclose(fichero);

    // Si hay error lo mostramos
    if(errCode < 0){
        printf("ERROR: Ha ocurrido un error al intentar volcar la cantidad de números primos (%d) al fichero %s.\n", cantidadPrimos, NOMBRE_FICH_CUENTA);
        retorno = 1;
    }

    // Retornamos si ha habido error o no
    return retorno;

}
