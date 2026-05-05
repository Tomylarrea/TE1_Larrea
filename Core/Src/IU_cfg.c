/*
 * interfaz.c
 *
 *  Created on: 1 may 2026
 *      Author: tomyl
 */
#include <stdint.h>
#include "IU_cfg.h"
#include "DUT.h"
#include "main.h"

/*-------------------------------------------------------------

	NOMENCLATURA DE PREFIJOS:

	IU: función/variable declarada en el header de IU.h, librería de la interfaz de usuario.
	DUT: función/variable declarada en el header DUT.h, librería del DUT.
	ANTR: función/variable declarada en el header ANTR.h, librería antirrebote.
	Sin prefijo: función/variable local de este .c

-------------------------------------------------------------*/


static uint8_t color = 1;
static volatile uint8_t flag_UART = 0;
const char* const str_parametro[] = {"resistencia", "capacitancia"};
const char* const str_modo[] = {"medida unica", "medida periodica"};

static volatile char rx_byte;
static char str_tx[80];
static uint8_t size;

typedef enum {
	MENU_PRINCIPAL,
	SUBMENU_PARAMETRO,
	SUBMENU_MODO
} menu_t;

static menu_t estado_menu = MENU_PRINCIPAL;
static uint8_t flag_1er_llamado = 1;  /* Se inicializa como activo para que se grafique el MP ni bien arranque el sistema.
 * Luego solo se reactiva desde IU_iniciar(), de esa manera siempre aparece en el primer
								  llamado */

void imprimir_menu(menu_t menu);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	flag_UART = 1;
	HAL_UART_Receive_IT(&huart1, (uint8_t*)&rx_byte, 1);

}


void IU_menu(void) {
	if (flag_1er_llamado == 1){
		flag_1er_llamado = 0;
		imprimir_menu(estado_menu);
	}

	if (flag_UART == 1) {
		flag_UART = 0;

		if (rx_byte == '\n' || rx_byte == '\r') {
			return;
		}

		switch (estado_menu) {
		case MENU_PRINCIPAL:
			if (rx_byte == '1') {
				estado_menu = SUBMENU_PARAMETRO;
				imprimir_menu(estado_menu);
			} else if (rx_byte == '2') {
				estado_menu = SUBMENU_MODO;
				imprimir_menu(estado_menu);
			} else if (rx_byte == '3') {
				color = !color;   // toggle
				imprimir_menu(estado_menu);
			} else {
				size = sprintf(str_tx, "\r\n%sOpcion invalida.%s\r\n",
						(color==1)?"\033[31m":"", (color==1)?"\033[0m":"");
				HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);
				imprimir_menu(estado_menu);
			}
			break;

		case SUBMENU_PARAMETRO:
			if (rx_byte == '1') {
				DUT_estado_parametro = DUT_PARAMETRO_RESISTENCIA;
				estado_menu = MENU_PRINCIPAL;
				imprimir_menu(estado_menu);
			} else if (rx_byte == '2') {
				DUT_estado_parametro = DUT_PARAMETRO_CAPACITANCIA;
				estado_menu = MENU_PRINCIPAL;
				imprimir_menu(estado_menu);
			} else {
				size = sprintf(str_tx, "\r\n%sOpcion invalida.%s\r\n",
						(color==1)?"\033[31m":"", (color==1)?"\033[0m":"");
				HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);
				imprimir_menu(estado_menu);
			}
			break;

		case SUBMENU_MODO:
			if (rx_byte == '1') {
				DUT_estado_modo = DUT_MODO_UNICO;
				estado_menu = MENU_PRINCIPAL;
				imprimir_menu(estado_menu);
			} else if (rx_byte == '2') {
				DUT_estado_modo = DUT_MODO_PERIODICO;
				estado_menu = MENU_PRINCIPAL;
				imprimir_menu(estado_menu);
			} else {
				size = sprintf(str_tx, "\r\n%sOpcion invalida.%s\r\n",
						(color==1)?"\033[31m":"", (color==1)?"\033[0m":"");
				HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);
				imprimir_menu(estado_menu);
			}
			break;
		}

	}
}

void IU_iniciar(void){
	HAL_UART_Receive_IT(&huart1, (uint8_t*)&rx_byte, 1);
	flag_1er_llamado = 1;
}

// IU_cfg.c

void IU_Detener(void){
    flag_UART = 0;
    HAL_UART_AbortReceive_IT(&huart1);

    // Al salir del menú, si el modo es único disparar la primera medición
    if (DUT_estado_modo == DUT_MODO_UNICO) {
        flag_med_unica = 1;
    }
}

void imprimir_menu(menu_t menu) {
	switch (menu) {
	case MENU_PRINCIPAL:

		size = sprintf(str_tx, "\r\nCONFIGURACION ACTUAL: %s%s%s - %s%s%s\r\n",
				(color==1)?(DUT_estado_parametro==0?"\033[32m":"\033[35m"):"",
						str_parametro[DUT_estado_parametro],
						(color==1)?"\033[0m":"",

								(color==1)?(DUT_estado_modo==0?"\033[34m":"\033[33m"):"",
										str_modo[DUT_estado_modo],
										(color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "1) modificar parametro (%sresistencia%s / %scapacitancia%s)\r\n",
				(color==1)?"\033[32m":"", (color==1)?"\033[0m":"",
						(color==1)?"\033[35m":"", (color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "2) modificar modo (%smedida unica%s / %smedida periodica%s)\r\n",
				(color==1)?"\033[34m":"", (color==1)?"\033[0m":"",
						(color==1)?"\033[33m":"", (color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "3) activar/desactivar color\r\n");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "\r\nIngrese una opcion: \r\n");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);
		break;

	case SUBMENU_PARAMETRO:
		size = sprintf(str_tx, "\r\nPARAMETRO: %s%s%s\r\n",
				(color==1)?(DUT_estado_parametro==0?"\033[32m":"\033[35m"):"",
						str_parametro[DUT_estado_parametro],
						(color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "1) %sresistencia%s\r\n",
				(color==1)?"\033[32m":"", (color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "2) %scapacitancia%s\r\n",
				(color==1)?"\033[35m":"", (color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "\r\nIngrese un parametro: \r\n");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);
		break;

	case SUBMENU_MODO:
		size = sprintf(str_tx, "\r\nMODO: %s%s%s\r\n",
				(color==1)?(DUT_estado_modo==0?"\033[34m":"\033[33m"):"",
						str_modo[DUT_estado_modo],
						(color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "1) %smedida unica%s\r\n",
				(color==1)?"\033[34m":"", (color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "2) %smedida periodica%s\r\n",
				(color==1)?"\033[33m":"", (color==1)?"\033[0m":"");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);

		size = sprintf(str_tx, "\r\nIngrese un modo: \r\n");
		HAL_UART_Transmit(&huart1, (uint8_t*)str_tx, size, 100);
		break;
	}
}
