#include "configuration.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>

#define TIMEOUT_CONFIG_MS (30UL * 60UL * 1000UL) // 30 minutes

static uint32_t debutConfigTick = 0;
static bool configActive = false;

extern UART_HandleTypeDef huart2;

void initialiserConfiguration(void) {
    configActive = false;
}

void gererModeConfiguration(void) {
    if (!configActive) {
        configActive = true;
        debutConfigTick = HAL_GetTick();
        printf("[CONFIG] Entree dans le mode configuration (timeout 30 min).\r\n");
    }

    // --- LECTURE SÉRIE ROBUSTE (DIRECTE PAR REGISTRES) ---
    static char rx_buffer[32];
    static uint8_t rx_index = 0;

    // Nettoyage des erreurs UART si présentes (Overrun, Framing, Noise)
    if (huart2.Instance->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        huart2.Instance->ICR = (USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF);
    }

    // Tant qu'il y a des caractères reçus dans le registre matériel
    while (huart2.Instance->ISR & USART_ISR_RXNE) {
        char c = (char)(huart2.Instance->RDR & 0xFF);

        if (c == '\r' || c == '\n') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0'; // Fin de chaîne

                // Analyse de la commande
                if (strcmp(rx_buffer, "VERSION") == 0 || strcmp(rx_buffer, "version") == 0) {
                    printf(">>> VERSION : Station Meteo STM32 L4 - v1.0\r\n");
                } else {
                    printf(">>> Commande inconnue : %s\r\n", rx_buffer);
                }

                rx_index = 0; // Réinitialisation du buffer
            }
        } else if (rx_index < sizeof(rx_buffer) - 1) {
            rx_buffer[rx_index++] = c;
        }
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