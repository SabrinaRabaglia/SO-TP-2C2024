//main.c
#include <../include/main.h>

Algoritmo algoritmo;
static uint32_t buscar_PID;
static uint32_t buscar_TID;

bool desalojado;
t_list * lista_exec_hilo;
t_list * lista_bloq_hilo;
t_list * lista_dependencias;//hilos dependientes
t_list* lista_thread_id;

int socket_dispatch;
int socket_interrupt;
int socket_memoria;
int process_id;

t_log * kernel_logger;
t_config* config;
KERNEL_config kernel_config;
ColasReady colas_ready_cmn;
int kernel_log_counter;

pthread_mutex_t mutex_kernel_log_counter;
pthread_mutex_t mutex_cola_new;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_lista_thread_id; 
pthread_mutex_t process_id_mutex;
pthread_mutex_t thread_id_mutex;
pthread_mutex_t mutex_config;
pthread_mutex_t mutex_lista_exec_hilo;
pthread_mutex_t mutex_cola_ready_fifo_prioridades;
pthread_mutex_t mutex_colas_ready_cmn;

int main(int argc, char **argv[])
{
    // Validar cantidad de argumentos
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [archivo_pseudocodigo] [tamanio_proceso] [...args]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Obtener parámetros de entrada
    const char* archivo_pseudocodigo = argv[1];
    int tamanio_proceso = atoi(argv[2]); // Convertir tamaño del proceso a entero


    sem_init(&sem_nuevoProceso, 0, 0);

    // Iniciar archivos
    iniciarLogger();
    config = config_create("kernel.config");
    leerConfig();
    inicializar_colas();
    iniciador();

    algoritmo = buscarAlgoritmo(kernel_config.algoritmo_planificacion);

    // iniciar conexion CPU
    conectarCPU();

    planificador_largo_plazo();//crear hilo para largo plazo
    planificador_corto_plazo();//crear hilo para corto plazo

    
    // Crear proceso inicial:
    crearProcesoInicial(archivo_pseudocodigo, tamanio_proceso); // char* y int

    destruirEstructuras();
    

    return 0;
}


//----------------OTROS----------------------------
void iniciarLogger()
{

    kernel_logger = log_create("kernel.log", "KERNEL", 1, LOG_LEVEL_INFO);

    if (kernel_logger == NULL)
    {
        printf("No se encontro el log\n");
        log_destroy(kernel_logger);
        abort();
    }
}

void iniciarConfig()
{
    config = config_create("kernel.config");
}

void leerConfig()
{
    if (config == NULL)
    {
        pthread_mutex_lock(&mutex_config);
        log_error(kernel_logger, "Config no inicializada.\n");
        abort();
    }

    log_info(kernel_logger, "Leyendo config.\n");

    kernel_config.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    kernel_config.puertoMemoria = config_get_string_value(config, "PUERTO_MEMORIA");
    kernel_config.ip_cpu = config_get_string_value(config, "IP_CPU");
    kernel_config.puerto_cpu_dispatch = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    kernel_config.puerto_cpu_interrupt = config_get_string_value(config, "PUERTO_CPU_INTERRUPT");
    kernel_config.algoritmo_planificacion = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    kernel_config.quantum = config_get_int_value(config, "QUANTUM");
    kernel_config.log_level = config_get_string_value(config, "LOG_LEVEL");
}

void conectarCPU()
{

    log_info(kernel_logger, "Conectando con CPU.\n");

    socket_dispatch = conectar(kernel_config.ip_cpu, kernel_config.puerto_cpu_dispatch, KERNEL);

    socket_interrupt = conectar(kernel_config.ip_cpu, kernel_config.puerto_cpu_interrupt, KERNEL);

    if (socket_dispatch < 0 || socket_interrupt < 0)
    {
        log_error(kernel_logger, "Error al conectar con CPU.\n");
        abort();
    }

    log_info(kernel_logger, "Conexión establecida con CPU en dispatch y interrupt.\n");
}

