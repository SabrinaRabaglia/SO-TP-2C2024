#include "../utils_C/main.h"

void* obtener_direccion_fisica(uint32_t dir, uint32_t base,uint32_t limite){
    
    if(limite < dir){

        return (void*) -1;
    }else{
        uint32_t fisica = dir + base;
        return (void*) fisica;
    }
    
}