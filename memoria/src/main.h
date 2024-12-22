#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <string.h>
#include "pthread.h"

#include "commons/collections/list.h"
#include "commons/string.h"

#include "../../utils/src/commons.h"
#include "../../utils/src/server/utils.h"

#include "manejar_cpu.h"

#include "manejar_kernel.h"

typedef char* String;

/*typedef enum {
    
    MEMORIA_CPU_OBTENER_CONTEXTO,
    MEMORIA_CPU_ACTUALIZAR_CONTEXTO,
    MEMORIA_CPU_OBTENER_INSTRUCCION,
    MEMORIA_CPU_READ_MEM,
    MEMORIA_CPU_WRITE_MEM,
    MEMORIA_KERNEL_CREAR_PROCESO,
    MEMORIA_KERNEL_CREAR_HILO,
    MEMORIA_KERNEL_FINALIZAR_PROCESO,
    MEMORIA_KERNEL_FINALIZAR_HILO,
    MEMORIA_KERNEL_MEMORY_DUMP

}Memoria_operaciones_t;*/ //se cambia a commons

typedef struct
{
    String puerto_escucha;
    int ip_filesystem;
    int puerto_filesystem;
    int tam_total_memoria;
    String path_instrucciones;
    int retardo_respuesta;
    String esquema;
    String algoritmo_busqueda;
    char **lista_particiones;
    String log_level;

}t_memoria_config;

typedef struct{

    int tamanio;
    int estado;
    char *base;

}t_particion;


void inicializar_memoria();

t_memoria_config *cargar_memoria_config(String nombre_archivo);

void gestionar_particiones();

t_proceso *get_proceso(int pid);

t_hilo *get_hilo(int pid, int tid);

bool liberar_proceso(void *ptr_proceso);

bool liberar_hilo(void *ptr_hilo);

void liberar_memoria(char *base);

void liberar_particion(void *arg);

void enviar_estado(int estado, int fd_cliente, int operacion);



#endif //MAIN_H