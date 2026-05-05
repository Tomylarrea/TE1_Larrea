/*
 * DUT.c
 *
 *  Created on: 1 may 2026
 *      Author: tomyl
 */

#include <stdint.h>
#include <stdio.h>
#include "DUT.h"
#include "main.h"

#define UMBRAL_1M_BAJA    0.5f
#define UMBRAL_10K_BAJA   0.005f
#define UMBRAL_330_BAJA   0.000033f
#define UMBRAL_10K_SUBE   1.5f
#define UMBRAL_330_SUBE   0.015f
#define ADC_SAT_HIGH      4000u
#define ADC_SAT_LOW       200u

/* Timeout máximo esperando que el capacitor descargue (ms).
 * Con 330R y capacitores grandes puede necesitar varios segundos;
 * se elige un valor conservador. Ajustar según el rango esperado. */
#define TIMEOUT_DESCARGA_MS  5000u

/*-------------------------------------------------------------

	NOMENCLATURA DE PREFIJOS:

	IU: función/variable declarada en el header de IU.h, librería de la interfaz de usuario.
	DUT: función/variable declarada en el header DUT.h, librería del DUT.
	ANTR: función/variable declarada en el header ANTR.h, librería antirrebote.
	Sin prefijo: función/variable local de este .c

-------------------------------------------------------------*/

DUT_parametro_t DUT_estado_parametro = DUT_PARAMETRO_RESISTENCIA;
DUT_modo_t      DUT_estado_modo      = DUT_MODO_UNICO;
uint8_t flag_ADC = 0;
uint8_t flag_med_unica = 0;
uint32_t resultado;
float flag_res;
volatile uint32_t medida_adc;
static uint32_t tick_periodico = 0;
static volatile uint32_t ticks_CAP = 0;

typedef enum {
	MEDIDA_1M,
	MEDIDA_10K,
	MEDIDA_330,
	MEDIDA_OFF
} tipo_medida_t;

tipo_medida_t medida_actual = MEDIDA_330;

void DUT_Configurar(tipo_medida_t medida);
static void imprimir_resistencia(float res_Mohms);
static void Ajustar_Rango(float res_Mohms);
static void imprimir_capacitancia(float pF);
void Descarga(uint8_t activar);
static void Configurar_Timer(void);
static void Restaurar_Timer(void);

void Set_Pin(uint16_t Pin, uint8_t alto) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = Pin;
	if (alto) {
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
		HAL_GPIO_WritePin(GPIOA, Pin, GPIO_PIN_SET);
	} else {
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	if (hadc->Instance == ADC1) {
		medida_adc = HAL_ADC_GetValue(hadc);
		flag_ADC = 1;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM3) {
		ticks_CAP++;
	}
}

void DUT_Iniciar() {
	medida_actual = MEDIDA_330;
	DUT_Configurar(MEDIDA_330);
	HAL_TIM_Base_Start_IT(&htim3);
	HAL_ADC_Start_IT(&hadc1);
}

void DUT_Detener() {
	DUT_Configurar(MEDIDA_OFF);
	HAL_TIM_Base_Stop_IT(&htim3);
	HAL_ADC_Stop_IT(&hadc1);
}

void DUT_Configurar(tipo_medida_t medida) {
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
		Set_Pin(GPIO330R_Pin, 0);
		break;
	default:
		break;
	}
}

static void Configurar_Timer(void) {
	HAL_TIM_Base_Stop_IT(&htim3);

	/* Todos los rangos usan el mismo periodo base: 10 µs/tick
	 * (72 MHz / (71+1) / (9+1) = 100 000 ticks/s → 10 µs/tick) */
	htim3.Init.Prescaler = 71;
	htim3.Init.Period    = 9;

	HAL_TIM_Base_Init(&htim3);
	HAL_TIM_Base_Start_IT(&htim3);
}

