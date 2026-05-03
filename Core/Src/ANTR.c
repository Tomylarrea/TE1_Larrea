#include "ANTR.h"
#include "main.h"

#define TIEMPO_REBOTE 20

void ANTR_iniciar(ANTR_Pulsador *p, GPIO_TypeDef *port, uint16_t PIN, ANTR_Tipo_pull pull) {
    p->port = port;
    p->PIN = PIN;
    p->pull = pull;
    p->ANTR_estado = ANTR_ESPERANDO_FLANCO;
    p->tick_flanco = 0;
}


void ANTR_Flanco(ANTR_Pulsador *p) {
    if (p->ANTR_estado == ANTR_ESPERANDO_FLANCO) {
        p->ANTR_estado = ANTR_ANTR;
        p->tick_flanco = HAL_GetTick();
    }

}


ANTR_Boton ANTR_Procesar(ANTR_Pulsador *p) {
    if (p->ANTR_estado == ANTR_ANTR) {

        if ((HAL_GetTick() - p->tick_flanco) >= TIEMPO_REBOTE) {

            p->ANTR_estado = ANTR_ESPERANDO_FLANCO;

            GPIO_PinState pin = HAL_GPIO_ReadPin(p->port, p->PIN);
            uint8_t nivel = (pin == GPIO_PIN_SET) ? 1 : 0;

            if (p->pull == ANTR_PULLUP) {
                nivel = !nivel;
            }

            if (nivel == 1) {
                return ANTR_PRESIONADO;
            } else {
                return ANTR_LIBERADO;
            }
        }
    }

    return ANTR_NINGUNO;
}
