#ifndef UTILSMAIN_H_
#define UTILSMAIN_H_

#include <client/utils.h>
#include <server/utils.h>

#include <pthread.h>
#include <commons.h>
#include <commons/config.h>
#include <semaphore.h>

#include <math.h>

//estructuras
typedef struct
{
    char* ip_memoria;
    char* puertoMemoria;
    char* puerto_dispatch;
    char* puerto_interrupt;
    char* log_level;
}CPU_config;

typedef enum{
    HANDSHAKE,
    CONTEXTO,
    INTERRUPCION
}op_code;

typedef struct {
    TCB *tcb;
    Registros *registros;
    int socketMemoria;
    int socketCliente;
    bool *interrupcionRecibida;
} cicloParams;

//fin estructuras
//inicio definiciones varias

extern bool interrupcionRecv;
extern int tamanio_pagina;
extern server interrupt;
extern server dispatch;
extern bool interrupcionRecibida;


extern CPU_config cpu_config;
extern int socket_conexionMemoria;

extern int procesoEnEjecucion;
extern int threadEnEjecucion;

extern pthread_mutex_t mutex_logger;
extern pthread_mutex_t mutex_flag_interrpupcion;

typedef uint32_t dir_logica;

//fin definiciones varias
//definiciones de funciones

void obtenerConfiguracion(t_config *config);
void iniciarLogger();
void gestionarKernel();
void ServerCPU(int socketCliente);
void conectarAMemoria();
void obtenerDatosMemoria();
Registros* obtenerContexto();


//mmu
void* obtener_direccion_fisica(uint32_t, uint32_t, uint32_t);
int numero_pagina(dir_logica dir);
int calcular_desplazamiento(dir_logica dir, int pagina);

//conexion memoria
t_buffer* leer_en_memoria(void** direccion);
int escribir_en_memoria(void** direccion, void* value);
t_buffer* solicitar_instruccion(uint32_t pid,uint32_t tid, int pc);
Registros* solicitar_contexto(TCB* pcb);

//contexto




t_buffer* EjecutarProceso(void *args,int* codigo);
//void InicializarRegistros(Registros *reg);
char* fetchInstruccion(int socketMemoria, TCB *p,Registros* Registros);//modificacion lista
void inicializarInstruccion(t_instruccion *instruccion);//modificacion lista
uint32_t * obtenerRegistro(Registros* registros, char* token);//modificacion lista

instruccion obtenerInstruccion(char *token);//modificacion lista
t_instruccion parsearInstruccion(char* instruccion, Registros *registros);//modificacion lista
t_instruccion decodeInstruccion(char* instruccion, Registros *registros);//modificacion lista
bool executeInstruccion(t_instruccion *instruccion, int socketCliente, int procesoEnEjecucion, int threadEnEjecucion,uint32_t*);//modificacion lista

bool interrupciones(bool);

void asignarMemoriaParaInt(uint32_t **ptr);
void asignarMemoriaParaString(char **ptr);

#endif // UTILSMAIN_H_