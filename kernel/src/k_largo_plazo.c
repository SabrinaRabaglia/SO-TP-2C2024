#include <../include/main.h>

t_queue* cola_new;
t_queue* cola_ready;
t_queue* cola_exec;
t_queue* cola_blocked;
t_queue* cola_exit;
t_queue* cola_ready_fifo_prioridades;

sem_t sem_nuevoProceso;


void planificador_largo_plazo(){
    pthread_t hilo_largo;

    if(pthread_create(&hilo_largo,NULL, crearProceso,NULL) != 0){

        log_error(kernel_logger,"No se pudo crear el hilo del planificador de largo plazo. ");
        exit(EXIT_FAILURE);
    }
    
    pthread_detach(hilo_largo);
    return;
}


//---------------------PROCESOS-------------------------------------------
PCB *crear_pcb()
{ // Reserva el espacio en RAM e inicializa los campos. (Pone el estado del PCB en NEW pero no lo agrega a la COLA)
    PCB *nuevo_PCB = malloc(sizeof(PCB));
    if (nuevo_PCB == NULL)
    {
        log_error(kernel_logger, "No se pudo crear el PCB");
    }
    pthread_mutex_lock(&process_id_mutex);
    nuevo_PCB->PID = process_id; // process_id lo gestiona con una var global
    process_id++;
    pthread_mutex_unlock(&process_id_mutex);
    nuevo_PCB->TIDs = list_create();
    nuevo_PCB->mutex = list_create();
    nuevo_PCB->estado = NEW;
    return nuevo_PCB;
}

void crearProceso(char * archivo_pseudo, int tam_proceso, int prioridad_tid0){ // si puede crea el proceso y si no lo encola. Sincroniza con finalizarProceso()
    
    PCB *nuevoPCB = crear_pcb();
    if (queue_is_empty(cola_new))
    {
        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger, "%d (<%d>:0) Se crea el proceso - Estado: NEW", kernel_log_counter, nuevoPCB->PID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        pthread_mutex_lock(&mutex_cola_new);
        queue_push(cola_new, nuevoPCB);
        pthread_mutex_unlock(&mutex_cola_new);

        int respuesta = crearProcesoMemoria(nuevoPCB,archivo_pseudo,tam_proceso);
        if (respuesta == 1)
        { // Si memoria tiene espacio: (Preguntarle a Lean que devuelve él)
            pthread_mutex_lock(&mutex_cola_new);
            queue_pop(cola_new);
            pthread_mutex_unlock(&mutex_cola_new);
            nuevoPCB->estado = READY;
            pthread_mutex_lock(&mutex_cola_ready);
            queue_push(cola_ready, nuevoPCB);
            pthread_mutex_unlock(&mutex_cola_ready);
            crear_hilo(archivo_pseudo, prioridad_tid0, nuevoPCB->PID); 
            pthread_mutex_lock(&mutex_lista_thread_id);
            list_add_in_index(lista_thread_id,nuevoPCB->PID, 0 );
            pthread_mutex_unlock(&mutex_lista_thread_id);

        }
        else
        {
            sem_wait(&sem_nuevoProceso);
            crearProceso(archivo_pseudo, tam_proceso, prioridad_tid0);
        }
    }
    else
    {
        pthread_mutex_lock(&mutex_cola_new);
        queue_push(cola_new, nuevoPCB);
        pthread_mutex_unlock(&mutex_cola_new);
    }
}

void finalizarProceso(PCB *pcb_finalizado)
{ // Le mando el pcb a memoria y con el ok lo libero

    int respuesta = finalizarProcesoMemoria(pcb_finalizado);
    if (respuesta == 1)
    { 
        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger, "%d Finaliza el proceso <%d>", kernel_log_counter, pcb_finalizado->PID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        int count = pcb_finalizado->TIDs->elements_count;
        while (count != 0)
        {
            uint32_t tid_n = list_get(pcb_finalizado->TIDs,count-1);
            TCB* tcb_n = obtener_TCB_por_PID_y_TID(lista_exec_hilo, pcb_finalizado->PID, tid_n);
            quitar_TCB_de_lista_exec_hilo(lista_exec_hilo,pcb_finalizado->PID,tid_n);
            free(tcb_n);
        }
        free(pcb_finalizado->mutex);
        free(pcb_finalizado->TIDs);
        free(pcb_finalizado);
    }
    sem_post(&sem_nuevoProceso);
}


