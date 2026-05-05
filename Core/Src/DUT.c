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


/* =========================================================================================
 * 									NOMENCLATURA DE PREFIJOS:
 * =========================================================================================

	IU: función/variable declarada en el header de IU.h, librería de la interfaz de usuario.
	DUT: función/variable declarada en el header DUT.h, librería del DUT.
	ANTR: función/variable declarada en el header ANTR.h, librería antirrebote.
	Sin prefijo: función/variable local de este .c

-------------------------------------------------------------------------------------------*/


/* =========================================================================================
 * 										Funciones
 * =========================================================================================

 *  DUT_Iniciar(): configura los pines, el ADC y el timer para comenzar a medir: inicia el
 *  rango inicial en 330 Ohms.
 *
 *  DUT_Detener(): pone todos los pines en alta impedancia, detiene el timer y el ADC.
 *
 *  DUT_Medir(): dependiendo del parámetro y modo configurados, gestiona la lógica para las
 *  muestras del ADC. Tanto resistencia o la secuencia descarga/carga para la capacitancia.
 *  Imprime el resultado.
 *
 *  DUT_Configurar(): habilita el pin GPIO correspondiente al rango recibido como parámetro,
 *  poniendo los otros dos en alta impedancia con Set_Pin().
 *
 *  Set_Pin(): configura los GPIO para el alternado de pines en alto o baja impedancia.
 *
 *  Descarga(): controla el pin PB5 de descarga. Al activar, lo configura como salida a GND
 *  para descargar el capacitor; al desactivar, lo deja como alta impedancia.
 *
 *  Configurar_Timer(): configura TIM3 con prescaler 71 y period 9 para obtener ticks de 10 us,
 *  usados como base de tiempo en la medida de capacitancia.
 *
 *  Restaurar_Timer(): devuelve TIM3 a su configuración normal con el trigger periódico del
 *  ADC a 1 ms parala medición de resistencia.
 *
 *  Medida_unica(): acumula 32 muestras ADC, calcula el  promedio y devuelve la resistencia
 *  enMOhms aplicando la fórmula del divisor resistivo. Devuelve -1 mientras no se completaron
 *  las 32 muestras.
 *
 *  Ajustar_Rango(): revisa el resultado de la última medición de resistencia y, si está fuera
 *  de los del rango actual, cambia al rango correspondiente.
 *
 *  imprimir_resistencia(): formatea e imprime el valor de resistencia por UART con la unidad
 *  más conveniente. Avisa si la medida es fuera de escala.
 *
 *  imprimir_capacitancia(): formatea e imprime el valor de capacitancia por UART con la unidad
 *  más conveniente

 * ======================================================================================== */


#define ADC_SAT_HIGH      3890u   // 95% de 4095
#define ADC_SAT_LOW        82u   // 2%  de 4095
#define ADC_63_PERCENT    2580u   // 63% de 4095


#define UMBRAL_1M_BAJA    0.5f
#define UMBRAL_10K_BAJA   0.005f
#define UMBRAL_330_BAJA   0.000033f
#define UMBRAL_10K_SUBE   1.5f
#define UMBRAL_330_SUBE   0.015f

#define TIMEOUT_DESCARGA_MS  5000u


DUT_parametro_t DUT_estado_parametro = DUT_PARAMETRO_RESISTENCIA;
DUT_modo_t      DUT_estado_modo      = DUT_MODO_UNICO;
uint8_t flag_ADC = 0;
uint8_t flag_med_unica = 0;
volatile uint32_t medida_adc;
static uint32_t tick_periodico = 0;
static volatile uint32_t ticks_CAP = 0;

typedef enum {
	MEDIDA_1M,
	MEDIDA_10K,
	MEDIDA_330,
	MEDIDA_OFF
} tipo_medida_t;

static tipo_medida_t medida_actual = MEDIDA_330;

static void DUT_Configurar(tipo_medida_t medida);
static void imprimir_resistencia(float res_Mohms);
static void Ajustar_Rango(float res_Mohms);
static void imprimir_capacitancia(float pF);
static void Descarga(uint8_t activar);
static void Configurar_Timer(void);
static void Restaurar_Timer(void);