void conectarMemoria()
{
    log_info(kernel_logger, "Conectando con Memoria.\n");

    socket_memoria = conectar(kernel_config.ip_memoria, kernel_config.puertoMemoria, KERNEL);

    if (socket_memoria < 0)
    {
        log_error(kernel_logger, "Error al conectar con Memoria.\n");
        abort();
    }

    log_info(kernel_logger, "Conexión establecida con Memoria.\n");
}

void inicializar_colas()
{
    cola_new = queue_create();
    cola_ready = queue_create();
    cola_exec = queue_create();
    cola_blocked = queue_create();
    cola_exit = queue_create();
}

void destruir_pcb(PCB *pcb)
{
    list_destroy(pcb->TIDs);
    list_destroy(pcb->mutex);
    free(pcb);
}

void destruir_tcb(TCB *tcb)
{
    free(tcb);
}

void destruirEstructuras()
{
    // Colas
    queue_destroy(cola_new);
    queue_destroy(cola_ready);
    queue_destroy(cola_exec);
    queue_destroy(cola_blocked);
    queue_destroy(cola_exit);
    // Mutex
    pthread_mutex_destroy(&thread_id_mutex);
    pthread_mutex_destroy(&process_id_mutex);
    pthread_mutex_destroy(&mutex_cola_new);
    // Semaforos
    sem_destroy(&sem_nuevoProceso);

    free(config);
}


void enviarTCBCPU(TCB* tcb){

    t_buffer* buffer = SerializarTCB(tcb);

    EnviarOperacion(socket_dispatch,1,buffer);
    buffer_destroy(buffer);

    desalojado = true;
}
TCB* recibirTCBCPU(int* funcion){

    t_buffer* buffer = RecibirBuffer(socket_dispatch);
    TCB* tcb = DeserializarTCB(buffer);
    *funcion = buffer_read_int(buffer);

    desalojado = false;
    buffer_destroy(buffer);
    
    return tcb;
}

void InterrumpirCPU(int *quantumProceso)
{
    usleep(*quantumProceso * 1000);
    if (!desalojado)
    {
        t_buffer *buffer = buffer_create(sizeof(int));
        buffer_add_uint32(buffer, 0);
        EnviarOperacion(socket_interrupt, 2, buffer);
    }
}

