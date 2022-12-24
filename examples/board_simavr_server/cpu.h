
#ifndef __CPU_H__
#define __CPU_H__

#include "sim_avr.h"


void avr_special_init( avr_t * avr, void * data);
void avr_special_deinit( avr_t* avr, void * data);
avr_t* init_cpu(const char* boot_path);

#endif /* __CPU_H__ */
