#include "../utils_C/main.h"

sem_t sem_busqueda_proceso;
sem_t sem_ciclo;
sem_t sem_interrupt;
sem_t sem_interrupt_fin; 

pthread_mutex_t mutex_logger = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_flag_interrpupcion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_interrupcionRecibida = PTHREAD_MUTEX_INITIALIZER;

CPU_config cpu_config;  // Definición de la variable
bool interrupcionRecv;  // Definición de la variable
int tamanio_pagina;  // Definición de la variable
int socket_conexionMemoria;
server interrupt;  // Definición de la variable
server dispatch;  // Definición de la variable
bool interrupcionRecibida;  // Definición de la variable
int procesoEnEjecucion;
int threadEnEjecucion;




int main(int argc, char *argv[]){
    
    // Config inicial
    t_config *config = config_create("cpu.config");
    obtenerConfiguracion(config);

    iniciarLogger();

    //conecto a memoria
    conectarAMemoria();

    //Iniciar Server de CPU(Kernel)
    gestionarKernel();

    while(1){
        sleep(1);
    }

    return 0;
}

void gestionarKernel(){
    interrupt.f = ServerCPU;
    interrupt.socket = getSocketServidor(cpu_config.puerto_interrupt);    
    if(interrupt.socket < 0)
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Ha habido un problema al iniciar el servidor %i", interrupt.socket);
        pthread_mutex_unlock(&mutex_logger);
        exit(1);
    } else {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Socket: %i", interrupt.socket);
        pthread_mutex_unlock(&mutex_logger);
    }
    
    dispatch.socket = getSocketServidor(cpu_config.puerto_dispatch);  
    dispatch.f = ServerCPU;
    if(dispatch.socket < 0)
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Ha habido un problema al iniciar el servidor %i", interrupt.socket);
        pthread_mutex_unlock(&mutex_logger);
        exit(1);
    }
    else
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Socket: %i", dispatch.socket);
        pthread_mutex_unlock(&mutex_logger);
    }

    pthread_t *thread_dispatch = malloc(sizeof(pthread_t));
    pthread_t *thread_interrupt = malloc(sizeof(pthread_t));
    pthread_create(thread_interrupt, NULL, (void*)iniciar_servidor, &interrupt);
    pthread_detach(*thread_interrupt);
    pthread_create(thread_dispatch, NULL, (void*)iniciar_servidor, &dispatch);
    pthread_detach(*thread_dispatch);
}

void ServerCPU(int socketCliente) {
    int opcode = recibir_operacion(socketCliente);
    interrupcionRecibida = false;

    while (opcode >= 0) {
        switch (opcode) {
            case HANDSHAKE:
                handshake_Server(socketCliente, CPU);
                break;

            case CONTEXTO: {
                t_buffer *buffer = RecibirBuffer(socketCliente);
                TCB* tcb = DeserializarTCB(buffer);
                procesoEnEjecucion = tcb->PID;
                threadEnEjecucion = tcb->TID;

                cicloParams *params = malloc(sizeof(cicloParams));
                params->tcb = tcb;

                // Pedir el contexto a memoria
                log_info(logger,"PID:%d TID:%d - Solicito Contexto Ejecución. ",procesoEnEjecucion,threadEnEjecucion);
                Registros* registros = solicitar_contexto(tcb);
                params->registros = registros;
                params->socketMemoria = socket_conexionMemoria;
                params->socketCliente = socketCliente;

                interrupcionRecibida = false;
                params->interrupcionRecibida = &interrupcionRecibida;

                int codigo_desalojo;
                t_buffer* valorRetorno = EjecutarProceso(params, &codigo_desalojo);

                // Serializar y enviar datos de desalojo
                t_buffer* bufferMemoria = SerializarPIDTID(params->registros,procesoEnEjecucion,threadEnEjecucion);

                log_info(logger,"PID:%d TID:%d - Actualizo Contexto Ejecución. ",procesoEnEjecucion,threadEnEjecucion);
                EnviarOperacion(socket_conexionMemoria,MEMORIA_CPU_ACTUALIZAR_CONTEXTO,bufferMemoria);
                EnviarBuffer(socketCliente, valorRetorno);

                buffer_destroy(buffer);
                buffer_destroy(valorRetorno);
                free(params);
                break;
            }

            case INTERRUPCION: {
                t_buffer *buffer = RecibirBuffer(socketCliente);
                buffer_read_uint32(buffer);

                log_info(logger,"Llega interrupción al puerto interrupt. ");

                pthread_mutex_lock(&mutex_interrupcionRecibida);
                interrupcionRecibida = true;
                pthread_mutex_unlock(&mutex_interrupcionRecibida);

                buffer_destroy(buffer);
                break;
            }

            default:
                break;
        }

        // Obtener el siguiente opcode para continuar el bucle
        opcode = recibir_operacion(socketCliente);
    }
}

