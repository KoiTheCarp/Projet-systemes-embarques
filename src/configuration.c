#include "configuration.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>

#define TIMEOUT_CONFIG_MS (30UL * 60UL * 1000UL) // 30 minutes

static uint32_t debutConfigTick = 0;
static bool configActive = false;

void initialiserConfiguration(void) {
    configActive = false;
}

void gererModeConfiguration(void) {
    if (!configActive) {
        configActive = true;
        debutConfigTick = HAL_GetTick();
        printf("[CONFIG] Entree dans le mode configuration (timeout 30 min).\r\n");
    }
}

bool verifierTimeoutConfiguration(void) {
    if (!configActive) {
        return false;
    }

    if ((HAL_GetTick() - debutConfigTick) >= TIMEOUT_CONFIG_MS) {
        configActive = false;
        printf("[CONFIG] Timeout de 30 minutes atteint. Sortie du mode configuration.\r\n");
        return true;
    }

    return false;
}
