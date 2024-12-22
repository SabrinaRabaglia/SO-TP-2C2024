#include <../include/main.h>

void planificador_corto_plazo_inicio(){
    pthread_t hilo_corto;

    if(pthread_create(&hilo_corto,NULL, planificador_corto_plazo,NULL) != 0){

        log_error(kernel_logger,"No se pudo crear el hilo del planificador de largo plazo. ");
        exit(EXIT_FAILURE);
    }
    
    pthread_detach(hilo_corto);
    return;
}


void planificador_corto_plazo(){

    switch (algoritmo){
    case FIFO:
        atender_fifo();
        break;
    case PRIORIDADES:
        atender_prioridades();
        break;
    case COLAS_MULTINIVEL:
        atender_cmn();
        break;    
    
    default:
        log_error(kernel_logger, "ALGORITMO DE CORTO PLAZO DESCONOCIDO");
		exit(EXIT_FAILURE);
        break;
    }
}

void atender_fifo(){ // Pone al TCB que corresponde en exec, busca en 1 cola de ready. Le pasa a CPU el TCB y cpu devuelve TID y motivo. 
    
    while(1){
        pthread_mutex_lock(&mutex_lista_exec_hilo);

        TCB* un_tcb = malloc(sizeof(TCB));

        if(list_is_empty(lista_exec_hilo)){// si la lista en ejecucion esta vacía

            pthread_mutex_lock(&mutex_cola_ready_fifo_prioridades);                      
            if (! queue_is_empty(cola_ready_fifo_prioridades)){// si hay hilos en ready 
            un_tcb = queue_peek(cola_ready_fifo_prioridades);
            queue_pop(cola_ready_fifo_prioridades);
            pthread_mutex_unlock(&mutex_cola_ready_fifo_prioridades);

            pthread_mutex_lock(&mutex_kernel_log_counter);
            log_info(kernel_logger, "Planifica el hilo %d, cuyo pid es %d", un_tcb->TID, un_tcb->PID);
            kernel_log_counter++;
            pthread_mutex_unlock(&mutex_kernel_log_counter);
            
            if(un_tcb != NULL){
                list_add(lista_exec_hilo, un_tcb);
                un_tcb -> estado = EXEC;

                enviarTCBCPU(un_tcb); //DESARROLLADO

                interpretarCPU(); //FALTA QUE LO COMPLETES SABRI
            }
            
            } 
            else{
                log_warning(kernel_logger, "Cola de READY vacía");
            }  
        }
        pthread_mutex_unlock(&mutex_lista_exec_hilo);
    }
    
    
}

void atender_prioridades(){  // Pone al TCB que corresponde en exec, busca en 1 cola de ready. Le pasa a CPU el TCB y cpu devuelve TID y motivo. 
    
    while(1){
        pthread_mutex_lock(&mutex_lista_exec_hilo);
    
    
        if(list_is_empty(lista_exec_hilo)){

            
            TCB* un_tcb = malloc(sizeof(TCB));


            pthread_mutex_lock(&mutex_cola_ready_fifo_prioridades);
            if(! queue_is_empty(cola_ready_fifo_prioridades)){
                if(queue_size(cola_ready_fifo_prioridades)==1){
                    un_tcb = queue_peek(cola_ready_fifo_prioridades);
                    queue_pop(cola_ready_fifo_prioridades);
                }
                else{
                un_tcb = elegir_maxima_prioridad(cola_ready_fifo_prioridades);
                }
            pthread_mutex_unlock(&mutex_cola_ready_fifo_prioridades);

            pthread_mutex_lock(&mutex_kernel_log_counter);
            log_info(kernel_logger, "Planifica el hilo %d, cuyo pid es %d", un_tcb->TID, un_tcb->PID);
            kernel_log_counter++;
            pthread_mutex_unlock(&mutex_kernel_log_counter);

            
            if(un_tcb != NULL){
                list_add(lista_exec_hilo, un_tcb);
                un_tcb -> estado = EXEC;

                enviarTCBCPU(un_tcb);//DEFINIDO EN MAIN
                interpretarCPU();//DEFINIDO EN MAIN FALTA QUE LO COMPLETES SABRI O NICO
            }
            }
            else{
                log_warning(kernel_logger, "Cola de READY vacía");
            }
            
        }
        pthread_mutex_unlock(&mutex_lista_exec_hilo);
    }
    
}

