#include "commons.h"
#include <string.h>
#include <stdlib.h>
#include <commons/string.h>
#include <stdlib.h>
#include <string.h>

t_log_level obtener_log_level(const char* log_level_name) {
    
	if (strcmp(log_level_name, "DEBUG") == 0) {
        return LOG_LEVEL_DEBUG;
    } else if (strcmp(log_level_name, "INFO") == 0) {
        return LOG_LEVEL_INFO;
    } else if (strcmp(log_level_name, "WARNING") == 0) {
        return LOG_LEVEL_WARNING;
    } else if (strcmp(log_level_name, "ERROR") == 0) {
        return LOG_LEVEL_ERROR;
    } else if (strcmp(log_level_name, "TRACE") == 0) {
        return LOG_LEVEL_TRACE;
    }
	
	//falta manejar error
    return LOG_LEVEL_INFO;
}

t_log* iniciar_logger(String path, String name, bool is_active_console, t_log_level level) {

    t_log* nuevo_logger = log_create(path, name, is_active_console, level);

    if(nuevo_logger == NULL){
        fprintf(stderr, "[ERROR] iniciar_logger [%s]\n", path);
        exit(EXIT_FAILURE);
    }
        
    return nuevo_logger;
}

t_config* iniciar_config(String path) {

    t_config* nuevo_config = config_create(path);

    if(nuevo_config == NULL){
        fprintf(stderr, "[ERROR] iniciar_config [%s]\n", path);
        exit(EXIT_FAILURE);
    }

    return nuevo_config;
}

char *getNombreModulo(int modulo)
{
	char *nombre;
	switch (modulo)
	{
	case KERNEL:
		nombre = "Kernel";
		break;
	case MEMORIA:
		nombre = "Memoria";
		break;
	case FILESYSTEM:
		nombre = "FILESYSTEM";
		break;
	case CPU:
		nombre = "CPU";
		break;
        
	default:
		nombre = "No existe ese modulo";
		break;
	}
	return nombre;
}

//Agrega el segundo buffer al primero, sin mover el offset
void buffer_add_buffer(t_buffer* buffer1, t_buffer* buffer2)
{
	int offset = buffer1->size;
	buffer1->size = buffer1->size + buffer2->size;
	void *temp = realloc(buffer1->stream, buffer1->size);
	if (temp == NULL && buffer1->size != 0) {
		// Handle allocation error, e.g., log an error message or exit
		fprintf(stderr, "Memory allocation failed\n");
		// Decide on the error handling strategy (exit, return error code, etc.)
	} else {
	buffer1->stream = temp;
	}
	memcpy(buffer1->stream + offset,buffer2->stream,buffer2->size);
}

void buffer_add_int(t_buffer *buffer, int data)
{
	memcpy(buffer->stream + buffer->offset, &data, sizeof(int));
	buffer->offset += sizeof(int);
}
// Agrega un uint32_t al buffer en la posición actual y avanza el offset
void buffer_add_uint32(t_buffer *buffer, uint32_t data)
{
	memcpy(buffer->stream + buffer->offset, &data, sizeof(uint32_t));
	buffer->offset += sizeof(uint32_t);
}

// Agrega un uint8_t al buffer
void buffer_add_uint8(t_buffer *buffer, uint8_t data)
{
	memcpy(buffer->stream + buffer->offset, &data, sizeof(uint8_t));
	buffer->offset += sizeof(uint8_t);
}

// Agrega string al buffer con un uint32_t adelante
void buffer_add_string(t_buffer *buffer, char *string)
{
	int length = strlen(string);
	memcpy(buffer->stream + buffer->offset, &length, sizeof(uint32_t));
	buffer->offset += sizeof(uint32_t);
	memcpy(buffer->stream + buffer->offset, string, length);
	buffer->offset += length;
}

void buffer_add_contexto(t_buffer *buffer, t_contexto_cpu *contexto)
{
    // Serializar la cadena 'base' del contexto
    int length_base = strlen(contexto->contexto.base);  // Obtener el tamaño de la cadena
    memcpy(buffer->stream + buffer->offset, &length_base, sizeof(uint32_t));  // Guardar el tamaño
    buffer->offset += sizeof(uint32_t);  // Avanzar el offset
    memcpy(buffer->stream + buffer->offset, contexto->contexto.base, length_base);  // Copiar la cadena
    buffer->offset += length_base;  // Avanzar el offset

    // Serializar el 'limite' (un entero)
    memcpy(buffer->stream + buffer->offset, &contexto->contexto.limite, sizeof(int));  // Guardar el límite
    buffer->offset += sizeof(int);  // Avanzar el offset

    // Serializar los 'registros' (estructura t_registros_hilo)
    memcpy(buffer->stream + buffer->offset, &contexto->registros, sizeof(t_registros_hilo));  // Guardar los registros
    buffer->offset += sizeof(t_registros_hilo);  // Avanzar el offset
}

