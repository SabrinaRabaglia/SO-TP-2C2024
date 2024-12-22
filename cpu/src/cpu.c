#include "../utils_C/main.h"

t_buffer* bufferRetorno;

t_buffer* EjecutarProceso(void *args, int* codigo){

    cicloParams *params = (cicloParams*)args;
    TCB *p = params->tcb;
    Registros *registros = params->registros;
    bufferRetorno = buffer_create(0);
    int socketMemoria = params->socketMemoria;
    int socketCliente = params->socketCliente;
    bool *interrupcionRecibida = params->interrupcionRecibida;
    bool interrupcionInterna = false;


    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Iniciando ciclo de instruccion");
    pthread_mutex_unlock(&mutex_logger);


    while (!(interrupcionInterna || *interrupcionRecibida)) {

        char* inst = fetchInstruccion(socketMemoria, p, registros);
        log_info(logger, "Instruccion: %s",inst);
        t_instruccion instruccionDecodificada  = decodeInstruccion(inst, registros);

        interrupcionInterna = executeInstruccion(&instruccionDecodificada, socketCliente, procesoEnEjecucion,threadEnEjecucion,&registros->PC);

        if(!(interrupcionInterna || *interrupcionRecibida))
        {
            registros->PC++;
        }
        free(inst);
    }
    
    return bufferRetorno;
}


char* fetchInstruccion(int socketMemoria, TCB *p, Registros* Registros){
    
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "PID: %d - FETCH - Program Counter: %d", p->PID,Registros->PC);
    pthread_mutex_unlock(&mutex_logger);

    t_buffer *bufferInstruccion = solicitar_instruccion(p->PID,p->TID,Registros->PC);
    if(bufferInstruccion->stream == NULL)
    {
        pthread_mutex_lock(&mutex_logger);
        log_error(logger, "Error al solicitar instruccion");
        pthread_mutex_unlock(&mutex_logger);
        return NULL;
    }
    else
    {
        char *instruccionRecibida = DeserializarString(bufferInstruccion);
        buffer_destroy(bufferInstruccion);
        return instruccionRecibida;
    }
    buffer_destroy(bufferInstruccion);
}

void inicializarInstruccion(t_instruccion *instruccion){
    memset(instruccion, 0, sizeof(t_instruccion));
    instruccion->registro2 = NULL;
    instruccion->registro3 = NULL;
	instruccion->valor = 0;
    instruccion->recurso = NULL;
	instruccion->interfaz = NULL;
	instruccion->archivo = NULL;
}

uint32_t * obtenerRegistro(Registros* registros, char* token) { /*listo*/

    uint32_t * reg_ptr;
    reg_ptr = NULL;

    if (strcmp(token, "PC") == 0) {
        reg_ptr = &registros->PC;

    }else if(strcmp(token, "AX") == 0){
        reg_ptr = &registros->AX;

    }else if (strcmp(token, "BX") == 0) {
        reg_ptr = &registros->BX;
        
    } else if (strcmp(token, "CX") == 0) {
        reg_ptr = &registros->CX;
        
    } else if (strcmp(token, "DX") == 0) {
        reg_ptr = &registros->DX;
        
    } else if (strcmp(token, "EX") == 0) {
        reg_ptr = &registros->EX;
        
    } else if (strcmp(token, "FX") == 0) {
        reg_ptr = &registros->FX;
        
    } else if (strcmp(token, "GX") == 0) {
        reg_ptr = &registros->GX;
        
    } else if (strcmp(token, "HX") == 0) {
        reg_ptr = &registros->HX;
        
    } else if (strcmp(token, "BASE") == 0) {
        reg_ptr = &registros->BASE;
        
    } else if (strcmp(token, "LIMITE") == 0) {
        reg_ptr = &registros->LIMITE;
        
    }

    return reg_ptr;
} /*listo*/