void atender_cmn() {
    pthread_mutex_lock(&mutex_lista_exec_hilo);

        for (int i = 0; i < MAX_NIVELES_PRIORIDAD; i++) {
            TCB* thread = get_next_thread(i); //Busca el siguiente hilo segun RR (el 1ro de la cola)
            if (thread != NULL) {

                ejecutar_hilo(thread); //Pasa hilo a exec

                // Si el hilo no ha terminado, se vuelve a agregar al final de la cola (Round Robin)
                if (thread->estado == BLOCKED || thread->estado == READY) {
                    if (!insertar_en_cola_ready_cmn(thread)) {
                        log_error(kernel_logger,"Error al volver a insertar el hilo TID %d en la cola", thread->TID);
                    }
                }
            }

    }
    pthread_mutex_unlock(&mutex_lista_exec_hilo);
}

void ejecutar_hilo(TCB* un_tcb) {
        if(un_tcb != NULL){
            list_add(lista_exec_hilo, un_tcb);
            un_tcb -> estado = EXEC;

            enviarTCBCPU(un_tcb); 

            interpretarCPU(); 
        }

}

TCB* get_next_thread(int prioridad) {
    t_queue* queue = colas_ready_cmn.colas_prioridad[prioridad];
    if (queue_is_empty(queue)) {
        return NULL;  // No hay hilos en la cola
    }
    
    // Obtener el siguiente hilo en la cola (Round Robin) - fifo
    TCB* hilo = queue_pop(queue);
    return hilo;
}

TCB* elegir_maxima_prioridad(t_queue* cola_ready_fifo_prioridades) { // elige que TCB se ejecuta en prioridades
    uint32_t cola_size = queue_size(cola_ready_fifo_prioridades);
    TCB* tcb_actual;
    TCB* elegido;
    bool inicio = false;

    for (uint32_t i = 1; i < cola_size; i++) {

        tcb_actual = queue_peek(cola_ready_fifo_prioridades);
        queue_pop(cola_ready_fifo_prioridades);

        if(inicio){
            elegido = queue_peek(cola_ready_fifo_prioridades);
            queue_pop(cola_ready_fifo_prioridades);
        }  

        if (tcb_actual->prioridad < elegido->prioridad) { 
            
            queue_push(cola_ready_fifo_prioridades,tcb_actual);
        }else{

            queue_push(cola_ready_fifo_prioridades,elegido);
            elegido = tcb_actual;
        }
    }

    return elegido;
}

// Función para comparar TCBs
bool tcb_equals(TCB* tcb1, TCB* tcb2) {
    return tcb1->PID == tcb2->PID && tcb1->TID == tcb2->TID;
}

// Implementación de queue_remove
void queue_remove(t_queue* cola, TCB* un_tcb) {
    if (cola == NULL || un_tcb == NULL) {
        return; // Validación de entrada
    }

    t_queue* temp_queue = queue_create(); // Cola temporal para elementos que no se eliminan

    // Procesar la cola original
    while (!queue_is_empty(cola)) {
        TCB* tcb_actual = queue_pop(cola);
        if (tcb_equals(tcb_actual, un_tcb)) {
            free(tcb_actual); // Liberar memoria del TCB eliminado
        } else {
            queue_push(temp_queue, tcb_actual); // Mantener los elementos no eliminados
        }
    }

    // Restaurar elementos en la cola original
    while (!queue_is_empty(temp_queue)) {
        queue_push(cola, queue_pop(temp_queue));
    }

    queue_destroy(temp_queue); // Liberar la cola temporal
}