void crearProcesoInicial(char* archivo_pseoudocodigo, int tam_proceso) { 
    crearProceso(archivo_pseoudocodigo, tam_proceso,0);
}
//----------------------FIN PROCESOS ---------------------------------------------------

//---------------------HILOS----------------------------------------------------

void crear_hilo(char* archivo_pseudocodigo, int prioridad, int pid) //crea el TCB y lo agrega a la cola de READY que corresponda
{
    // Crear el TCB para el nuevo hilo
    TCB *nuevoTCB = crear_tcb(prioridad, pid);
    if (!nuevoTCB)
    {
        log_error(kernel_logger, "Error al crear el TCB para el nuevo hilo.");
        return;
    }


    // Notificar al módulo Memoria sobre la creación del hilo
    if (!crearHiloMemoria(nuevoTCB, archivo_pseudocodigo)) 
    {
        log_error(kernel_logger, "Error al notificar a Memoria la creación del hilo.");
        liberar_tcb(nuevoTCB); // Limpieza en caso de fallo
        return;
    }

    // Insertar el nuevo TCB en la cola READY correspondiente según algoritmo corto plazo

    agregarHiloAReady(nuevoTCB);

}

bool insertar_en_cola_ready_cmn(TCB *hilo)
{
    if (!hilo || hilo->prioridad < 0 || hilo->prioridad >= MAX_NIVELES_PRIORIDAD)
    {
        log_error(kernel_logger, "Error: TCB inválido o prioridad fuera de rango.");
        return false;
    }

    t_queue *cola_prioridad = colas_ready_cmn.colas_prioridad[hilo->prioridad];
    if (!cola_prioridad)
    {
        log_error(kernel_logger, "Error: cola de prioridad %d no inicializada.", hilo->prioridad);
        return false;
    }

    queue_push(cola_prioridad, hilo);
    
    pthread_mutex_lock(&mutex_kernel_log_counter);
    log_info(kernel_logger, "Hilo (PID:%d, TID:%d) encolado en READY, prioridad %d.",hilo->PID, hilo->TID, hilo->prioridad);
    kernel_log_counter++;
    pthread_mutex_unlock(&mutex_kernel_log_counter);
    
    return true;
}

void finalizar_hilo(TCB *hilo_finalizado)
{
    if (!hilo_finalizado)
    {
        log_error(kernel_logger, "Error: el TCB proporcionado es nulo.");
        return;
    }

    // Informar a Memoria sobre la finalización del hilo
    if (!finalizarHiloMemoria(hilo_finalizado))
    {
        log_error(kernel_logger, "Error al notificar a Memoria la finalización del hilo (PID:%d, TID:%d).",
                  hilo_finalizado->PID, hilo_finalizado->TID);
        return;
    }
    // Liberar el TCB del hilo finalizado
    liberar_tcb(hilo_finalizado);

    // Registrar el log de finalización del hilo
    pthread_mutex_lock(&mutex_kernel_log_counter);    
    log_info(kernel_logger, "%d (%d:%d) Finaliza el hilo",kernel_log_counter, hilo_finalizado->PID, hilo_finalizado->TID);
    kernel_log_counter++;
    pthread_mutex_unlock(&mutex_kernel_log_counter);
    
    free(hilo_finalizado); // Liberar memoria del TCB
}

void liberar_tcb(TCB *hilo)
{
    if (!hilo) return;
    if (tieneDependiente(hilo)){
        
        buscarDependiente(hilo);
        quitar_TCB_de_lista_exec_hilo(lista_bloq_hilo,hilo->PID,hilo->TID);
    }
}

TCB *crear_tcb(int prioridad, int pid)
{
    TCB *nuevo_TCB = malloc(sizeof(TCB));
    if (nuevo_TCB == NULL)
    {
        log_error(kernel_logger, "No se pudo crear el TCB");
        
    }
    pthread_mutex_lock(&mutex_lista_thread_id);
    uint32_t ultimo_tid = list_get(lista_thread_id, pid); 
    list_replace(lista_thread_id, pid, ultimo_tid+1);
    pthread_mutex_unlock(&mutex_lista_thread_id);
    nuevo_TCB->prioridad = prioridad;
    nuevo_TCB->estado = READY;
    nuevo_TCB->PID = pid;
    return nuevo_TCB;
}

//----------------------FIN HILOS ---------------------------------------------------
