#ifndef KERNEL_MAIN_H
#define KERNEL_MAIN_H
#define MAX_NIVELES_PRIORIDAD 100

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/config.h>
#include <commons/log.h>
#include <commons.h>

#include <client/utils.h>
#include <server/utils.h>


typedef struct{
    char* ip_memoria;
    char* puertoMemoria;
    char* ip_cpu;
    char* puerto_cpu_dispatch;
    char* puerto_cpu_interrupt;
    char* algoritmo_planificacion;
    int quantum;
    char* log_level;
}KERNEL_config;

typedef enum {
    CMN,
    FIFO,
    PRIORIDADES,
    COLAS_MULTINIVEL
} Algoritmo;

typedef struct
{
    TCB* hilo;
    TCB* dependiente;
} dependencia;


typedef struct {
    t_queue* colas_prioridad[MAX_NIVELES_PRIORIDAD]; // Array de listas, una por nivel de prioridad
} ColasReady;




extern ColasReady colas_ready_cmn; // si el alg de planif es cmn uso este array de colas

extern t_list * lista_exec_hilo;
extern t_list * lista_bloq_hilo;
extern t_list * lista_dependencias;//hilos dependientes
extern t_list* lista_thread_id; // lista de contadores para ir asignando los TID autoincrementales



extern bool desalojado;




extern pthread_mutex_t mutex_cola_ready_fifo_prioridades;
extern pthread_mutex_t mutex_colas_ready_cmn;
extern pthread_mutex_t mutex_lista_exec_hilo;
extern pthread_mutex_t mutex_config;
extern pthread_mutex_t mutex_lista_thread_id; 
extern pthread_mutex_t process_id_mutex;
extern pthread_mutex_t thread_id_mutex;
extern pthread_mutex_t mutex_kernel_log_counter;
extern pthread_mutex_t mutex_cola_new;
extern pthread_mutex_t mutex_cola_ready;


extern t_log * kernel_logger;
extern t_config* config;
extern KERNEL_config kernel_config;

//COLAS
extern t_queue* cola_new;
extern t_queue* cola_ready;
extern t_queue* cola_exec;
extern t_queue* cola_blocked;
extern t_queue* cola_exit;
extern t_queue* cola_ready_fifo_prioridades; // si el alg de planificacion es fifo o prioridades uso esta cola

//INT
extern int kernel_log_counter;
extern int socket_dispatch;
extern int socket_interrupt;
extern int socket_memoria;
extern int process_id; // el contador para ir asignando los PID autoincrementales


extern sem_t sem_nuevoProceso;
extern Algoritmo algoritmo;



//------------Prototipos 
//OTROS
void iniciarLogger();
void iniciarConfig();
void iniciador();
void leerConfig();
void conectarCPU();
void conectarMemoria();
void inicializar_colas();
int enviar_pedido_a_memoria(PCB *proceso);
void destruir_pcb(PCB* pcb);
void destruir_tcb(TCB* tcb);
void destruirEstructuras();
Algoritmo buscarAlgoritmo(char* token);

//HILOS
void crear_hilo(char* archivo_pseudocodigo, int prioridad, int pid);
bool insertar_en_cola_ready_cmn(TCB *hilo);
void finalizar_hilo(TCB *hilo_finalizado);
void liberar_tcb (TCB *hilo);
void desbloquear_hilos_por_dependencias (TCB *hilo_finalizado);
TCB* crear_tcb(int prioridad, int pid);
void agregarHiloAReady(TCB* tcb);
TCB* buscarHilo(uint32_t PID, uint32_t TID);

//PROCESOS
PCB* crear_pcb();
void crearProceso(char * archivo_pseudo, int tam_proceso, int prioridad_tid0);
int informar_a_memoria(PCB* proceso);
void finalizarProceso(PCB *pcb_finalizado);
void crearProcesoInicial(archivo_pseudocodigo, tamanio_proceso);

//CPU
void enviarTCBCPU(TCB* tcb);
void interpretarCPU();

//MEMORIA
int finalizarProcesoMemoria(PCB* pcb);
int crearProcesoMemoria(PCB *proceso,char* archivo,uint32_t tamanio);
bool crearHiloMemoria(TCB *hilo,char* archivo);
bool finalizarHiloMemoria(TCB *hilo);

//LARGO PLAZO
void planificador_largo_plazo();
void atender_colas_multinivel();


//CORTO PLAZO
void planificador_corto_plazo();
TCB* elegir_maxima_prioridad(t_queue* cola_ready_fifo_prioridades);
void atender_cmn();
void atender_fifo();
void atender_prioridades();

void queue_remove(t_queue* cola, TCB* un_tcb);
TCB* get_next_thread(int prioridad);
bool tcb_equals(TCB* tcb1, TCB* tcb2);
void ejecutar_hilo(TCB* thread);


//FUNCIONES DE COLAS
PCB* obtener_PCB_por_PID(t_queue* elementos, uint32_t PID);
bool comparar_por_PID(void* elemento);
bool es_TCB_buscado(TCB* tcb, uint32_t PID, uint32_t TID);
TCB* quitar_TCB_de_cola(t_queue* cola, uint32_t PID, uint32_t TID);
TCB* buscarTCBDeCola(t_queue* cola, uint32_t PID, uint32_t TID);

//FUNCIONES DE ARRAYS DE COLAS
TCB* buscarTCBPorPIDyTID(ColasReady colas_ready, uint32_t PID, uint32_t TID);

//FUNCIONES DE LISTAS
TCB* obtener_TCB_por_PID_y_TID(t_list* lista_exec_hilo, uint32_t PID, uint32_t TID);
bool comparar_por_PID_y_TID(void* elemento);
void quitar_TCB_de_lista_exec_hilo(t_list* lista_exec_hilo, uint32_t PID, uint32_t TID);
bool comparar_por_PID_y_TID(void* tcb);
bool tieneDependiente(TCB* hilo);
int buscarDependiente(TCB* hilo);
Mutex* buscar_mutex(t_list*,char*);


#endif
