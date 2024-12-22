#include "main.h"

t_memoria_config *memoria_config;

t_config *config;

void *memoria_usuario;

t_list *lista_particiones;

int fd_server_memoria;

t_dictionary *procesos;

int main(int argc, char *argv[]) {


    memoria_config = cargar_memoria_config("memoria.config");

    logger = iniciar_logger("memoria.log", "MEMORIA", 1, obtener_log_level(memoria_config->log_level));

    inicializar_memoria();

    //creo el server para que se conecten
    fd_server_memoria = getSocketServidor(memoria_config->puerto_escucha);

    gestionar_cpu();
    
    gestionar_kernel();

    //**FALTA FILE SYSTEM**

    //liberar memoria
    log_destroy(logger);
    
    config_destroy(config);

    list_destroy_and_destroy_elements(lista_particiones, (void*)free);

    free(memoria_config);

    free(memoria_usuario);

    return EXIT_SUCCESS;
}

void inicializar_memoria() {

    //Espacio de memoria de usuario
    memoria_usuario = malloc(memoria_config->tam_total_memoria);
	
	if(memoria_usuario == NULL) {
		
		log_info(logger, "No se pudo crear la memoria de usuario");
		
		exit(EXIT_FAILURE);
	}

    log_info(logger, "Memoria de usuario creada con exito");

    //Creo las particones
    gestionar_particiones();

    return;
}

t_memoria_config *cargar_memoria_config(String nombre_archivo) {


    config = iniciar_config(nombre_archivo);

        if (config == NULL) {
        fprintf(stderr, "Config invalido!\n");
        exit(EXIT_FAILURE);
    }

    t_memoria_config* memoria_config = malloc(sizeof(t_memoria_config));
    
    if (memoria_config == NULL) {
        perror("Error en malloc()");
        exit(EXIT_FAILURE);
    }

    memoria_config->puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    
    memoria_config->ip_filesystem = config_get_int_value(config,"IP_FILESYSTEM");

    memoria_config->puerto_filesystem = config_get_int_value(config,"PUERTO_FILESYSTEM");

    memoria_config->tam_total_memoria = config_get_int_value(config,"TAM_MEMORIA");
    
    memoria_config->path_instrucciones = config_get_string_value(config,"PATH_INSTRUCCIONES");
    
    memoria_config->retardo_respuesta = config_get_int_value(config,"RETARDO_RESPUESTA");

    memoria_config->esquema = config_get_string_value(config,"ESQUEMA");

    memoria_config->algoritmo_busqueda = config_get_string_value(config,"ALGORITMO_BUSQUEDA");

    memoria_config->lista_particiones = config_get_array_value(config, "PARTICIONES");

    if (memoria_config->lista_particiones == NULL) {
        
        fprintf(stderr, "No se encontraron las particiones\n");
        
        config_destroy(config);
        
        exit(EXIT_FAILURE);
    }

    memoria_config->log_level = config_get_string_value(config,"LOG_LEVEL");


    if (memoria_config->tam_total_memoria <= 0 || memoria_config->retardo_respuesta <= 0) {
        
        fprintf(stderr, "Tamaño de memoria o retardo de respuesta invalido!\n");

        config_destroy(config);

        exit(EXIT_FAILURE);
    }

    return memoria_config;
}

void gestionar_particiones() {

    t_particion *particion;

    lista_particiones = list_create();

    if(lista_particiones == NULL) {

		log_info(logger, "No se pudo crear la lista de particiones");

		exit(EXIT_FAILURE);
	}

    if(strcmp(memoria_config->esquema, "FIJAS") == 0){

        //agrego las particiones

        //int num_elementos = sizeof(memoria_config->lista_particiones) / sizeof(memoria_config->lista_particiones[0]);

        int num_elementos = 0;
        while (memoria_config->lista_particiones[num_elementos] != NULL) {
            num_elementos++;
        }

        log_debug(logger, "Cantidad de particiones: %i", num_elementos);
    
        char *ptr_base_particion = (char*)memoria_usuario;

        for( int i = 0; i < num_elementos; i++) {
            
            particion = malloc(sizeof(t_particion));

            particion->estado = 0; //libre 0, ocupado 1
            particion->tamanio = atoi(memoria_config->lista_particiones[i]);
            particion->base = ptr_base_particion;

            //verifica si esta dentro de memoria usuario
            if(particion->base + atoi(memoria_config->lista_particiones[i]) > (char*)memoria_usuario + memoria_config->tam_total_memoria || particion->base < (char*)memoria_usuario) {

                log_info(logger, "Fuera del rango de memoria, particion: %i de tamanio: %i", i, atoi(memoria_config->lista_particiones[i]));

                exit(EXIT_FAILURE); //seguro me queda memoria colgada aca
            }

            if(list_add(lista_particiones, (void*)particion) == -1) {
                
                log_info(logger, "No se pudo agregar la particion a la lista ");
            }

            ptr_base_particion += atoi(memoria_config->lista_particiones[i]); //"incremento la base"

            log_info(logger, "Particion agregada, tamanio: %i, base: %p, en la pos: %i", particion->tamanio, particion->base, i);	
        }

        return;

    }

    if(strcmp(memoria_config->esquema, "DINAMICAS")) {

        //creo la primer gran particion

        particion = malloc(sizeof(t_particion));

        particion->base = (char*)memoria_usuario;
        particion->estado = 0;
        particion->tamanio = memoria_config->tam_total_memoria;

        if(list_add(lista_particiones, (void*)particion) == -1) {
            
            log_info(logger, "No se pudo agregar la particion a la lista ");
        }

        log_info(logger, "Particion creada, tamanio: %i, base: %p, en la pos: 0", particion->tamanio, particion->base);

        return;
    }

    log_error(logger, "Error al leer el tipo de esquema");

    exit(EXIT_FAILURE);
}


