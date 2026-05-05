/*
 * DUT.h
 *
 *  Created on: 1 may 2026
 *      Author: tomyl
 */

#ifndef INC_DUT_H_
#define INC_DUT_H_

#include <stdint.h>

typedef enum {
	DUT_PARAMETRO_RESISTENCIA,
	DUT_PARAMETRO_CAPACITANCIA
} DUT_parametro_t;

typedef enum {
	DUT_MODO_UNICO,
	DUT_MODO_PERIODICO
} DUT_modo_t;

extern DUT_parametro_t DUT_estado_parametro; // Se declaran como extern para poder cambiarla desde la IU
extern DUT_modo_t      DUT_estado_modo;
extern uint8_t flag_med_unica;

extern uint8_t flag_ADC;	// Para avisar que se ejecutó la ISR del ADC con timer

void DUT_Iniciar(void);
void DUT_Detener(void);
void DUT_Medir(void);

#endif /* INC_DUT_H_ */