//lee el contexto_cpu del buffer de datos
t_contexto_cpu *buffer_read_contexto(t_buffer *buffer) {
    
	t_contexto_cpu *contexto;

    // Leer la base
    int size_base = strlen((char *)(buffer->stream + buffer->offset)) + 1;  // +1 para el '\0'
    contexto->contexto.base = malloc(size_base);
    memcpy(contexto->contexto.base, buffer->stream + buffer->offset, size_base);
    buffer->offset += size_base;

    // Leer el limite
    memcpy(&contexto->contexto.limite, buffer->stream + buffer->offset, sizeof(int));
    buffer->offset += sizeof(int);

    // Leer los registros
    memcpy(&contexto->registros, buffer->stream + buffer->offset, sizeof(t_registros_hilo));
    buffer->offset += sizeof(t_registros_hilo);

    return contexto;
}

//Lee un string del buffer
char* buffer_read_string(t_buffer* buffer)
{	
	int length = buffer_read_int(buffer);

	char* string = malloc(length+1);
	for(int i = 0; i<length;i++)
	{
		string[i] = *(char*)(buffer->stream + buffer->offset + i);
	}
	string[length] = '\0';
	buffer->offset += length;
	return string;
}

int buffer_read_int(t_buffer *buffer)
{
	int i = *(int *)(buffer->stream + buffer->offset);
	buffer->offset += sizeof(int);
	return i;
}

// Lee un uint32_t del buffer en la dirección data y avanza el offset
uint32_t buffer_read_uint32(t_buffer *buffer)
{
	uint32_t i = *(uint32_t *)(buffer->stream + buffer->offset);
	buffer->offset += sizeof(uint32_t);
	return i;
}
// Lee un uint8_t del buffer
uint8_t buffer_read_uint8(t_buffer *buffer)
{
	uint8_t i = *(uint8_t *)(buffer->stream + buffer->offset);
	buffer->offset += sizeof(uint8_t);
	return i;
}

// Crea el buffer para posterior uso
t_buffer *buffer_create(int size)
{
	t_buffer *buffer = malloc(sizeof(t_buffer));
	buffer->offset = 0;
	buffer->size = size;
	buffer->stream = malloc(size);
	return buffer;
}
// Libera la memoria asociada al buffer
void buffer_destroy(t_buffer *buffer)
{
	free(buffer->stream);

	free(buffer);
}

void* buffer_read(t_buffer * buffer,int size)
{
	void* value = malloc(size);
	memcpy(value,buffer->stream+buffer->offset,size);
	buffer->offset +=size;
	return value;
}

t_buffer * buffer_get_sobrante(t_buffer * buffer)
{
	int sobrante = buffer->size-buffer->offset;
	t_buffer * bufferNuevo = buffer_create(sobrante);
	memcpy(bufferNuevo->stream,buffer->stream+ buffer->offset,sobrante);
	return bufferNuevo;
}

void buffer_write(t_buffer* buffer, void* value, int size)
{
	memcpy(buffer->stream+buffer->offset,value,size);
	buffer->offset +=size;
}

t_buffer* SerializarString(char *mensaje)
{
	t_buffer * buffer = buffer_create(strlen(mensaje)+sizeof(int));
	buffer_add_string(buffer,mensaje);
	return buffer;
}

TCB* DeserializarTCB(t_buffer *buffer)
{
	TCB *p = malloc(sizeof(TCB));

	p->PID = buffer_read_uint32(buffer);
	p->TID = buffer_read_uint32(buffer);
    p->prioridad = buffer_read_uint32(buffer);
    p->estado = buffer_read_int(buffer);

	return p;
}

t_buffer* SerializarTCB(TCB* p) {
    // Crear un buffer nuevo
    t_buffer* buffer = malloc(sizeof(t_buffer));
    buffer->offset = 0;
    buffer->stream = malloc(sizeof(uint32_t) * 3 + sizeof(int)); // Tamaño inicial suficiente para los datos del TCB

    // Serializar PID
    *(uint32_t*)(buffer->stream + buffer->offset) = p->PID;
    buffer->offset += sizeof(uint32_t);

    // Serializar TID
    *(uint32_t*)(buffer->stream + buffer->offset) = p->TID;
    buffer->offset += sizeof(uint32_t);

    // Serializar prioridad
    *(uint32_t*)(buffer->stream + buffer->offset) = p->prioridad;
    buffer->offset += sizeof(uint32_t);

    // Serializar estado
    *(int*)(buffer->stream + buffer->offset) = p->estado;
    buffer->offset += sizeof(int);

    return buffer;
}


