/*
 * DUT.c
 *
 *  Created on: 1 may 2026
 *      Author: tomyl
 */

#include <stdint.h>
#include "DUT.h"
#include "main.h"

/*-------------------------------------------------------------

	NOMENCLATURA DE PREFIJOS:

	IU: función/variable declarada en el header de IU.h, librería de la interfaz de usuario.
	DUT: función/variable declarada en el header DUT.h, librería del DUT.
	ANTR: función/variable declarada en el header ANTR.h, librería antirrebote.
	Sin prefijo: función/variable local de este .c

-------------------------------------------------------------*/


DUT_parametro_t DUT_estado_parametro = DUT_PARAMETRO_RESISTENCIA;
DUT_parametro_t DUT_estado_modo = DUT_MODO_UNICO;
uint8_t flag_ADC = 0;
uint32_t resultado;
uint32_t medida_adc;

typedef enum{
	MEDIDA_1M,
	MEDIDA_10K,
	MEDIDA_330,
	MEDIDA_OFF
}	tipo_medida_t;

void DUT_Configurar(tipo_medida_t medida);


void Set_Pin(uint16_t Pin, uint8_t alto) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = Pin;
	if (alto) {
		// 3.3V
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
		HAL_GPIO_WritePin(GPIOA, Pin, GPIO_PIN_SET);
	} else {
		// Alta Impedancia
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	}
}


void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADC1){
		medida_adc = HAL_ADC_GetValue(hadc);
		flag_ADC = 1;
	}
}


void DUT_Iniciar(){
	DUT_Configurar(MEDIDA_1M);
	HAL_TIM_Base_Start(&htim3);
	HAL_ADC_Start_IT(&hadc1);
}

void DUT_Detener(){
	DUT_Configurar(MEDIDA_OFF);
	HAL_TIM_Base_Stop(&htim3);
	HAL_ADC_Stop_IT(&hadc1);
}

void DUT_Configurar(tipo_medida_t medida){
	switch (medida) {
	case MEDIDA_1M:
		Set_Pin(GPIO1M_Pin, 1);
		Set_Pin(GPIO10K_Pin, 0);
		Set_Pin(GPIO330R_Pin, 0);
		break;
	case MEDIDA_10K:
		Set_Pin(GPIO1M_Pin, 0);
		Set_Pin(GPIO10K_Pin, 1);
		Set_Pin(GPIO330R_Pin, 0);
		break;
	case MEDIDA_330:
		Set_Pin(GPIO1M_Pin, 0);
		Set_Pin(GPIO10K_Pin, 0);
		Set_Pin(GPIO330R_Pin, 1);
		break;
	case MEDIDA_OFF:
		Set_Pin(GPIO1M_Pin, 0);
		Set_Pin(GPIO10K_Pin, 0);
		Set_Pin(GPIO330R_Pin, 1);
		break;
	default:
		break;
	}
}

uint32_t DUT_Medir(){
	if (flag_ADC == 1){
		flag_ADC = 0;
		resultado = medida_adc;
		return resultado;
	}
	return 0;
}
