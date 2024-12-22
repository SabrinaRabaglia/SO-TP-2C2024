#include "../utils_C/main.h"

t_buffer* leer_en_memoria(void** direccion){

    t_buffer* buffer = buffer_create(sizeof(int)*2 + sizeof(int*));
    buffer_write(buffer,direccion,sizeof(int**));//direccion
    EnviarOperacion(socket_conexionMemoria,MEMORIA_CPU_READ_MEM,buffer);
    
    t_buffer* bufferRespuesta = RecibirBuffer(socket_conexionMemoria);
    buffer_destroy(buffer);
    return bufferRespuesta;
}

int escribir_en_memoria(void** direccion, void* value){

    t_buffer* buffer = buffer_create(sizeof(uint32_t)*2);
    buffer_write(buffer,direccion,sizeof(int**));//direccion

    EnviarOperacion(socket_conexionMemoria,MEMORIA_CPU_WRITE_MEM,buffer);

    buffer = RecibirBuffer(socket_conexionMemoria);
    int errorCode = buffer_read_uint32(buffer);
    free(buffer);
    return errorCode;
}

t_buffer* solicitar_instruccion(uint32_t pid,uint32_t tid, int pc){

    t_buffer* buffer =buffer_create(sizeof(int)*2);
    buffer_add_uint32(buffer,pid);
    buffer_add_uint32(buffer,tid);
    buffer_add_uint32(buffer,pc);

    EnviarOperacion(socket_conexionMemoria,MEMORIA_CPU_OBTENER_INSTRUCCION,buffer);
    buffer = RecibirBuffer(socket_conexionMemoria);
    return buffer;

}



Registros* solicitar_contexto(TCB* tcb){

    t_buffer* buffer = buffer_create(sizeof(uint32_t)*2);

    buffer_add_uint32(buffer,tcb->PID);
    buffer_add_uint32(buffer,tcb->TID);

    EnviarOperacion(socket_conexionMemoria,MEMORIA_CPU_OBTENER_CONTEXTO,buffer);
    buffer = RecibirBuffer(socket_conexionMemoria);

    Registros* registros = DeserializarRegistros(buffer);

    return registros;
}
