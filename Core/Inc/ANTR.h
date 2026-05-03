#ifndef INC_ANTR_H_
#define INC_ANTR_H_

#include "main.h"
#include <stdint.h>

typedef enum {
    ANTR_PULLDOWN = 0,
    ANTR_PULLUP   = 1
} ANTR_Tipo_pull;

// ANTRs discretos en lugar de estados continuos
typedef enum {
    ANTR_NINGUNO    = 0,
    ANTR_PRESIONADO = 1,
    ANTR_LIBERADO   = 2
} ANTR_Boton;

typedef enum {
    ANTR_ESPERANDO_FLANCO = 0,
    ANTR_ANTR      = 1
} ANTR_Estado;

typedef struct {
    GPIO_TypeDef      *port;
    uint16_t           PIN;
    ANTR_Tipo_pull          pull;
    volatile ANTR_Estado ANTR_estado;
    uint32_t           tick_flanco;
} ANTR_Pulsador;

void ANTR_iniciar(ANTR_Pulsador *p, GPIO_TypeDef *port, uint16_t PIN, ANTR_Tipo_pull pull);
void ANTR_Flanco(ANTR_Pulsador *p);
ANTR_Boton ANTR_Procesar(ANTR_Pulsador *p);

#endif /* INC_ANTR_H_ */