void interpretarCPU(){ //despues de recibir de CPU

    t_buffer* buffer = RecibirBuffer(socket_dispatch);
    TCB* tcb = DeserializarTCB(buffer);
    int funcion = buffer_read_int(buffer);
    
    uint32_t PID = tcb->PID;
    uint32_t TID = tcb->TID;
    char* archivo;
    char* recurso;
    uint32_t tid;
    TCB* tcb_n = malloc(sizeof(TCB));
    PCB* pcb = malloc(sizeof(PCB));
    Mutex* mutex_existente;
    
    switch (funcion)
    {
    case DUMP_MEMORY://FINALIZADO

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: DUMP_MEMORY",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        tcb_n = obtener_TCB_por_PID_y_TID(lista_exec_hilo,PID,TID);
        tcb_n->estado = BLOCKED;
        
        quitar_TCB_de_lista_exec_hilo(tcb_n,PID,TID);
        list_add(lista_bloq_hilo,tcb_n);

        t_buffer* bufferDump = buffer_create(sizeof(uint32_t)*2);
        buffer_add_uint32(bufferDump,PID);
        buffer_add_uint32(bufferDump,TID);
        EnviarOperacion(socket_memoria,MEMORIA_KERNEL_MEMORY_DUMP,buffer);

        int respuesta = RecibirBuffer(socket_memoria);
        if(respuesta < 0){
            tcb_n->estado = EXIT;
            quitar_TCB_de_lista_exec_hilo(lista_exec_hilo,PID,TID);
            finalizar_hilo(tcb_n);
        }else{
            tcb_n->estado = READY;
            quitar_TCB_de_lista_exec_hilo(lista_bloq_hilo,PID,TID);
            list_add(lista_exec_hilo,tcb_n);
        }
        break;
    case IO://FINALIZADO

        uint32_t retorno = buffer_read_uint32(buffer);

        tcb = obtener_TCB_por_PID_y_TID(lista_exec_hilo,PID,TID);

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Bloqueado por: IO",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        quitar_TCB_de_lista_exec_hilo(lista_exec_hilo,PID,TID);
        usleep(retorno*1000);
        agregarHiloAReady(tcb);
        break;

    case PROCESS_CREATE://FINALIZADO

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: PROCESS_CREATE",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        archivo = buffer_read_string(buffer);
        uint32_t tamanio = buffer_read_uint32(buffer);
        uint32_t prioridad_0 = buffer_read_uint32(buffer);

        crearProceso(archivo,tamanio,prioridad_0);
        break;
    case THREAD_CREATE://FINALIZADO

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: THREAD_CREATE",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        archivo = buffer_read_string(buffer);
        uint32_t prioridad = buffer_read_uint32(buffer);

        crear_hilo(archivo,prioridad,PID);
        break;
    case THREAD_JOIN://FINALIZADO?

        tid = buffer_read_uint32(buffer);
        
        tcb_n = buscarHilo(PID,tid); //FALTA FINALIZAR CORTO PLAZO

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Bloqueado por: THREAD_JOIN",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        if(tcb_n != NULL){

            tcb = obtener_TCB_por_PID_y_TID(lista_exec_hilo,PID,tid);
            
            dependencia* dependencia = malloc(sizeof(dependencia));
            dependencia->hilo = tcb_n;
            dependencia->dependiente = tcb;
            list_add(lista_dependencias,dependencia);

            quitar_TCB_de_lista_exec_hilo(lista_exec_hilo,PID,tid);
            list_add(lista_bloq_hilo,tcb);
        } 
        break;
    case THREAD_CANCEL://FINALIZADO

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: THREAD_CANCEL",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        tid = buffer_read_uint32(buffer);
        tcb = buscarHilo(PID,tid);

        tcb->estado =EXIT;
        finalizar_hilo(tcb);
        break;
    case MUTEX_CREATE://FINALIZADO

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: MUTEX_CREATE",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        recurso = buffer_read_string(buffer);
        pcb = obtener_PCB_por_PID(cola_exec,PID);
        tcb = buscarHilo(PID,TID);

        Mutex* mutex = malloc(sizeof(Mutex));
        t_queue* cola = malloc(sizeof(t_queue));
        mutex->recurso = recurso;
        mutex->hilo_actual = TID;
        mutex->bloqueado = false;
        mutex->tomado = false;
        mutex->cola_bloqueados = cola;
        mutex->hilo_actual = 0;
        list_add(pcb->mutex,mutex);
        //ACCIONAR COMO CORRESPONDA

        break;
    case MUTEX_LOCK://FINALIZADO

        recurso = buffer_read_string(buffer);
        pcb = obtener_PCB_por_PID(cola_exec,PID);
        tcb = obtener_TCB_por_PID_y_TID(lista_exec_hilo,PID,TID);
        mutex_existente = buscar_mutex(pcb->mutex,recurso);

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Bloqueado por: MUTEX_LOCK",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        if(mutex_existente != NULL){
            if(mutex_existente->tomado == true){
                
                queue_push(mutex_existente->cola_bloqueados,tcb);

            }else{
                mutex_existente->bloqueado = true;
                mutex_existente->tomado = true;
                mutex_existente->hilo_actual = tcb->TID;
                queue_push(mutex_existente->cola_bloqueados,tcb);

                tcb->estado=BLOCKED;
                quitar_TCB_de_lista_exec_hilo(lista_exec_hilo,PID,TID);
                list_add(lista_bloq_hilo,tcb);
            }
        }

        break;
    case MUTEX_UNLOCK:

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: MUTEX_UNLOCK",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        recurso = buffer_read_string(buffer);

        //ACCIONAR COMO CORRESPONDA
        recurso = buffer_read_string(buffer);
        pcb = obtener_PCB_por_PID(cola_exec,PID);
        tcb = obtener_TCB_por_PID_y_TID(lista_exec_hilo,PID,TID);
        mutex_existente = buscar_mutex(pcb->mutex,recurso);

        if(mutex != NULL){
            if(mutex_existente->tomado == true){
                
                if (queue_size(mutex->cola_bloqueados) == 1) {

                    tcb = queue_peek(mutex_existente->cola_bloqueados);
                    queue_pop(mutex_existente->cola_bloqueados);
                    mutex_existente->bloqueado = false;
                    mutex_existente->tomado =false;
                    mutex_existente->hilo_actual = 0;
                    
                    tcb->estado = READY;
                    agregarHiloAReady(tcb);

                }else{

                    tcb = queue_peek(mutex_existente->cola_bloqueados);
                    queue_pop(mutex_existente->cola_bloqueados);
                    tcb_n = queue_peek(mutex_existente->cola_bloqueados);
                    mutex_existente->hilo_actual = tcb_n->TID;

                    tcb->estado = READY;
                    agregarHiloAReady(tcb);
                }

                mutex_existente->bloqueado = true;
                mutex_existente->tomado = true;
                mutex_existente->hilo_actual = tcb->TID;
                queue_push(mutex_existente->cola_bloqueados,tcb);

            }else{
                
            }
        }
        
        break;
    case THREAD_EXIT://FINALIZADO

        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: THREAD_EXIT",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        tcb_n = obtener_TCB_por_PID_y_TID(lista_exec_hilo,PID,TID);
        tcb_n->estado = EXIT;
        quitar_TCB_de_lista_exec_hilo(lista_exec_hilo,PID,TID);
        finalizar_hilo(tcb_n);
        break;
    case WRITE_MEM: //HACE LO MISMO QUE EL PROCESS EXIT

        
        int error = buffer_read_int(buffer);

        if(error == OP_OUT_OF_MEMORY){
            pcb = obtener_PCB_por_PID(cola_exec,PID);
            finalizarProceso(pcb);
        }
        break;
        
        
    case PROCESS_EXIT: //FINALIZADO

        
        pthread_mutex_lock(&mutex_kernel_log_counter);
        log_info(kernel_logger,"%d (<%d>:<%d>) - Solicitó syscall: PROCESS_EXIT",kernel_log_counter,PID,TID);
        kernel_log_counter++;
        pthread_mutex_unlock(&mutex_kernel_log_counter);

        if(TID == 0){
            pcb = obtener_PCB_por_PID(cola_exec,PID);
            finalizarProceso(pcb);
        }
        break;
    default:
        break;
    }

    buffer_destroy(buffer);
}