//devuelve un proceso usando la key pid
t_proceso *get_proceso(int pid) {

    char str_pid[8];
    snprintf(str_pid, sizeof(str_pid), "%d", pid);

    t_proceso *proceso = (t_proceso*)dictionary_get(procesos, str_pid);

    if(proceso == NULL) {

        log_error(logger, "No se encontro el proceso de pid: %i", pid);

        return NULL;
    }

    return proceso;
}

t_hilo *get_hilo(int pid, int tid) {

    t_proceso *proceso = get_proceso(pid);

    char str_tid[8];
    snprintf(str_tid, sizeof(str_tid), "%d", tid);

    t_hilo *hilo = (t_hilo*)dictionary_get(proceso->hilos, str_tid);

    if(hilo == NULL) {

        log_error(logger, "No se encontro el hilo de tid: %i, pid: %i", tid, pid);

        return NULL;
    }

    return hilo;
}

bool liberar_proceso(void *ptr_proceso) {

    t_proceso *proceso = (t_proceso*)ptr_proceso;

    if(proceso == NULL) {

        return false;
    }

    free(proceso->archivo_pseudocodigo);

    liberar_memoria(proceso->contexto.base);//verificar

    free(proceso->contexto.base);//libero la memoria asignada

    dictionary_destroy_and_destroy_elements(proceso->hilos, (void*)liberar_hilo);

    //dictionary_remove_and_destroy(procesos, string_itoa(pid), free);

    return true;
}

bool liberar_hilo(void *ptr_hilo){

    t_hilo *hilo = (t_hilo*)ptr_hilo;

    if(hilo == NULL) {
        
        return false;
    
    }
    //t_proceso *proceso = get_proceso(pid);

    //t_hilo *hilo = get_hilo(pid, tid);

    free(hilo->archivo_pseudocodigo);

    list_destroy_and_destroy_elements(hilo->instrucciones, free);//libero memoria asociada a las instrucciones

    //dictionary_remove_and_destroy(proceso, string_itoa(tid), free);

    return true;

}

void liberar_memoria(char *base) {
    int cant_particiones = list_size(lista_particiones);

    // Buscar la partición a liberar
    for (int i = 0; i < cant_particiones; i++) {
        t_particion *particion = (t_particion*)list_get(lista_particiones, i);

        // Verificamos si la base corresponde a la partición a liberar
        if (particion->base == base) {
            particion->estado = 0; // Marca la partición como libre

            // Coalescencia con la partición anterior
            if (i > 0) {
                t_particion *anterior = (t_particion*)list_get(lista_particiones, i - 1);
                if (anterior->estado == 0) {  // Si la partición anterior está libre
                    anterior->tamanio += particion->tamanio;  // Fusionamos las particiones
                    list_remove_and_destroy_element(lista_particiones, i, liberar_particion);  // Eliminamos la partición actual
                    i--;  // Ajustamos el índice después de la eliminación
                    cant_particiones--;  // Reducimos el tamaño de la lista
                }
            }

            // Coalescencia con la partición siguiente
            if (i < cant_particiones) {  // Verificamos si no es la última partición
                t_particion *siguiente = (t_particion*)list_get(lista_particiones, i + 1);
                if (siguiente->estado == 0) {  // Si la partición siguiente está libre
                    particion->tamanio += siguiente->tamanio;  // Fusionamos las particiones
                    list_remove_and_destroy_element(lista_particiones, i + 1, liberar_particion);  // Eliminamos la partición siguiente
                    cant_particiones--;  // Reducimos el tamaño de la lista
                }
            }
            
            return; // Terminamos la función después de liberar la partición
        }
    }

    log_info(logger,"Error: No se encontró la partición con la base %p", base);
}

void liberar_particion(void *arg) {

    t_particion *particion = (t_particion*)arg;

    free(particion->base);

    free(particion);
}

void enviar_estado(int estado, int fd_cliente, int operacion) {

    t_buffer *buffer = buffer_create(sizeof(int));

    buffer_add_int(buffer, estado);

    EnviarOperacion(fd_cliente, operacion, buffer);
}