#ifndef COMMONS_H_
#define COMMONS_H_

#include <commons/log.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include "commons/config.h"

#include <commons/collections/queue.h>
#include <commons/collections/list.h>

typedef char* String;
#define OK 1
#define FAIL 0

typedef struct{

    char *base;
    int limite;//tam del proceso

} t_contexto_comun;

typedef struct {

    int ax, bx, cx, dx, ex, fx, gx, hx, pc;

} t_registros_hilo;

typedef struct {

    int tid;
    int prioridad;
    t_registros_hilo registro;
    String archivo_pseudocodigo;
    t_list *instrucciones;

} t_hilo;

typedef struct {

    int pid;
    t_contexto_comun contexto;//contexto de ejecucion comun
    t_dictionary *hilos;//hilos del proceso
    String archivo_pseudocodigo;

} t_proceso;

typedef struct t_contexto_cpu {
    
    t_contexto_comun contexto;
    t_registros_hilo registros;

} t_contexto_cpu;

typedef enum
{
	KERNEL,
	MEMORIA,
	FILESYSTEM,
	CPU
} modulos;

typedef enum
{
	NEW,
	READY,
	EXEC,
	BLOCKED,
	EXIT
} estados_proceso;

typedef enum
{
    GENERICA,
    STDIN,
    STDOUT,
    DIALFS
} tipo_io;

typedef enum
{
    OP_IO_GEN_SLEEP,
    OP_IO_STDIN_READ,
    OP_IO_STDOUT_WRITE,
    OP_IO_FS_CREATE,
    OP_IO_FS_DELETE,
    OP_IO_FS_TRUNCATE,
    OP_IO_FS_WRITE,
    OP_IO_FS_READ
}operaciones_IO;
typedef enum
{
    OP_WAIT,
    OP_SIGNAL
}operaciones_recursos;
typedef struct{
    uint32_t PC,AX, BX, CX, DX;
    uint32_t EX, FX, GX, HX, BASE, LIMITE;
} Registros;
typedef struct 
{
    char* recurso;                // Identificador único del mutex
    bool tomado;                // Indica si el mutex está tomado
    bool bloqueado;             // Estado del mutex
    uint32_t hilo_actual;       // ID del hilo que tiene el mutex tomado (0 si no está tomado)
    t_queue* cola_bloqueados;   // Cola de hilos bloqueados esperando este mutex
} Mutex;


typedef struct 
{
	uint32_t PID;
    estados_proceso estado; // por ahi esto no es necesario, porque el PCB está o no en las colas
    t_list* TIDs;
    t_list* mutex;
} PCB;

typedef struct 
{
	uint32_t PID;
    uint32_t TID;
    uint32_t prioridad;
    estados_proceso estado;
} TCB;

typedef enum
{
    INTERRUPCION_EXTERNA = 1,
    OP_OUT_OF_MEMORY = 2
}interrupt_code;

typedef struct
{
    uint32_t size; // Tamaño del payload
    uint32_t offset; // Desplazamiento dentro del payload
    void* stream; // Payload
} t_buffer;

typedef enum
{
    SET,
    READ_MEM,
    WRITE_MEM,
    SUM,
    SUB,
    JNZ,
    LOG,

    DUMP_MEMORY,
    IO,
    PROCESS_CREATE,
    THREAD_CREATE,
    THREAD_JOIN,
    THREAD_CANCEL,
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    THREAD_EXIT,
    PROCESS_EXIT,

//

    RESIZE,
    COPY_STRING,
    WAIT,
	SIGNAL,
    INST_EXIT,
	INVALID_INSTRUCTION
} instruccion;


typedef struct
{
	instruccion instruccion;
	uint32_t *registro1, *registro2, *registro3;
	uint32_t valor; // Dato, tamaño, unidades de trabajo
	char *interfaz;
	char *archivo;
    char* recurso;
} t_instruccion;

typedef struct
{
    uint8_t op_code;
    t_buffer* buffer;
} t_paquete;

typedef enum{
    WRITE,
    READ,
    DIRECCION_LOGICA
}accion;