instruccion obtenerInstruccion(char *token){//Listo
    static instruccion SET_INST = SET;
    static instruccion READ_MEM_INST = READ_MEM;
    static instruccion WRITE_MEM_INST = WRITE_MEM;
    static instruccion SUM_INST = SUM;
    static instruccion SUB_INST = SUB;
    static instruccion JNZ_INST = JNZ;
    static instruccion LOG_INST = LOG;

    //syscalls

    static instruccion DUMP_MEMORY_INST = DUMP_MEMORY;
    static instruccion IO_INST = IO;
    static instruccion PROCESS_CREATE_INST = PROCESS_CREATE;
    static instruccion THREAD_CREATE_INST = THREAD_CREATE;
    static instruccion THREAD_JOIN_INST = THREAD_JOIN;
    static instruccion THREAD_CANCEL_INST = THREAD_CANCEL;
    static instruccion MUTEX_CREATE_INST = MUTEX_CREATE;
    static instruccion MUTEX_LOCK_INST = MUTEX_LOCK;
    static instruccion MUTEX_UNLOCK_INST = MUTEX_UNLOCK;
    static instruccion THREAD_EXIT_INST = THREAD_EXIT;
    static instruccion PROCESS_EXIT_INST = PROCESS_EXIT;


    //VIEJOS 
    static instruccion RESIZE_INST = RESIZE;
    static instruccion COPY_STRING_INST = COPY_STRING;
    static instruccion WAIT_INST = WAIT;

    //INVALIDO
    
    static instruccion INVALID_INSTRUCTION_INST = INVALID_INSTRUCTION;

    if (strcmp(token, "SET") == 0) return SET_INST;
    if (strcmp(token, "READ_MEM") == 0) return READ_MEM_INST;
    if (strcmp(token, "WRITE_MEM") == 0) return WRITE_MEM_INST;
    if (strcmp(token, "SUM") == 0) return SUM_INST;
    if (strcmp(token, "SUB") == 0) return SUB_INST;
    if (strcmp(token, "JNZ") == 0) return JNZ_INST;
    if (strcmp(token, "LOG") == 0) return LOG_INST;

    // Syscalls
    if (strcmp(token, "DUMP_MEMORY") == 0) return DUMP_MEMORY_INST;
    if (strcmp(token, "IO") == 0) return IO_INST;
    if (strcmp(token, "PROCESS_CREATE") == 0) return PROCESS_CREATE_INST;
    if (strcmp(token, "THREAD_CREATE") == 0) return THREAD_CREATE_INST;
    if (strcmp(token, "THREAD_JOIN") == 0) return THREAD_JOIN_INST;
    if (strcmp(token, "THREAD_CANCEL") == 0) return THREAD_CANCEL_INST;
    if (strcmp(token, "MUTEX_CREATE") == 0) return MUTEX_CREATE_INST;
    if (strcmp(token, "MUTEX_LOCK") == 0) return MUTEX_LOCK_INST;
    if (strcmp(token, "MUTEX_UNLOCK") == 0) return MUTEX_UNLOCK_INST;
    if (strcmp(token, "THREAD_EXIT") == 0) return THREAD_EXIT_INST;
    if (strcmp(token, "PROCESS_EXIT") == 0) return PROCESS_EXIT_INST;


    //VIEJO
    if (strcmp(token, "RESIZE") == 0) return RESIZE_INST;
    if (strcmp(token, "COPY_STRING") == 0) return COPY_STRING_INST;
    if (strcmp(token, "WAIT") == 0) return WAIT_INST;

    return INVALID_INSTRUCTION_INST;
}

