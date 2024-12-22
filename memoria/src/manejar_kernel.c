#include "manejar_kernel.h"

extern int fd_server_memoria;

extern t_list *lista_particiones;

extern t_dictionary *procesos;

extern t_log *logger;

void gestionar_kernel(void)
{

    pthread_t hilo_peticion;

    while (1)
    {

        log_debug(logger, "Esperando peticion de kernel");

        int fd_cliente = esperar_cliente(fd_server_memoria);

        if (handshake_Server(fd_server_memoria, MEMORIA) == -1)
        {
            perror("Error en handshake");

            exit(EXIT_FAILURE);
        }

        int *ptr_fd_clienete = malloc(sizeof(int));

        *ptr_fd_clienete = fd_cliente;

        if (pthread_create(&hilo_peticion, NULL, manejar_peticion, ptr_fd_clienete) != 0)
        {
            perror("Error al crear el hilo");
        }
        else
        {
            pthread_detach(hilo_peticion); // Descartar el hilo una vez que termine
        }
    }
}

void *manejar_peticion(void *arg)
{

    int fd_cliente = *(int *)arg;

    free(arg);

    t_paquete *paquete;

    paquete = recibir_paquete(fd_cliente);

    int pid, tid, estado;;

    String archivo_pseudocodigo = NULL;

    switch (paquete->op_code)
    {
    case MEMORIA_KERNEL_CREAR_PROCESO:
        
        pid = buffer_read_int(paquete->buffer);

        archivo_pseudocodigo = buffer_read_string(paquete->buffer);
        
        int tam_proceso = buffer_read_int(paquete->buffer);

        estado = crear_proceso(pid, archivo_pseudocodigo, tam_proceso);

        enviar_estado(estado, fd_cliente, MEMORIA_KERNEL_CREAR_PROCESO);

        break;

    case MEMORIA_KERNEL_CREAR_HILO:

        pid = buffer_read_int(paquete->buffer);

        tid = buffer_read_int(paquete->buffer);

        archivo_pseudocodigo = buffer_read_string(paquete->buffer);

        int prioridad = buffer_read_int(paquete->buffer);

        estado = crear_hilo(pid, tid, archivo_pseudocodigo, prioridad);

        enviar_estado(estado, fd_cliente, MEMORIA_KERNEL_CREAR_HILO);

        break;
    
    case MEMORIA_KERNEL_FINALIZAR_PROCESO:

        pid = buffer_read_int(paquete->buffer);

        estado = finalizar_proceso(pid);

        enviar_estado(estado, fd_cliente, MEMORIA_KERNEL_FINALIZAR_PROCESO);

        break;

    case MEMORIA_KERNEL_FINALIZAR_HILO:

        pid = buffer_read_int(paquete->buffer);

        tid = buffer_read_int(paquete->buffer);

        estado = finalizar_hilo(pid, tid);

        enviar_estado(estado, fd_cliente, MEMORIA_KERNEL_FINALIZAR_HILO);

        break;

    case MEMORIA_KERNEL_MEMORY_DUMP:

        pid = buffer_read_int(paquete->buffer);

        tid = buffer_read_int(paquete->buffer);

        estado = memory_dump(pid, tid);

        enviar_estado(estado, fd_cliente, MEMORIA_KERNEL_MEMORY_DUMP);

        break;

    default:

        log_error(logger, "No se reconoce la operacion recibida %i", paquete->op_code);

        break;
    }


    if(paquete != NULL) {
        buffer_destroy(paquete->buffer);
        free(paquete);
    }

    close(fd_cliente);

    return NULL;
}

//particiones fijas
char *first_fit(int tam_proceso) {

    int cant_particiones = list_size(lista_particiones);

    for(int i = 0; i < cant_particiones; i++) {

        t_particion *particion = (t_particion*)list_get(lista_particiones, i);

        if(particion->estado != 0){
            continue;
        }

        if (particion->tamanio >= tam_proceso) {
            particion->estado = 1;
            return particion->base;
        }
    }

    return NULL;
}