void iniciarLogger(){
    t_log_level nivel = get_log_level_from_string(cpu_config.log_level);
    
    logger = log_create("cpu.log", "CPU", true, nivel);

    if(logger == NULL){
        write(0, "ERROR -> NO SE PUDE CREAR EL LOG \n", 30);
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "CPU INIT");
    pthread_mutex_unlock(&mutex_logger);

    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "----Log modulo CPU ----");
    pthread_mutex_unlock(&mutex_logger);
}

void obtenerConfiguracion(t_config *config)
{
    cpu_config.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    cpu_config.puertoMemoria = config_get_string_value(config, "PUERTO_MEMORIA");
    cpu_config.puerto_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");
    cpu_config.puerto_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    cpu_config.log_level = config_get_string_value(config, "LOG_LEVEL");
}

void obtenerDatosMemoria(){

    uint32_t desplazamiento = 1;
    t_buffer* buffer = buffer_create(sizeof(uint32_t));
    buffer_add_uint32(buffer, desplazamiento);

    EnviarOperacion(socket_conexionMemoria, 6, buffer);

    buffer = RecibirBuffer(socket_conexionMemoria);


    tamanio_pagina = buffer_read_uint32(buffer);

    log_info(logger,"consigo tamanio de pagina: %d",tamanio_pagina);

    pthread_mutex_lock(&mutex_logger);

    if(tamanio_pagina > 0){
        log_info(logger, "tamaño de pagina obtenida...");
    } else {
        log_error(logger, "error al obtener el tamaño de pagina");
    }

    pthread_mutex_unlock(&mutex_logger);
    
    free(buffer);
}

Registros* obtenerContexto(){
    Registros* registros = malloc(sizeof(Registros));


    return registros;
}

void conectarAMemoria(){
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "conectando a memoria...");
    pthread_mutex_unlock(&mutex_logger);
    
    socket_conexionMemoria = conectar(cpu_config.ip_memoria, cpu_config.puertoMemoria, CPU);
}

void liberar_configuracion(){
    free(cpu_config.ip_memoria);
    free(cpu_config.puerto_dispatch);
    free(cpu_config.puerto_interrupt);
    log_destroy(logger);
}

void iniciarSemaforos(){
    sem_init(&sem_busqueda_proceso, 0, 1);
    sem_init(&sem_ciclo, 0, 0);
    sem_init(&sem_interrupt, 0, 0);
    sem_init(&sem_interrupt_fin, 0, 0);
    
    pthread_mutex_init(&mutex_flag_interrpupcion, NULL);
    pthread_mutex_init(&mutex_logger, NULL);
    pthread_mutex_init(&mutex_interrupcionRecibida, NULL);
}

void liberarSemaforos(){
    sem_destroy(&sem_busqueda_proceso);
    sem_destroy(&sem_ciclo);
    sem_destroy(&sem_interrupt);
    sem_destroy(&sem_interrupt_fin);

    pthread_mutex_destroy(&mutex_flag_interrpupcion);
    pthread_mutex_destroy(&mutex_logger);
    pthread_mutex_destroy(&mutex_interrupcionRecibida);
}