t_buffer* SerializarPCB(PCB* pcb) {
    // Crear un buffer nuevo
    t_buffer* buffer = malloc(sizeof(t_buffer));
    buffer->size = 0;
    buffer->stream = malloc(1024); // Tamaño inicial arbitrario, puede ajustarse dinámicamente si es necesario

    // Serializar PID
    uint32_t pid = pcb->PID;
    memcpy(buffer->stream + buffer->size, &pid, sizeof(uint32_t));
    buffer->size += sizeof(uint32_t);

    // Serializar cantidad de TIDs
    uint32_t tid_count = list_size(pcb->TIDs);
    memcpy(buffer->stream + buffer->size, &tid_count, sizeof(uint32_t));
    buffer->size += sizeof(uint32_t);

    // Serializar los TIDs
    for (int i = 0; i < tid_count; i++) {
        uint32_t tid = (uint32_t) list_get(pcb->TIDs, i);
        memcpy(buffer->stream + buffer->size, &tid, sizeof(uint32_t));
        buffer->size += sizeof(uint32_t);
    }

    // Serializar cantidad de Mutexes
    uint32_t mutex_count = list_size(pcb->mutex);
    memcpy(buffer->stream + buffer->size, &mutex_count, sizeof(uint32_t));
    buffer->size += sizeof(uint32_t);

    // Serializar los Mutexes
    for (int i = 0; i < mutex_count; i++) {
        uint32_t mutex_id = (uint32_t) list_get(pcb->mutex, i);
        memcpy(buffer->stream + buffer->size, &mutex_id, sizeof(uint32_t));
        buffer->size += sizeof(uint32_t);
    }

    return buffer;
}