static void Set_Pin(uint16_t Pin, uint8_t alto) {
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

static void DUT_Configurar(tipo_medida_t medida) {
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

void DUT_Medir(void) {
	static uint8_t flag_adq = 0;
	float res_parcial;

	if (flag_adq == 0 && DUT_estado_parametro == DUT_PARAMETRO_RESISTENCIA) {
		switch (DUT_estado_modo) {
		case DUT_MODO_UNICO:
			if (flag_med_unica == 1) {
				flag_med_unica = 0;
				flag_adq = 1;
				HAL_UART_Transmit(&huart1, (uint8_t*)"Midiendo...\r\n", 13, 100);
			}
			break;
		case DUT_MODO_PERIODICO:
			if (HAL_GetTick() - tick_periodico >= 100) {
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
				imprimir_resistencia(res_parcial);
				Ajustar_Rango(res_parcial);
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
					flag_med_unica = 0;
					HAL_UART_Transmit(&huart1, (uint8_t*)"Midiendo...\r\n", 13, 100);
					tick_descarga = HAL_GetTick();
					estado_cap = CAP_DESCARGANDO;
				}
				break;
			case DUT_MODO_PERIODICO:
				if (HAL_GetTick() - tick_periodico >= 100) {
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
					case MEDIDA_1M:
						/* Se agotaron todos los rangos */
						HAL_UART_Transmit(&huart1, (uint8_t*)"FUERA DE ESCALA\r\n", 17, 100);
						medida_actual = MEDIDA_330;
						Descarga(1);
						estado_cap = CAP_ESPERANDO;
						break;
					default: break;
					}

					if (estado_cap != CAP_ESPERANDO) {
						DUT_Configurar(medida_actual);
						tick_descarga = HAL_GetTick();
						estado_cap = CAP_DESCARGANDO;
					}
					break;
				}

				/* Verificar con muestra fresca si el capacitor llegó al 63% de VCC */
				if (flag_ADC == 1) {
					flag_ADC = 0;

					if (medida_adc >= ADC_63_PERCENT) {
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

						float cte_tiempo      = ticks * ticks_xsegundo;
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
							tick_descarga = HAL_GetTick();
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
}

static void Ajustar_Rango(float res_Mohms) {
	tipo_medida_t nueva = medida_actual;

	switch (medida_actual) {
	case MEDIDA_1M:
		if (res_Mohms < UMBRAL_1M_BAJA)
			nueva = MEDIDA_10K;
		else if (medida_adc >= ADC_SAT_HIGH) {
			HAL_UART_Transmit(&huart1, (uint8_t*)"FUERA DE ESCALA\r\n", 17, 100);
			return;
		}
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

	if (medida_actual == MEDIDA_1M && medida_adc >= ADC_SAT_HIGH) {
		HAL_UART_Transmit(&huart1, (uint8_t*)"FUERA DE ESCALA\r\n", 17, 100);
		return;
	}

	switch (medida_actual) {
	case MEDIDA_1M: {
		/* Convertir a kOhms para valores >= 1000 kOhms, si no en MOhms */
		if (res_Mohms >= 1.0f) {
			entero  = (int32_t)res_Mohms;
			decimal = (int32_t)((res_Mohms - (float)entero) * 100);
			len = sprintf(buffer, "%ld.%02ld MOhm\r\n", entero, decimal);
		} else {
			float kohms = res_Mohms * 1000.0f;
			entero  = (int32_t)kohms;
			decimal = (int32_t)((kohms - (float)entero) * 10);
			len = sprintf(buffer, "%ld.%01ld kOhm\r\n", entero, decimal);
		}
		break;
	}
	case MEDIDA_10K: {
		float kohms = res_Mohms * 1000.0f;
		if (kohms > 150.0f || kohms < 5.0f) return;
		entero  = (int32_t)kohms;
		decimal = (int32_t)((kohms - (float)entero) * 10);
		len = sprintf(buffer, "%ld.%01ld kOhm\r\n", entero, decimal);
		break;
	}
	case MEDIDA_330: {
		float ohms = res_Mohms * 1000000.0f;
		if (ohms > 3000.0f || ohms < 33.0f) return;
		entero  = (int32_t)ohms;
		decimal = (int32_t)((ohms - (float)entero) * 10);
		len = sprintf(buffer, "%ld.%01ld Ohm\r\n", entero, decimal);
		break;
	}
	case MEDIDA_OFF:
		len = sprintf(buffer, "FUERA DE ESCALA\r\n");
		break;
	default: return;
	}

	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 100);
}

static void imprimir_capacitancia(float pF) {
	char buffer[50];
	uint16_t len;
	int32_t entero, decimal;

	if (pF >= 1000000.0f) {
		/* Mostrar en µF */
		float uF = pF / 1000000.0f;
		entero  = (int32_t)uF;
		decimal = (int32_t)((uF - (float)entero) * 100);
		len = sprintf(buffer, "%ld.%02ld uF\r\n", entero, decimal);
	} else if (pF >= 1000.0f) {
		/* Mostrar en nF */
		float nF = pF / 1000.0f;
		entero  = (int32_t)nF;
		decimal = (int32_t)((nF - (float)entero) * 10);
		len = sprintf(buffer, "%ld.%01ld nF\r\n", entero, decimal);
	} else {
		/* Mostrar en pF */
		entero  = (int32_t)pF;
		decimal = (int32_t)((pF - (float)entero) * 10);
		len = sprintf(buffer, "%ld.%01ld pF\r\n", entero, decimal);
	}

	HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 100);
}

static void Descarga(uint8_t activar) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Descarga_Pin;
	if (activar) {
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIO_Descarga_GPIO_Port, &GPIO_InitStruct);
		HAL_GPIO_WritePin(GPIO_Descarga_GPIO_Port, GPIO_Descarga_Pin, GPIO_PIN_RESET);
	} else {
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIO_Descarga_GPIO_Port, &GPIO_InitStruct);
	}
}