int crearProcesoMemoria(PCB *proceso,char* archivo,uint32_t tamanio)//CREAR PROCESO EN MEMORIA++
{
    t_buffer* buffer = buffer_create(sizeof(uint32_t)*2 + sizeof(String));
    buffer_add_uint32(buffer,proceso->PID);
    buffer_add_string(buffer,archivo);
    buffer_add_uint32(buffer,tamanio);

    conectarMemoria();
    EnviarOperacion(socket_memoria,MEMORIA_KERNEL_CREAR_PROCESO,buffer);
    buffer = RecibirBuffer(socket_memoria);

    int respuesta = buffer_read_int(buffer);

    close(socket_memoria);

    return respuesta;
}

int finalizarProcesoMemoria(PCB* pcb){
    t_buffer* buffer = buffer_create(sizeof(uint32_t));
    buffer_add_uint32(buffer,pcb->PID);

    conectarMemoria();
    EnviarOperacion(socket_memoria,MEMORIA_KERNEL_FINALIZAR_PROCESO,buffer);
    buffer = RecibirBuffer(socket_memoria);
    int respuesta = buffer_read_int(buffer);
    close(socket_memoria);
    return respuesta;
}

bool finalizarHiloMemoria(TCB *hilo)//FINALIZAR HILO++
{
    uint32_t PID = hilo->PID;
    uint32_t TID = hilo->TID;

    t_buffer* buffer = buffer_create(sizeof(uint32_t)*2);

    buffer_add_uint32(buffer,PID);
    buffer_add_uint32(buffer,TID);

    conectarMemoria();
    EnviarOperacion(socket_memoria,MEMORIA_KERNEL_FINALIZAR_HILO,buffer);
    buffer = RecibirBuffer(socket_memoria);

    int respuesta = buffer_read_int(buffer);

    close(socket_memoria);
    buffer_destroy(buffer);

    return respuesta;
}