typedef enum{
    WRITE_OK = 6,
    WRITE_FAULT = -6,
    READ_OK = 7,
    READ_FAULT = -7
}estado_memoria;

typedef enum {
    
    MEMORIA_CPU_OBTENER_CONTEXTO,
    MEMORIA_CPU_ACTUALIZAR_CONTEXTO,
    MEMORIA_CPU_OBTENER_INSTRUCCION,
    MEMORIA_CPU_READ_MEM,
    MEMORIA_CPU_WRITE_MEM,
    MEMORIA_KERNEL_CREAR_PROCESO,
    MEMORIA_KERNEL_CREAR_HILO,
    MEMORIA_KERNEL_FINALIZAR_PROCESO,
    MEMORIA_KERNEL_FINALIZAR_HILO,
    MEMORIA_KERNEL_MEMORY_DUMP,
    MEMORIA_CPU_OBTENER_DIRECCION
}Memoria_operaciones_t;


typedef struct{
    unsigned int frame;
    unsigned int desplazamiento;
    int pagina;
    accion accion_solicitada;
    uint32_t dato;
    estado_memoria estado;
} __attribute__((packed)) t_solicitud;

t_log_level obtener_log_level(const char* log_level_name);
t_log* iniciar_logger(String path, String name, bool is_active_console, t_log_level level);
t_config* iniciar_config(String path);

void buffer_add_contexto(t_buffer *buffer, t_contexto_cpu *contexto);
t_contexto_cpu *buffer_read_contexto(t_buffer *buffer);

//PCB * CrearPCB(int PID,int quantum);
//void PrintPCB(PCB p);
char* DeserializarString(t_buffer* buffer);
t_buffer * SerializarString(char* mensaje);
char* getNombreModulo(int modulo);
//t_buffer *bufferPCB(PCB *p);
t_buffer *buffer_create(int size);
void buffer_destroy(t_buffer *buffer);
void buffer_add_buffer(t_buffer* buffer1, t_buffer* buffer2);
void buffer_add_string(t_buffer *buffer, char *string);
void buffer_add_int(t_buffer *buffer, int data);
void buffer_add_uint32(t_buffer *buffer, uint32_t data);
void buffer_add_uint8(t_buffer *buffer, uint8_t data);
int buffer_read_int(t_buffer *buffer);
uint32_t buffer_read_uint32(t_buffer *buffer);
uint8_t buffer_read_uint8(t_buffer *buffer);
char* buffer_read_string(t_buffer *buffer);
void buffer_write(t_buffer* buffer, void* value, int size);
void* buffer_read(t_buffer * buffer,int size);
//t_buffer* SerializarPCB(PCB *p);


void EnviarOperacion(int socket, int op_code, t_buffer * buffer);
int recibirOperacion(int socket);
t_buffer * RecibirBuffer(int socket);
void EnviarBuffer(int socket,t_buffer *buffer);
char* get_instruction_name(int value);
void SerializarPIDyPC(t_buffer *buffer, uint32_t pid, uint32_t pc);
int buffer_get_int(t_buffer* buffer);
void insertAtEnd(char *str, const char *value);
void deleteValue(char *str, const char *value);
t_log_level get_log_level_from_string(const char* log_level_str);


//Contexto CPU
TCB* DeserializarTCB(t_buffer *buffer);
t_buffer * SerializarTCB(TCB *p);
PCB* DeserializarPCB(t_buffer* buffer);
t_buffer * SerializarPCB(PCB* pcb);
Registros* DeserializarRegistros(t_buffer *buffer);
void SerializarRegistros(t_buffer *buffer, Registros *registros);
t_buffer* SerializarPIDTID(Registros *registros, uint32_t PID, uint32_t TID);

//CPU + MEMORIA
void EnviarBuffer(int socket,t_buffer * buffer);
void InicializarRegistros(Registros *r);
t_buffer * buffer_get_sobrante(t_buffer * buffer);

//CPU
void asignarMemoriaParaInt(uint32_t **ptr);
void asignarMemoriaParaString(char **ptr);


#endif