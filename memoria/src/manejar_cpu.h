#ifndef MANEJARCPU2_H
#define MANEJARCPU2_H

#include "main.h"


#define BUFF_SIZE 256

void gestionar_cpu();

void *hilo_instrucciones_cpu(void *args);

t_contexto_cpu *obtener_contexto(int pid, int tid);

void enviar_contexto(t_contexto_cpu *contexto, int fd_cliente);

bool actualizar_contexto(int pid, int tid, t_contexto_cpu *contexto);

String obtener_instruccion_numero(int pid, int tid, int pc);

void enviar_instruccion(int fd_cliente, String instruccion);

#endif // MANEJARCPU2_H