bool crearHiloMemoria(TCB *hilo,char* archivo) //CREAR HILO +++
{

    t_buffer* buffer = buffer_create(sizeof(uint32_t)*3 + sizeof(String));
    buffer_add_uint32(buffer,hilo->PID);
    buffer_add_uint32(buffer,hilo->TID);
    buffer_add_string(buffer,archivo);
    buffer_add_uint32(buffer,hilo->prioridad);

    conectarMemoria();
    EnviarOperacion(socket_memoria,MEMORIA_KERNEL_CREAR_HILO,buffer);
    buffer = RecibirBuffer(socket_memoria);
    int respuesta = buffer_read_int(buffer);
    close(socket_memoria);

    return respuesta;
}

Algoritmo buscarAlgoritmo(char* token) {
    if (strcmp(token, "CMN") == 0) {
        return CMN;
    } else if (strcmp(token, "FIFO") == 0) {
        return FIFO;
    } else if (strcmp(token, "PRIORIDADES") == 0) {
        return PRIORIDADES;
    } else if (strcmp(token, "COLAS MULTINIVEL") == 0) {
        return COLAS_MULTINIVEL;
    }

    return -1;
    
}

void agregarHiloAReady(TCB* nuevoTCB){

    switch (algoritmo){
        case CMN: {
            if (!insertar_en_cola_ready_cmn(nuevoTCB)){
            log_error(kernel_logger, "Error al insertar el TCB en la cola READY.");
            liberar_tcb(nuevoTCB);
            break;
            }
            // Registrar el log de creación del hilo
            pthread_mutex_lock(&mutex_kernel_log_counter);
            log_info(kernel_logger, "%d (%d:%d) Se crea el Hilo - Estado: READY", kernel_log_counter, nuevoTCB->PID, nuevoTCB->TID);
            kernel_log_counter++;  
            pthread_mutex_unlock(&mutex_kernel_log_counter);   
            break;       
        }
        case FIFO:{
            queue_push(cola_ready_fifo_prioridades, nuevoTCB);
            pthread_mutex_lock(&mutex_kernel_log_counter);
            log_info(kernel_logger, "%d (%d:%d) Se crea el Hilo - Estado: READY", kernel_log_counter, nuevoTCB->PID, nuevoTCB->TID);
            kernel_log_counter++;
            pthread_mutex_unlock(&mutex_kernel_log_counter);
            break;
        }
        case PRIORIDADES:{
            queue_push(cola_ready_fifo_prioridades, nuevoTCB);
            pthread_mutex_lock(&mutex_kernel_log_counter);
            log_info(kernel_logger, "%d (%d:%d) Se crea el Hilo - Estado: READY", kernel_log_counter, nuevoTCB->PID, nuevoTCB->TID);
            kernel_log_counter++;
            pthread_mutex_unlock(&mutex_kernel_log_counter);
            break;
        }
        default:
            break;
    }
}


// Función auxiliar para comparar PIDs
bool comparar_por_PID(void* elemento) {
    PCB* pcb = (PCB*)elemento;
    return (pcb->PID == buscar_PID);
}