t_instruccion parsearInstruccion(char* instruccion, Registros *registros) {//modificando
    t_instruccion instruccionParseada;
    inicializarInstruccion(&instruccionParseada);

    char *token = strtok(instruccion, " ");
    if (token != NULL) {
        instruccionParseada.instruccion = obtenerInstruccion(token);
        int operandoCount = 0;

        switch(instruccionParseada.instruccion) {
            case SET:
                while ((token = strtok(NULL, " ")) != NULL) {
                    operandoCount++;
                    switch (operandoCount) {
                        case 1: {
                            instruccionParseada.registro1 = obtenerRegistro(registros, token);
                            break;
                        }
                        case 2:
                            instruccionParseada.valor = (uint32_t)atoi(token);
                            break;
                        default:
                            break;
                    }
                }
                break;
            case JNZ:
                while ((token = strtok(NULL, " ")) != NULL) {
                    operandoCount++;
                    switch (operandoCount) {
                        case 1: {
                            instruccionParseada.registro1 = obtenerRegistro(registros, token);
                            break;
                        }
                        case 2:
                            instruccionParseada.valor = (uint32_t)atoi(token);
                            break;
                        default:
                            break;
                    }
                }
                instruccionParseada.registro2 = (uint32_t*)obtenerRegistro(registros, "PC");
                break;
            case READ_MEM:
                while ((token = strtok(NULL, " ")) != NULL) {
                    operandoCount++;
                    switch (operandoCount) {
                        case 1: {
                            instruccionParseada.registro1 = obtenerRegistro(registros, token);
                            break;
                        }
                        case 2: {
                            instruccionParseada.registro2 = obtenerRegistro(registros, token);
                            break;
                        }
                    }
                }
                break;
            case WRITE_MEM:
                while ((token = strtok(NULL, " ")) != NULL) {
                    operandoCount++;
                    switch (operandoCount) {
                        case 1: {
                            instruccionParseada.registro1 = obtenerRegistro(registros, token);
                            break;
                        }
                        case 2: {
                            instruccionParseada.registro2 = obtenerRegistro(registros, token);
                            break;
                        }
                    }
                }
                break;
            case SUM:
            case SUB:
                while ((token = strtok(NULL, " ")) != NULL) {
                    operandoCount++;
                    switch (operandoCount) {
                        case 1: {
                            instruccionParseada.registro1 = obtenerRegistro(registros, token);
                            break;
                        }
                        case 2: {
                            instruccionParseada.registro2 = obtenerRegistro(registros, token);
                            break;
                        }
                        default:
                            break;
                    }
                }
                break;
            case LOG:
                while ((token = strtok(NULL, " ")) != NULL) {
            
                    instruccionParseada.valor = (uint32_t)atoi(token);
                    
                }break;

            case DUMP_MEMORY://no tiene atributos
                break;
            
            case IO:
                while ((token = strtok(NULL, " ")) != NULL) {
            
                    instruccionParseada.valor = (uint32_t)atoi(token);
                    
                }break;
            case PROCESS_CREATE:
                while ((token = strtok(NULL, " ")) != NULL) {
                    operandoCount++;
                    switch (operandoCount) {
                        case 1: {
                            instruccionParseada.archivo = strdup(token);
                            break;
                        }
                        case 2: {
                            instruccionParseada.registro1 = obtenerRegistro(registros, token);
                            break;
                        }
                        case 3:{
                            instruccionParseada.registro2 = obtenerRegistro(registros, token);
                            break;
                        }
                        default:
                            break;
                    }
                }
                break;
            case THREAD_CREATE:
                while ((token = strtok(NULL, " ")) != NULL) {
                    operandoCount++;
                    switch (operandoCount) {
                        case 1: {
                            instruccionParseada.archivo = strdup(token);
                            break;
                        }
                        case 2: {
                            instruccionParseada.registro1 = obtenerRegistro(registros, token);
                            break;
                        }
                        default:
                            break;
                    }
                }
                break;
            case THREAD_JOIN:
            case THREAD_CANCEL:
                while ((token = strtok(NULL, " ")) != NULL) {
            
                    instruccionParseada.valor = (uint32_t)atoi(token);
                    
                }break;
                
            case MUTEX_CREATE:
            case MUTEX_LOCK:
            case MUTEX_UNLOCK:
                while ((token = strtok(NULL, " ")) != NULL) {
            
                    instruccionParseada.recurso = strdup(token);
                    
                }break;

            case THREAD_EXIT:
            case PROCESS_EXIT:
                break;
            
            default:
                break;
        }
    }
    return instruccionParseada;
}

t_instruccion decodeInstruccion(char* instruccion, Registros *registros){
    return parsearInstruccion(instruccion, registros);
}