//particiones fijas
char *best_fit(int tam_proceso) {

    int cant_particiones = list_size(lista_particiones);

    int mejor_ajuste = -1;
    int espacio_sobrante_minimo = -1;

    for(int i = 0; i < cant_particiones; i++) {

        t_particion *particion = (t_particion*)list_get(lista_particiones, i);
    
        if(particion->estado != 0) {
            continue;
        }

        // Verifica si la partición tiene suficiente espacio para el proceso
        if (particion->tamanio >= tam_proceso) {
            
            int espacio_sobrante = particion->tamanio - tam_proceso;

            // Si es la primera partición que encontramos o si esta partición tiene un sobrante menor
            if (mejor_ajuste == -1 || espacio_sobrante < espacio_sobrante_minimo) {
                mejor_ajuste = i;  // Actualizamos el índice de la mejor partición
                espacio_sobrante_minimo = espacio_sobrante;  // Actualizamos el espacio sobrante
            }
        }
    }

    // Si encontramos una partición adecuada, asignamos el proceso y la marcamos como ocupada
    if (mejor_ajuste != -1) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, mejor_ajuste);
        particion->estado = 1; // Marca la partición como ocupada
        return particion->base; // Retorna la base de la partición
    }

    // Si no encontramos ninguna partición adecuada, retornamos NULL
    return NULL;
}

//particiones fijas
char *worst_fit(int tam_proceso) {
    
    int cant_particiones = list_size(lista_particiones);
    int peor_ajuste = -1;  // Índice de la partición con el peor ajuste (más grande)
    int espacio_sobrante_maximo = -1;  // Espacio sobrante máximo en la peor partición encontrada

    for (int i = 0; i < cant_particiones; i++) {
        
        t_particion *particion = (t_particion*)list_get(lista_particiones, i);

        // Verifica si la partición está libre (estado == 0)
        if (particion->estado != 0) {
            continue; // Si la partición no está libre, seguimos con la siguiente
        }

        // Verifica si la partición tiene suficiente espacio para el proceso
        if (particion->tamanio > tam_proceso) {
            int espacio_sobrante = particion->tamanio - tam_proceso;

            // Si es la primera partición que encontramos o si esta partición tiene un sobrante mayor
            if (peor_ajuste == -1 || espacio_sobrante > espacio_sobrante_maximo) {
                peor_ajuste = i;  // Actualizamos el índice de la peor partición
                espacio_sobrante_maximo = espacio_sobrante;  // Actualizamos el espacio sobrante máximo
            }
        }
    }

    // Si encontramos una partición adecuada, asignamos el proceso y la marcamos como ocupada
    if (peor_ajuste != -1) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, peor_ajuste);
        particion->estado = 1; // Marca la partición como ocupada
        return particion->base; // Retorna la base de la partición
    }

    // Si no encontramos ninguna partición adecuada, retornamos NULL
    return NULL;
}


char *first_fit_din(int tam_proceso) {
    
    int cant_particiones = list_size(lista_particiones);

    for (int i = 0; i < cant_particiones; i++) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, i);

        if (particion->estado == 0 && particion->tamanio >= tam_proceso) {
            // Si sobra espacio, se divide la partición
            if (particion->tamanio > tam_proceso) {
                // Crear nueva partición libre con el sobrante
                t_particion *nueva_particion = malloc(sizeof(t_particion));
                nueva_particion->base = particion->base + tam_proceso;
                nueva_particion->tamanio = particion->tamanio - tam_proceso;
                nueva_particion->estado = 0; // Libre

                // Actualizar la partición original
                particion->tamanio = tam_proceso;

                // Insertar nueva partición en la lista de particiones
                if(list_add(lista_particiones, nueva_particion) < 0) {
                    perror("No se pudo crear la nueva particion");
                    exit(EXIT_FAILURE);
                }
            }

            // Asignar el proceso a la partición
            particion->estado = 1; // Ocupado
            return particion->base; // Retorna la base de la partición
        }
    }
    return NULL; // No se encontró espacio adecuado
}