// Función para obtener un PCB de la cola por PID
PCB* obtener_PCB_por_PID(t_queue* cola, uint32_t PID) {
    if (!cola) {
        return NULL; // Validación básica
    }

    // Configurar la variable global para la búsqueda
    buscar_PID = PID;

    // Iterar sobre la cola para buscar el PCB con el PID correspondiente
    void* pcb_encontrado = NULL;
    int cantidad_elementos = queue_size(cola); // Obtener el tamaño de la cola

    for (int i = 0; i < cantidad_elementos; i++) {
        pcb_encontrado = queue_pop(cola); // Extraer el primer elemento de la cola
        if (comparar_por_PID(pcb_encontrado)) {
            return (PCB*)pcb_encontrado; // Encontramos el PCB con el PID
        }
        // Si no es el PCB que buscamos, lo volvemos a poner en la cola
        queue_push(cola, pcb_encontrado);
    }

    return NULL; // Si no se encuentra el PCB
}

bool comparar_por_PID_y_TID(void* elemento) {
    TCB* tcb = (TCB*)elemento;
    return (tcb->PID == buscar_PID && tcb->TID == buscar_TID);
}

// Función para buscar un TCB en la lista por PID y TID
TCB* obtener_TCB_por_PID_y_TID(t_list* lista_exec_hilo, uint32_t PID, uint32_t TID) {
    if (!lista_exec_hilo) {
        return NULL; // Validación básica
    }

    // Configurar las variables globales para la búsqueda
    buscar_PID = PID;
    buscar_TID = TID;

    // Usar list_find para buscar el TCB
    void* tcb_encontrado = list_find(lista_exec_hilo, comparar_por_PID_y_TID);

    return (TCB*)tcb_encontrado; // Devuelve el TCB si lo encuentra, o NULL si no
}

// Función para eliminar un TCB de la lista_exec_hilo según PID y TID
void quitar_TCB_de_lista_exec_hilo(t_list* lista_exec_hilo, uint32_t PID, uint32_t TID) {
    // Asignar los valores de PID y TID a las variables globales
    buscar_PID = PID;
    buscar_TID = TID;

    // Eliminar el TCB con el PID y TID especificados
    list_remove_by_condition(lista_exec_hilo, comparar_por_PID_y_TID);
}


// Función para comparar dos TCBs por PID y TID
bool es_TCB_buscado(TCB* tcb, uint32_t PID, uint32_t TID) {
    return tcb->PID == PID && tcb->TID == TID;
}

// Función para quitar un TCB de la cola sin saber su posición
TCB* quitar_TCB_de_cola(t_queue* cola, uint32_t PID, uint32_t TID) {
    if (!cola) {
        return NULL; // Validación básica
    }

    t_queue* cola_auxiliar = queue_create();
    TCB* tcb_encontrado = NULL;

    while (!queue_is_empty(cola)) {
        TCB* tcb_actual = queue_pop(cola);

        if (!tcb_encontrado && es_TCB_buscado(tcb_actual, PID, TID)) {
            tcb_encontrado = tcb_actual; // Encontramos el TCB buscado
        } else {
            queue_push(cola_auxiliar, tcb_actual); // Reinsertamos el resto
        }
    }

    // Restaurar los elementos a la cola original
    while (!queue_is_empty(cola_auxiliar)) {
        queue_push(cola, queue_pop(cola_auxiliar));
    }

    queue_destroy(cola_auxiliar);
    return tcb_encontrado; // Devuelve el TCB eliminado o NULL si no se encontró
}

TCB* buscarTCBDeCola(t_queue* cola, uint32_t PID, uint32_t TID){
    if (!cola) {
        return NULL; // Validación básica
    }

    t_queue* cola_auxiliar = queue_create();
    TCB* tcb_encontrado = NULL;

    while (!queue_is_empty(cola)) {
        TCB* tcb_actual = queue_pop(cola);

        if (!tcb_encontrado && es_TCB_buscado(tcb_actual, PID, TID)) {
            tcb_encontrado = tcb_actual; // Encontramos el TCB buscado
            queue_push(cola_auxiliar, tcb_actual); // Reinsertamos nuevamente
        } else {
            queue_push(cola_auxiliar, tcb_actual); // Reinsertamos el resto
        }
    }

    // Restaurar los elementos a la cola original
    while (!queue_is_empty(cola_auxiliar)) {
        queue_push(cola, queue_pop(cola_auxiliar));
    }

    queue_destroy(cola_auxiliar);
    return tcb_encontrado; // Devuelve el TCB eliminado o NULL si no se encontró
}