static void Restaurar_Timer(void) {
	HAL_TIM_Base_Stop_IT(&htim3);
	htim3.Init.Prescaler = 71;
	htim3.Init.Period    = 999;
	HAL_TIM_Base_Init(&htim3);
	HAL_TIM_Base_Start_IT(&htim3);
}

static float Medida_unica(void) {
	static uint32_t suma_adc = 0;
	static uint8_t contador_muestras = 0;

	if (flag_ADC == 1) {
		flag_ADC = 0;
		suma_adc += medida_adc;
		contador_muestras++;

		if (contador_muestras >= 32) {
			uint16_t promedio = suma_adc / 32;
			suma_adc = 0;
			contador_muestras = 0;

			float referencia;
			switch (medida_actual) {
			case MEDIDA_1M:  referencia = 1.0f;     break;
			case MEDIDA_10K: referencia = 0.01f;    break;
			case MEDIDA_330: referencia = 0.00033f; break;
			default:         referencia = 1.0f;     break;
			}

			return referencia * ((float)promedio / (4095.0f - (float)promedio));
		}
	}

	return -1.0f;
}

uint32_t DUT_Medir(void) {
	static uint8_t flag_adq = 0;
	float res_parcial;

	if (flag_adq == 0 && DUT_estado_parametro == DUT_PARAMETRO_RESISTENCIA) {
		switch (DUT_estado_modo) {
		case DUT_MODO_UNICO:
			if (flag_med_unica == 1) {
				flag_med_unica = 0;
				flag_adq = 1;
			}
			break;
		case DUT_MODO_PERIODICO:
			if (HAL_GetTick() - tick_periodico >= 1000) {
				tick_periodico = HAL_GetTick();
				flag_adq = 1;
			}
			break;
		default:
			break;
		}
	}

	switch (DUT_estado_parametro) {
	case DUT_PARAMETRO_RESISTENCIA:
		if (flag_adq == 1) {
			res_parcial = Medida_unica();
			if (res_parcial != -1.0f) {
				flag_adq = 0;
				resultado = (uint32_t)res_parcial;
				imprimir_resistencia(res_parcial);
				Ajustar_Rango(res_parcial);
				return resultado;
			}
		}
		break;

	case DUT_PARAMETRO_CAPACITANCIA: {
		static enum {
			CAP_ESPERANDO,
			CAP_DESCARGANDO,
			CAP_MIDIENDO
		} estado_cap = CAP_ESPERANDO;

		/* Tick guardado al entrar en CAP_DESCARGANDO, para el timeout */
		static uint32_t tick_descarga = 0;

		switch (estado_cap) {
		case CAP_ESPERANDO:
			switch (DUT_estado_modo) {
			case DUT_MODO_UNICO:
				if (flag_med_unica == 1) {
					HAL_UART_Transmit(&huart1, (uint8_t*)"Midiendo...\r\n", 13, 100);
					flag_med_unica = 0;
					tick_descarga = HAL_GetTick();
					estado_cap = CAP_DESCARGANDO;
				}

				break;
			case DUT_MODO_PERIODICO:
				if (HAL_GetTick() - tick_periodico >= 1000) {
					tick_periodico = HAL_GetTick();
					tick_descarga  = HAL_GetTick();
					estado_cap = CAP_DESCARGANDO;
				}
				break;
			default:
				break;
			}
			break;

		case CAP_DESCARGANDO:
			DUT_Configurar(MEDIDA_OFF); /* Aísla la fuente de tensión    */
			Descarga(1);                /* Drena el componente a masa     */

			if (medida_adc <= ADC_SAT_LOW) {
				/* Descarga completa: continuar con la medición */
				Descarga(0);
				DUT_Configurar(medida_actual);
				estado_cap = CAP_MIDIENDO;
			} else if (HAL_GetTick() - tick_descarga >= TIMEOUT_DESCARGA_MS) {
				/* Timeout: el capacitor no descargó en el tiempo esperado.
				 * Se vuelve al estado inicial para no quedar bloqueado. */
				Descarga(0);
				medida_actual = MEDIDA_330;
				DUT_Configurar(medida_actual);
				estado_cap = CAP_ESPERANDO;
			}
			break;

		case CAP_MIDIENDO: {
			static uint8_t  timer_iniciado = 0;
			static uint32_t tick_timeout   = 0;

			if (!timer_iniciado) {
				ticks_CAP    = 0;
				tick_timeout = HAL_GetTick();
				Configurar_Timer();
				timer_iniciado = 1;
			}

			uint32_t timeout_ms;
			switch (medida_actual) {
			case MEDIDA_1M:  timeout_ms = 15000; break;
			case MEDIDA_10K: timeout_ms =  5000; break;
			case MEDIDA_330: timeout_ms =  5000; break;
			default:         timeout_ms =  5000; break;
			}

			if (HAL_GetTick() - tick_timeout >= timeout_ms) {
				/* El capacitor no alcanzó el umbral: cambiar a rango superior */
				timer_iniciado = 0;
				Restaurar_Timer();

				switch (medida_actual) {
				case MEDIDA_330: medida_actual = MEDIDA_10K; break;
				case MEDIDA_10K: medida_actual = MEDIDA_1M;  break;
				case MEDIDA_1M:  medida_actual = MEDIDA_OFF; break;
				default: break;
				}

				DUT_Configurar(medida_actual);

				if (medida_actual == MEDIDA_OFF) {
					Descarga(1);
					medida_actual = MEDIDA_330; /* resetear para la próxima medición */
					estado_cap = CAP_ESPERANDO;
				} else {
					tick_descarga = HAL_GetTick();
					estado_cap = CAP_DESCARGANDO;
				}
				break;
			}

			/* Verificar con muestra fresca si el capacitor llegó al 63% de VCC */
			if (flag_ADC == 1) {
				flag_ADC = 0;

				if (medida_adc >= 2580) {
					uint32_t ticks = ticks_CAP;
					timer_iniciado = 0;
					Restaurar_Timer();

					/* Todos los rangos tienen el mismo periodo de tick: 10 µs
					 * (72 MHz / (71+1) / (9+1) = 10 µs/tick)              */
					const float ticks_xsegundo = 10e-6f;

					float R_ohms;
					switch (medida_actual) {
					case MEDIDA_1M:  R_ohms = 1000000.0f; break;
					case MEDIDA_10K: R_ohms = 10000.0f;   break;
					case MEDIDA_330: R_ohms = 330.0f;     break;
					default:         R_ohms = 1.0f;       break;
					}

					float cte_tiempo     = ticks * ticks_xsegundo;
					float capacitancia_pF = (cte_tiempo / R_ohms) * 1e12f;

					/* Verificar si conviene cambiar de rango */
					tipo_medida_t nueva = medida_actual;
					switch (medida_actual) {
					case MEDIDA_1M:
						if (capacitancia_pF > 10000.0f)
							nueva = MEDIDA_10K;
						break;
					case MEDIDA_10K:
						if (capacitancia_pF > 1000000.0f)
							nueva = MEDIDA_330;
						else if (capacitancia_pF < 100.0f)
							nueva = MEDIDA_1M;
						break;
					case MEDIDA_330:
						if (capacitancia_pF < 1000000.0f)
							nueva = MEDIDA_10K;
						break;
					default:
						break;
					}

					if (nueva != medida_actual) {
						medida_actual = nueva;
						tick_descarga  = HAL_GetTick();
						estado_cap = CAP_DESCARGANDO;
					} else {
						imprimir_capacitancia(capacitancia_pF);
						estado_cap = CAP_ESPERANDO;
					}
				}
			}
			break;
		} /* CAP_MIDIENDO */
		} /* switch estado_cap */
		break;
	} /* DUT_PARAMETRO_CAPACITANCIA */

	default:
		break;
	}

	return 0;
}