char *best_fit_din(int tam_proceso) {
    int cant_particiones = list_size(lista_particiones);
    int mejor_ajuste = -1;
    int espacio_sobrante_minimo = -1;

    for (int i = 0; i < cant_particiones; i++) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, i);

        if (particion->estado == 0 && particion->tamanio >= tam_proceso) {
            int espacio_sobrante = particion->tamanio - tam_proceso;

            if (mejor_ajuste == -1 || espacio_sobrante < espacio_sobrante_minimo) {
                mejor_ajuste = i;
                espacio_sobrante_minimo = espacio_sobrante;
            }
        }
    }

    if (mejor_ajuste != -1) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, mejor_ajuste);

        // Dividir si sobra espacio
        if (particion->tamanio > tam_proceso) {
            t_particion *nueva_particion = malloc(sizeof(t_particion));
            nueva_particion->base = particion->base + tam_proceso;
            nueva_particion->tamanio = particion->tamanio - tam_proceso;
            nueva_particion->estado = 0; // Libre
            particion->tamanio = tam_proceso;

            if(list_add(lista_particiones, nueva_particion) < 0) {
                perror("No se pudo crear la nueva particion");
                exit(EXIT_FAILURE);
            }
        }

        particion->estado = 1; // Ocupado
        return particion->base;
    }
    return NULL;
}

char *worst_fit_din(int tam_proceso) {
    int cant_particiones = list_size(lista_particiones);
    int peor_ajuste = -1;
    int espacio_sobrante_maximo = -1;

    for (int i = 0; i < cant_particiones; i++) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, i);

        if (particion->estado == 0 && particion->tamanio >= tam_proceso) {
            int espacio_sobrante = particion->tamanio - tam_proceso;

            if (peor_ajuste == -1 || espacio_sobrante > espacio_sobrante_maximo) {
                peor_ajuste = i;
                espacio_sobrante_maximo = espacio_sobrante;
            }
        }
    }

    if (peor_ajuste != -1) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, peor_ajuste);

        // Dividir si sobra espacio
        if (particion->tamanio > tam_proceso) {
            t_particion *nueva_particion = malloc(sizeof(t_particion));
            nueva_particion->base = particion->base + tam_proceso;
            nueva_particion->tamanio = particion->tamanio - tam_proceso;
            nueva_particion->estado = 0; // Libre
            particion->tamanio = tam_proceso;

            if(list_add(lista_particiones, nueva_particion) < 0) {
                perror("No se pudo crear la nueva particion");
                exit(EXIT_FAILURE);
            }
        }

        particion->estado = 1; // Ocupado
        return particion->base;
    }
    return NULL;
}

bool crear_proceso(int pid, String archivo_pseudocodigo, int tam_proceso) {

    t_proceso *nuevo_proceso = malloc(sizeof(t_proceso));

    if(nuevo_proceso == NULL) {
        
        fprintf(stderr, "No se pudo crear el proceso pid: %i\n", pid);

        return false;
    }

    nuevo_proceso->pid = pid;

    nuevo_proceso->archivo_pseudocodigo = string_duplicate(archivo_pseudocodigo);

    nuevo_proceso->contexto.limite = tam_proceso; 
    
    nuevo_proceso->contexto.base = asignar_memoria(tam_proceso);

    if(nuevo_proceso->contexto.base == NULL) {
        
        fprintf(stderr, "No se puedo asignar memoria al proceso pid: %i\n", pid);

        free(nuevo_proceso->archivo_pseudocodigo);

        free(nuevo_proceso);

        return false;
    }

    nuevo_proceso->hilos = dictionary_create();

    dictionary_put(procesos, string_itoa(pid), (void*)nuevo_proceso);

    return true;
}