PCB* DeserializarPCB(t_buffer* buffer) {
    PCB* pcb = malloc(sizeof(PCB));
    if (!pcb) return NULL; // Manejo de error por falta de memoria

    size_t offset = 0;

    // Deserializar PID
    memcpy(&(pcb->PID), buffer->stream + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Deserializar cantidad de TIDs
    uint32_t tid_count;
    memcpy(&tid_count, buffer->stream + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Deserializar los TIDs
    pcb->TIDs = list_create();
    for (uint32_t i = 0; i < tid_count; i++) {
        uint32_t tid;
        memcpy(&tid, buffer->stream + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        list_add(pcb->TIDs, (void*)(uintptr_t)tid);
    }

    // Deserializar cantidad de Mutexes
    uint32_t mutex_count;
    memcpy(&mutex_count, buffer->stream + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Deserializar los Mutexes
    pcb->mutex = list_create();
    for (uint32_t i = 0; i < mutex_count; i++) {
        uint32_t mutex_id;
        memcpy(&mutex_id, buffer->stream + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        list_add(pcb->mutex, (void*)(uintptr_t)mutex_id);
    }

    return pcb;
}


Registros* DeserializarRegistros(t_buffer *buffer) {
    Registros *registros = malloc(sizeof(Registros));
    
    registros->PC = buffer_read_uint32(buffer);
    registros->AX = buffer_read_uint32(buffer);
    registros->BX = buffer_read_uint32(buffer);
    registros->CX = buffer_read_uint32(buffer);
    registros->DX = buffer_read_uint32(buffer);
    registros->EX = buffer_read_uint32(buffer);
    registros->FX = buffer_read_uint32(buffer);
    registros->GX = buffer_read_uint32(buffer);
    registros->HX = buffer_read_uint32(buffer);
    registros->BASE = buffer_read_uint32(buffer);
    registros->LIMITE = buffer_read_uint32(buffer);

    return registros;
}

void SerializarRegistros(t_buffer *buffer, Registros *registros) {
    // Asegurarse de que hay suficiente espacio en el buffer
    buffer->stream = realloc(buffer->stream, buffer->offset + sizeof(uint32_t) * 11);

    // Escribir cada campo en el buffer en orden
    *(uint32_t *)(buffer->stream + buffer->offset) = registros->PC;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->AX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->BX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->CX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->DX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->EX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->FX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->GX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->HX;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->BASE;
    buffer->offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + buffer->offset) = registros->LIMITE;
    buffer->offset += sizeof(uint32_t);
}

t_buffer* SerializarPIDTID(Registros *registros, uint32_t PID, uint32_t TID) {
    // Crear e inicializar el buffer
    t_buffer* buffer = malloc(sizeof(t_buffer));
    buffer->size = sizeof(uint32_t) * 11; // 11 campos a serializar
    buffer->stream = malloc(buffer->size);

    // Inicializar el offset
    uint32_t offset = 0;

    // Serializar cada registro
    *(uint32_t *)(buffer->stream + offset) = registros->PC;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->AX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->BX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->CX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->DX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->EX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->FX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->GX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = registros->HX;
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = PID; // Serializar PID
    offset += sizeof(uint32_t);

    *(uint32_t *)(buffer->stream + offset) = TID; // Serializar TID
    offset += sizeof(uint32_t);

    return buffer; // Devolver el buffer serializado
}




char* DeserializarString(t_buffer* buffer)
{
	//int size = buffer_read_uint32(buffer);
	char* string = buffer_read_string(buffer);
	return string;
}
// Envia la operacion, que siempre es op_code + buffer TODO: Manejo de errores
void EnviarOperacion(int socket, int op_code, t_buffer *buffer)
{
	send(socket, &op_code, sizeof(int), 0);
	EnviarBuffer(socket,buffer);
}

void EnviarBuffer(int socket,t_buffer * buffer)
{
	send(socket, &(buffer->size),sizeof(int),0);
	send(socket, buffer->stream, buffer->size, 0);
}

t_buffer * RecibirBuffer(int socket)
{
	int size;
    recv(socket, &size, sizeof(int), 0);
	t_buffer * buffer = buffer_create(size);
	recv(socket,buffer->stream,buffer->size,0);
	return buffer;
}

void asignarMemoriaPara_uint32(uint32_t **integer){
    if (*integer == NULL) {
        *integer = malloc(sizeof(uint32_t));
    }
}

void asignarMemoriaPara_char(char **string){
    if (*string == NULL) {
        *string = malloc(256 * sizeof(char)); // Asignar 256 caracteres por defecto
    (*string)[0] = '\0'; // Inicializar la cadena vacía
    }
}

void asignarMemoriaParaInt(uint32_t **ptr) {
    *ptr = (uint32_t *)malloc(sizeof(uint32_t));
    if (*ptr == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
}

void asignarMemoriaParaString(char **ptr) {
    *ptr = (char *)malloc(sizeof(char) * 256);
    if (*ptr == NULL) {
        perror("Failed to allocate memory for string");
        exit(EXIT_FAILURE);
    }
}

void insertAtEnd(char *str, const char *value) {
    size_t len = strlen(str);
    if (len > 1) {
        // Replace the closing bracket with a comma and then append the value and closing bracket
        str[len - 1] = ',';
        strcat(str, value);
        strcat(str, "]");
    } else {
        // If the string is empty, just add the value inside the brackets
        strcat(str, value);
        strcat(str, "]");
    }
}

void deleteValue(char *str, const char *value) {
    char *pos = strstr(str, value);
    if (pos) {
        size_t len = strlen(value);
        if (pos[len] == ',') {
            // If the value is followed by a comma, remove the value and the comma
            memmove(pos, pos + len + 1, strlen(pos + len + 1) + 1);
        } else if (pos > str + 1 && *(pos - 1) == ',') {
            // If the value is preceded by a comma, remove the value and the preceding comma
            memmove(pos - 1, pos + len, strlen(pos + len) + 1);
        } else {
            // If the value is the only one, remove it directly
            memmove(pos, pos + len, strlen(pos + len) + 1);
        }
    }
}

t_log_level get_log_level_from_string(const char* log_level_str) {
	if (log_level_str == NULL){
		return LOG_LEVEL_INFO;
	}else if (strcmp(log_level_str, "TRACE") == 0) {
        return LOG_LEVEL_TRACE;
    } else if (strcmp(log_level_str, "DEBUG") == 0) {
        return LOG_LEVEL_DEBUG;
    } else if (strcmp(log_level_str, "INFO") == 0) {
        return LOG_LEVEL_INFO;
    } else if (strcmp(log_level_str, "WARNING") == 0) {
        return LOG_LEVEL_WARNING;
    } else if (strcmp(log_level_str, "ERROR") == 0) {
        return LOG_LEVEL_ERROR;
    } else {
        // Manejar caso de valor desconocido
        return LOG_LEVEL_ERROR;  // Por ejemplo, devolver un nivel de error por defecto
    }
}