bool tieneDependiente(TCB* hilo) {
    // Recorrer la lista de dependencias buscando si el hilo tiene un dependiente
    for (int i = 0; i < list_size(lista_dependencias); i++) {
        dependencia* dep = list_get(lista_dependencias, i);
        if (dep->hilo == hilo) {
            return dep->dependiente != NULL;
        }
    }
    return false; // Si no se encuentra el hilo o no tiene dependiente
}
int buscarDependiente(TCB* hilo) {
    // Buscar la dependencia específica del hilo
    for (int i = 0; i < list_size(lista_dependencias); i++) {
        dependencia* dep = list_get(lista_dependencias, i);
        if (dep->hilo == hilo && dep->dependiente != NULL) {
            TCB* bq = dep->dependiente;
            list_remove(lista_dependencias,i);
            free(dep);
            bq->estado = READY;                        // Actualizar estado
            agregarHiloAReady(bq);;
            
        }
    }
    return NULL; // Si no se encuentra el dependiente con los PID y TID especificados
}

TCB* buscarTCBPorPIDyTID(ColasReady colas_ready, uint32_t PID, uint32_t TID) {
    for (int i = 0; i < MAX_NIVELES_PRIORIDAD; i++) {
        t_queue* cola = colas_ready.colas_prioridad[i];

        // Recorrer la cola para buscar el TCB con el PID y TID especificados
        for (int j = 0; j < queue_size(cola); j++) {
            TCB* tcb_en_cola = queue_peek(cola); // Obtener el TCB en la posición j de la cola

            // Comparar PID y TID del TCB con los valores proporcionados
            if (tcb_en_cola->PID == PID && tcb_en_cola->TID == TID) {
                return tcb_en_cola; // TCB encontrado
            }
        }
    }
    return false; // TCB no encontrado en ninguna cola
}

TCB* buscarHilo(uint32_t PID, uint32_t TID){
    
    TCB* tcb1 = obtener_TCB_por_PID_y_TID(lista_exec_hilo,PID,TID);//busco el hilo
    if(tcb1 == NULL){
        
        tcb1 = obtener_TCB_por_PID_y_TID(lista_bloq_hilo,PID,TID);
        if(tcb1 == NULL){
            switch(algoritmo){
                case FIFO:
                case PRIORIDADES:
                    tcb1 = buscarTCBDeCola(cola_ready_fifo_prioridades,PID,TID);
                    break;
                case CMN:
                    tcb1 = buscarTCBPorPIDyTID(colas_ready_cmn,PID,TID);
                    break;
                case COLAS_MULTINIVEL:

                    break;
            }
        }
    }
    return tcb1;
}

// Función para buscar un Mutex en una lista
Mutex* buscar_mutex(t_list* lista_mutexes, char* string) {
    // Recorrer la lista buscando el mutex con el recurso indicado
    for (int i = 0; i < list_size(lista_mutexes); i++) {
        Mutex* mutex_actual = list_get(lista_mutexes, i);
        if (strcmp(mutex_actual->recurso, string) == 0) {
            return mutex_actual; // Retorna el mutex encontrado
        }
    }
    return NULL; // Si no se encuentra el mutex, retorna NULL
}

void iniciador(){
    pthread_mutex_init(&mutex_kernel_log_counter, NULL);
    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_lista_thread_id, NULL);
    pthread_mutex_init(&process_id_mutex, NULL);
    pthread_mutex_init(&thread_id_mutex, NULL);
    pthread_mutex_init(&mutex_config, NULL);
    pthread_mutex_init(&mutex_lista_exec_hilo, NULL);
    pthread_mutex_init(&mutex_cola_ready_fifo_prioridades, NULL);
    pthread_mutex_init(&mutex_colas_ready_cmn, NULL);

    kernel_log_counter = 0;
    process_id = 0;
}