static void Ajustar_Rango(float res_Mohms) {
	tipo_medida_t nueva = medida_actual;

	switch (medida_actual) {
	case MEDIDA_1M:
		if (res_Mohms < UMBRAL_1M_BAJA)
			nueva = MEDIDA_10K;
		break;
	case MEDIDA_10K:
		if (res_Mohms < UMBRAL_10K_BAJA)
			nueva = MEDIDA_330;
		else if (res_Mohms > UMBRAL_10K_SUBE)
			nueva = MEDIDA_1M;
		break;
	case MEDIDA_330:
		if (res_Mohms > UMBRAL_330_SUBE || medida_adc >= ADC_SAT_HIGH)
			nueva = MEDIDA_10K;
		else if (res_Mohms < UMBRAL_330_BAJA || medida_adc <= ADC_SAT_LOW)
			nueva = MEDIDA_OFF;
		break;
	case MEDIDA_OFF:
		nueva = MEDIDA_330;
		break;
	default:
		break;
	}

	if (nueva != medida_actual) {
		medida_actual = nueva;
		DUT_Configurar(medida_actual);
	}
}

static void imprimir_resistencia(float res_Mohms) {
	char buffer[50];
	uint16_t len;
	int32_t entero, decimal;

	switch (medida_actual) {
	case MEDIDA_1M: {
		entero  = (int32_t)res_Mohms;
		decimal = (int32_t)((res_Mohms - (float)entero) * 10000);
		len = sprintf(buffer, "Rango 1M: %ld.%04ld MOhms\r\n", entero, decimal);
		break;
	}
	case MEDIDA_10K: {
		float kohms = res_Mohms * 1000.0f;
		if (kohms > 150.0f || kohms < 5.0f) return;
		entero  = (int32_t)kohms;
		decimal = (int32_t)((kohms - (float)entero) * 1000);
		len = sprintf(buffer, "Rango 10K: %ld.%03ld kOhms\r\n", entero, decimal);
		break;
	}
	case MEDIDA_330: {
		float ohms = res_Mohms * 1000000.0f;
		if (ohms > 3000.0f || ohms < 33.0f) return;
		entero  = (int32_t)ohms;
		decimal = (int32_t)((ohms - (float)entero) * 10);
		len = sprintf(buffer, "Rango 330: %ld.%01ld Ohms\r\n", entero, decimal);
		break;
	}
	case MEDIDA_OFF:
		len = sprintf(buffer, "Fuera de rango: muy bajo\r\n");
		break;
	default: return;
	}

	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 100);
}