bool crear_hilo(int pid, int tid, String archivo_pseudocodigo, int prioridad) {

    t_proceso *proceso = dictionary_get(procesos, string_itoa(pid));
    
    if(proceso == NULL) {
        
        fprintf(stderr, "No se encontro el proceso pid: %i\n", pid);
        
        return false;
    }

    t_hilo *nuevo_hilo = malloc(sizeof(t_hilo));

    nuevo_hilo->tid = tid;

    nuevo_hilo->archivo_pseudocodigo = string_duplicate(archivo_pseudocodigo);

    nuevo_hilo->prioridad = prioridad;

    inicializar_registros(&nuevo_hilo->registro);

    nuevo_hilo->instrucciones = obtener_instrucciones(archivo_pseudocodigo);

    if(nuevo_hilo->instrucciones == NULL) {

        fprintf(stderr, "No se pudo leer el archivo de pseudocodigo: %s, del hilo: %i, del proceso: %i\n", archivo_pseudocodigo, tid, pid);

        free(nuevo_hilo->archivo_pseudocodigo);

        free(nuevo_hilo);

        return false;
    }

    dictionary_put(proceso->hilos, string_itoa(tid), (void*)nuevo_hilo);

    return true;
}

bool finalizar_proceso(int pid) {

    t_proceso *ptr_proceso = get_proceso(pid);

    if(ptr_proceso == NULL) {

        fprintf(stderr, "No se encontro el proceso pid: %i\n", pid);

        return false;
    }
    
    if (liberar_proceso((void*)ptr_proceso) == false) {
        
        fprintf(stderr, "Error al liberar el proceso pid: %i\n", pid);
        
        return false;
    }

    char *str_pid = string_itoa(pid);

    dictionary_remove_and_destroy(procesos, str_pid, free);

    free(str_pid);

    return true;
}

bool finalizar_hilo(int pid, int tid) {

    t_hilo *hilo = get_hilo(pid, tid);
    
    if(hilo == NULL) {
        
        fprintf(stderr, "No se encontro el hilo, tid: %i pid: %i\n", tid, pid);

        return false;
    }

    if(liberar_hilo((void*)hilo) == false){
        
        fprintf(stderr, "Error al liberar el hilo, tid: %i pid: %i\n", tid, pid);
        
        return false;

    }

    t_proceso *proceso = get_proceso(pid);

    char *str_tid = string_itoa(tid);

    dictionary_remove_and_destroy(proceso->hilos, str_tid, free);

    free(str_tid);

    return true;

}

bool memory_dump(int pid, int tid) {

    //terminar

    return true;
}

t_list *obtener_instrucciones(String filename) {

    FILE* pseudocodigo = fopen(filename, "r");

    if (pseudocodigo == NULL) {

        fprintf(stderr, "Error al abrir el archivo de pseudocodigo! %s\n", filename);

        return NULL;
    }

    char linea[BUFF_SIZE];
    t_list* instrucciones = list_create();

    while (fgets(linea, sizeof(linea), pseudocodigo)) {

        if (linea[0] == '\n') // Ignora las lineas del archivo con solo salto de linea
           continue;

        linea[strcspn(linea, "\n")] = '\0';  // Elimina el salto de línea al final de la instrucción
        String instruccion = strdup(linea);  // Copia la linea leida en un string nuevo

        if (instruccion == NULL) {
            list_destroy_and_destroy_elements(instrucciones, free);
            return NULL;
        }

        list_add(instrucciones, instruccion); // Agrega la instrucción a la lista de instrucciones
    }

    fclose(pseudocodigo);
    return instrucciones;
}

char *asignar_memoria(int tam_proceso) {

    char *base = NULL;

    //terminar

    return base; 
}

// Inicializa todos los registros en cero
void inicializar_registros(t_registros_hilo *registros)
{
    registros->ax = 0;
    registros->bx = 0;
    registros->cx = 0;
    registros->dx = 0;
    registros->ex = 0;
    registros->fx = 0;
    registros->gx = 0;
    registros->hx = 0;
    registros->pc = 0;
}