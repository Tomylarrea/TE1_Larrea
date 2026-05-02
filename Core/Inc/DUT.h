/*
 * DUT.h
 *
 *  Created on: 1 may 2026
 *      Author: tomyl
 */

#ifndef INC_DUT_H_
#define INC_DUT_H_

typedef enum {
	DUT_PARAMETRO_RESISTENCIA,
	DUT_PARAMETRO_CAPACITANCIA
} DUT_parametro_t;

typedef enum {
	DUT_MODO_UNICO,
	DUT_MODO_PERIODICO
} DUT_modo_t;

extern DUT_parametro_t DUT_estado_parametro;
extern DUT_parametro_t DUT_estado_modo;


#endif /* INC_DUT_H_ */