bool executeInstruccion(t_instruccion *instruccion, int socketCliente, int procesoEnEjecucion, int threadEnEjecucion,uint32_t* pc){
    
    bool interrupcionInterna = false;
    switch (instruccion->instruccion) {
        case SET:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d - Ejecutando: SET - %p %u", procesoEnEjecucion, instruccion->registro1, instruccion->valor);
            pthread_mutex_unlock(&mutex_logger);
            
            *(instruccion->registro1) = instruccion->valor;

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,SET);

            break;
        case READ_MEM:
            //registro2 direccion

            void* direccion = obtener_direccion_fisica(*(uint32_t*)(instruccion->registro2),procesoEnEjecucion,threadEnEjecucion);

            t_buffer* buffer = leer_en_memoria(&direccion);
            uint32_t value32 = *(uint32_t*) buffer_read(buffer,sizeof(uint32_t));
            
            log_info(logger,"PID: %d TID: %d- accion: LEER - direccion Fisica: %p - valor: %d",procesoEnEjecucion,threadEnEjecucion,(void*) direccion,value32);
            memcpy(instruccion->registro1,&value32,sizeof(value32));

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,READ_MEM);
            break;
        
        case WRITE_MEM:
            //registro1 direccion
            int errorCode;

            void* dir = obtener_direccion_fisica(*(uint32_t*)(instruccion->registro1),procesoEnEjecucion,threadEnEjecucion);

            uint32_t value_32 = *(uint32_t*)(instruccion->registro2);
            log_info(logger,"PID: %d TID: %d- accion: ESCRIBIR - direccion Fisica: %p - valor: %d",procesoEnEjecucion,threadEnEjecucion,(void*) dir,value32);
            errorCode = escribir_en_memoria(&dir, &value_32);


            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,SUM);
            if(errorCode < 0)
            {
                buffer_add_int(bufferRetorno,OP_OUT_OF_MEMORY);
                interrupcionInterna = true;
            }else{
                buffer_add_int(bufferRetorno,0);
            }
            break;
        

        case SUM:

            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d- Ejecutando: SUM - %p %p", procesoEnEjecucion,threadEnEjecucion, instruccion->registro1, instruccion->registro2);
            pthread_mutex_unlock(&mutex_logger);

            *(instruccion->registro1) += *(instruccion->registro2);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,SUM);
            break;

        case SUB:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d- Ejecutando: SUB - %p %p", procesoEnEjecucion,threadEnEjecucion, instruccion->registro1, instruccion->registro2);
            pthread_mutex_unlock(&mutex_logger);

            if (instruccion->registro1 != NULL && instruccion->registro2 != NULL) {
                *(instruccion->registro1) -= *(instruccion->registro2);
            }

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,SUB);
            break;

        case JNZ:

            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d- Ejecutando: JNZ - %u", procesoEnEjecucion,threadEnEjecucion, instruccion->valor);
            pthread_mutex_unlock(&mutex_logger);

            if (*(instruccion->registro1) != 0) {
                //*(instruccion->registro2) = instruccion->valor;
                *pc = instruccion->valor;
            }

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,JNZ);
            break;

        case LOG:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "Valor del registro: %d", instruccion->valor);
            pthread_mutex_unlock(&mutex_logger);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,LOG);
            break;

        case DUMP_MEMORY:

            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d- Ejecutando: DUMP MEMORY", procesoEnEjecucion,threadEnEjecucion);
            pthread_mutex_unlock(&mutex_logger);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,DUMP_MEMORY);

            interrupcionInterna = true;
            break;

        case IO:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: IO - %d",procesoEnEjecucion,threadEnEjecucion, instruccion->valor);
            pthread_mutex_unlock(&mutex_logger);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,IO);
            buffer_add_uint32(bufferRetorno,instruccion->valor);  

            interrupcionInterna = true;
            break;

        case PROCESS_CREATE:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: PROCESS_CREATE - ARCHIVO: %s PRIORIDAD T0: %d",procesoEnEjecucion,threadEnEjecucion, instruccion->archivo,*(uint32_t*) instruccion->registro1);
            pthread_mutex_unlock(&mutex_logger);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,PROCESS_CREATE);
            buffer_add_string(bufferRetorno,instruccion->archivo);
            buffer_add_uint32(bufferRetorno,*instruccion->registro1);
            buffer_add_uint32(bufferRetorno,*instruccion->registro2);

            interrupcionInterna = true;
            break;
        case THREAD_CREATE:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: THREAD_CREATE - ARCHIVO: %s TAMANIO: %d PRIORIDAD T0: %d",procesoEnEjecucion,threadEnEjecucion, instruccion->archivo,*(uint32_t*) instruccion->registro1,*(uint32_t*) instruccion->registro2);
            pthread_mutex_unlock(&mutex_logger);
            
            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,THREAD_CREATE);
            buffer_add_string(bufferRetorno,instruccion->archivo);
            buffer_add_uint32(bufferRetorno,*instruccion->registro1);

            interrupcionInterna = true;
            break;

        case THREAD_JOIN:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: THREAD_JOIN - TID: %d",procesoEnEjecucion,threadEnEjecucion, instruccion->valor);
            pthread_mutex_unlock(&mutex_logger);
            
            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,THREAD_JOIN);
            buffer_add_uint32(bufferRetorno,instruccion->valor);

            interrupcionInterna = true;
            break;
        case THREAD_CANCEL:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: THREAD_CANCEL - TID: %d",procesoEnEjecucion,threadEnEjecucion, instruccion->valor);
            pthread_mutex_unlock(&mutex_logger);
            
            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,THREAD_CANCEL);
            buffer_add_uint32(bufferRetorno,instruccion->valor);

            interrupcionInterna = true;
            break;
        case MUTEX_CREATE:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: MUTEX_CREATE - %s",procesoEnEjecucion,threadEnEjecucion, instruccion->recurso);
            pthread_mutex_unlock(&mutex_logger);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,MUTEX_CREATE);
            buffer_add_string(bufferRetorno,instruccion->recurso);
            
            interrupcionInterna = true;
            break;
        case MUTEX_LOCK:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: MUTEX_LOCK - %s",procesoEnEjecucion,threadEnEjecucion, instruccion->recurso);
            pthread_mutex_unlock(&mutex_logger);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,MUTEX_LOCK);
            buffer_add_string(bufferRetorno,instruccion->recurso);

            interrupcionInterna = true;
            break;
        case MUTEX_UNLOCK:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d TID: %d - Ejecutando: MUTEX_UNLOCK - %s",procesoEnEjecucion,threadEnEjecucion, instruccion->recurso);
            pthread_mutex_unlock(&mutex_logger);

            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,MUTEX_UNLOCK);
            buffer_add_string(bufferRetorno,instruccion->recurso);

            interrupcionInterna = true;
            break;

       case PROCESS_EXIT:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d- TID: %d - Ejecutando: %d", procesoEnEjecucion, instruccion->instruccion);
            pthread_mutex_unlock(&mutex_logger);
            
            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,PROCESS_EXIT);

            interrupcionInterna = true;
            break;
        case THREAD_EXIT:
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "PID: %d - TID: %d - Ejecutando: %u", procesoEnEjecucion, threadEnEjecucion, instruccion->instruccion);
            pthread_mutex_unlock(&mutex_logger);
            
            buffer_add_uint32(bufferRetorno,procesoEnEjecucion);
            buffer_add_uint32(bufferRetorno,threadEnEjecucion);
            buffer_add_int(bufferRetorno,THREAD_EXIT);

            interrupcionInterna = true;
        break;
            
        default:
            pthread_mutex_lock(&mutex_logger);
            log_warning(logger, "PID: %d - TID: %d - Instrucción desconocida: %u", procesoEnEjecucion, threadEnEjecucion, instruccion->instruccion);
            pthread_mutex_unlock(&mutex_logger);
            break;
    }
    return interrupcionInterna;
}