static void imprimir_capacitancia(float pF) {
	char buffer[50];
	uint16_t len;
	int32_t entero, decimal;

	switch (medida_actual) {
	case MEDIDA_1M: {
		entero  = (int32_t)pF;
		decimal = (int32_t)((pF - (float)entero) * 10);
		len = sprintf(buffer, "Cap 1M: %ld.%01ld pF\r\n", entero, decimal);
		break;
	}
	case MEDIDA_10K: {
		float nF = pF / 1000.0f;
		entero  = (int32_t)nF;
		decimal = (int32_t)((nF - (float)entero) * 100);
		len = sprintf(buffer, "Cap 10K: %ld.%02ld nF\r\n", entero, decimal);
		break;
	}
	case MEDIDA_330: {
		float uF = pF / 1000000.0f;
		entero  = (int32_t)uF;
		decimal = (int32_t)((uF - (float)entero) * 100);
		len = sprintf(buffer, "Cap 330: %ld.%02ld uF\r\n", entero, decimal);
		break;
	}
	case MEDIDA_OFF:
		len = sprintf(buffer, "Fuera de rango: muy bajo\r\n");
		break;
	default: return;
	}

	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 100);
}

void Descarga(uint8_t activar) {
	if (activar) {

		GPIOB->CRL = (GPIOB->CRL & ~(0xF << 20)) | (0x2 << 20);

		GPIOB->BRR = (1 << 5);
	} else {

		GPIOB->CRL = (GPIOB->CRL & ~(0xF << 20)) | (0x4 << 20);
	}
}
