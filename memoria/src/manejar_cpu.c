#include "manejar_cpu.h"

extern int fd_server_memoria;

extern t_log *logger;

extern t_dictionary *procesos;

void gestionar_cpu() {

    pthread_t hilo_cpu;

    if(pthread_create(&hilo_cpu, NULL, hilo_instrucciones_cpu, NULL) != 0) {

        perror("No se pudo craer el hilo instrucciones cpu");

        exit(EXIT_FAILURE);
    }

    log_debug(logger, "Hilo de cpu creado con exito");

    pthread_detach(hilo_cpu);

    return;
}

void *hilo_instrucciones_cpu(void *args) {

    int fd_cpu = esperar_cliente(fd_server_memoria);

    if(handshake_Server(fd_cpu, MEMORIA) == -1) {
        perror("Error en handshake");
        
        exit(EXIT_FAILURE);
    }

    t_paquete *paquete;

    int pid, tid;

    t_contexto_cpu *contexto;

    while(1) {

        paquete = recibir_paquete(fd_cpu);

        if(paquete == NULL) {
            perror("No se pudo recibir la paquete");

            exit(EXIT_FAILURE);
        }

        switch (paquete->op_code)
        {
        case MEMORIA_CPU_OBTENER_CONTEXTO:

            pid = buffer_read_int(paquete->buffer);

            tid = buffer_read_int(paquete->buffer);

            contexto = obtener_contexto(pid, tid);

            if(contexto == NULL) {

                log_error(logger, "No se pudo encontrar el contexto del proceso: %i, tid: %i", pid, tid);

                buffer_destroy(paquete->buffer);
                
                continue;
            }
            
            enviar_contexto(contexto, fd_cpu);

            free(contexto); //libero la memoria asignada

            buffer_destroy(paquete->buffer); //faltaria algo como paquete_destroy
            
            break;

        case MEMORIA_CPU_ACTUALIZAR_CONTEXTO:

            pid = buffer_read_int(paquete->buffer);

            tid = buffer_read_int(paquete->buffer);

            contexto = buffer_read_contexto(paquete->buffer);

            bool estado = actualizar_contexto(pid, tid, contexto);

            enviar_estado(estado, fd_cpu, MEMORIA_CPU_ACTUALIZAR_CONTEXTO);

            buffer_destroy(paquete->buffer);

            break;

        case MEMORIA_CPU_OBTENER_INSTRUCCION:

            pid = buffer_read_int(paquete->buffer);

            tid = buffer_read_int(paquete->buffer);

            int pc = buffer_read_int(paquete->buffer);

            String instruccion = obtener_instruccion_numero(pid, tid, pc);

            enviar_instruccion(fd_cpu, instruccion);

            buffer_destroy(paquete->buffer);

            break;

        case MEMORIA_CPU_WRITE_MEM:

            break;

        case MEMORIA_CPU_READ_MEM:

            break;

        default:
            log_error(logger, "La peticion recibida no es correcta!");
            break;
        }
    }
}

t_contexto_cpu *obtener_contexto(int pid, int tid) {

    t_contexto_cpu *contexto = malloc(sizeof(t_contexto_cpu));

    t_proceso *proceso = get_proceso(pid);

    if(proceso == NULL) {

        return NULL;
    }

    contexto->contexto.limite = proceso->contexto.limite;

    contexto->contexto.base = strdup(proceso->contexto.base);

    t_hilo *hilo = get_hilo(pid, tid);

    if(hilo == NULL) { 
        
        return NULL;
    }

    contexto->registros = hilo->registro; 

    return contexto;
}


void enviar_contexto(t_contexto_cpu *contexto, int fd_cliente) { 

    t_buffer *buffer = buffer_create(sizeof(t_contexto_cpu));

    buffer_add_contexto(buffer, contexto);

    EnviarBuffer(fd_cliente, buffer);

    buffer_destroy(buffer);

    return;
}

bool actualizar_contexto(int pid, int tid, t_contexto_cpu *contexto) {

    t_proceso *proceso = get_proceso(pid);

    if(proceso == NULL) {
        
        return false;
    }

    // proceso->contexto.base = strdup(contexto.contexto.base);//queda memoria colgada con el puntero anterior?
    // proceso->contexto.limite = contexto.contexto.limite;

    t_hilo *hilo = get_hilo(pid, tid);

    if(hilo == NULL) {
        
        return false;
    }

    hilo->registro.ax = contexto->registros.ax;
    hilo->registro.bx = contexto->registros.bx;
    hilo->registro.cx = contexto->registros.cx;
    hilo->registro.dx = contexto->registros.dx;
    hilo->registro.ex = contexto->registros.ex;
    hilo->registro.fx = contexto->registros.fx;
    hilo->registro.gx = contexto->registros.gx;
    hilo->registro.hx = contexto->registros.hx;
    hilo->registro.pc = contexto->registros.pc;

    return true;
}

String obtener_instruccion_numero(int pid, int tid, int pc) {

    t_hilo *hilo = get_hilo(pid, tid);

    return (String)list_get(hilo->instrucciones, pc);
}

void enviar_instruccion(int fd_cliente, String instruccion) {

    int len_instruccin = strlen(instruccion);

    t_buffer *buffer = buffer_create(sizeof(char) * len_instruccin);

    buffer_add_string(buffer, instruccion);

    EnviarBuffer(fd_cliente, buffer);

    return;
}