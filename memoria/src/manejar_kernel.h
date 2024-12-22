#ifndef MANEJARKERNEL_H
#define MANEJARKERNEL_H

#include "main.h"

void gestionar_kernel(void);

void *manejar_peticion(void *arg);

char *first_fit(int tam_proceso);

char *best_fit(int tam_proceso);

char *worst_fit(int tam_proceso);

char *first_fit_din(int tam_proceso);

char *best_fit_din(int tam_proceso);

char *worst_fit_din(int tam_proceso);

bool crear_proceso(int pid, String archivo_pseudocodigo, int tam_proceso);

bool crear_hilo(int pid, int tid, String archivo_pseudocodigo, int prioridad);

bool finalizar_proceso(int pid);

bool finalizar_hilo(int pid, int tid);

bool memory_dump(int pid, int tid);

t_list *obtener_instrucciones(String filename);

char *asignar_memoria(int tam_proceso);

void inicializar_registros(t_registros_hilo *registros);

#endif
