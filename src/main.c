#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32l4xx_hal.h"
#include "cesi_types.h"
#include "setup.h"
#include "board_pins.h"

// Inclusions des pilotes matériels
#include "bme280.h"
#include "grove_button.h"
#include "grove_gps_air530z.h"
#include "grove_light.h"
#include "grove_rgb.h"
#include "grove_rtc_ds1307.h"

// Déclaration du gestionnaire UART du ST-Link (USART2 sur les Nucleo)
extern UART_HandleTypeDef huart2;

// Déclaration externe du gestionnaire I2C
extern I2C_HandleTypeDef hi2c1;

// Définition des modes de fonctionnement
typedef enum {
    MODE_STANDARD,
    MODE_CONFIGURATION,
    MODE_MAINTENANCE,
    MODE_ECONOMIC
} SystemMode_t;

// Variables globales d'état
static SystemMode_t current_mode = MODE_STANDARD;
static SystemMode_t previous_mode = MODE_STANDARD;

// Gestion des appuis longs pour les boutons
static uint32_t btn_red_press_start = 0;
static uint32_t btn_green_press_start = 0;
static bool btn_red_timer_active = false;
static bool btn_green_timer_active = false;

// Paramètres configurables (simulés)
static uint32_t log_interval_ms = 10000;

// Application de la couleur de la LED selon le mode
static void Update_System_LED(SystemMode_t mode) {
    switch (mode) {
        case MODE_STANDARD:
            GroveRGB_SetColor(0, 255, 0);     // Vert continu
            break;
        case MODE_CONFIGURATION:
            GroveRGB_SetColor(255, 255, 0);   // Jaune continu
            break;
        case MODE_ECONOMIC:
            GroveRGB_SetColor(0, 0, 255);     // Bleu continu
            break;
        case MODE_MAINTENANCE:
            GroveRGB_SetColor(255, 128, 0);   // Orange continu
            break;
    }
}

int main(void) {
    // 1. Initialisation globale du système (HAL, Horloge, I2C, SPI, UART debug)
    Global_Init();
    
    setvbuf(stdout, NULL, _IONBF, 0);

    // Délai de courtoisie pour laisser le moniteur série de VS Code s'ouvrir
    HAL_Delay(2000);

    // Initialisation des modules Grove
    GroveLight_Init();
    GroveButton_Init();
    GroveRGB_Init();
    GroveRTC_Init();
    GroveGPS_Init(); // Initialisation du GPS sur l'UART4 (Port A0)

/*
    // --- FORÇAGE UNIQUE DE L'HEURE (Optionnel) ---
    RTC_DateTime initial_dt = {
        .seconds = 0,
        .minutes = 22,  
        .hours = 11,   
        .day = 21,     
        .month = 10,   
        .year = 2026   
    };
    GroveRTC_SetDateTime(&initial_dt);
    printf("[RTC] Heure forcee avec succes !\r\n");
*/

    // 2. Initialisation du capteur BME280
    BME280_Handle bme;
    bool bme_ok = BME280_Init(&bme, &hi2c1);

    // 3. Choix du mode au démarrage (Bouton 2 pressé = Mode configuration)
    if (GroveButton2_IsPressed()) {
        current_mode = MODE_CONFIGURATION;
    } else {
        current_mode = MODE_STANDARD;
    }

    Update_System_LED(current_mode);
    printf("\r\n=== STATION METEO DEMARREE - Mode actuel : %d ===\r\n", current_mode);

    uint32_t last_log_tick = HAL_GetTick();
    uint32_t config_start_tick = HAL_GetTick();

    while (1) {
        uint32_t current_tick = HAL_GetTick();
        bool btn_red = GroveButton2_IsPressed();
        bool btn_green = GroveButton1_IsPressed();

        // --- GESTION DES APPUIS LONGS (Boutons 5 secondes) ---
        // Bouton Rouge
        if (btn_red) {
            if (!btn_red_timer_active) {
                btn_red_press_start = current_tick;
                btn_red_timer_active = true;
            } else if ((current_tick - btn_red_press_start) >= 5000) {
                if (current_mode == MODE_MAINTENANCE) {
                    current_mode = previous_mode; 
                    printf("[MODE] Sortie de Maintenance -> Retour au mode precedent\r\n");
                } else if (current_mode == MODE_ECONOMIC) {
                    current_mode = MODE_STANDARD; 
                    printf("[MODE] Sortie Mode Eco -> Retour Standard\r\n");
                } else if (current_mode == MODE_STANDARD) {
                    previous_mode = current_mode;
                    current_mode = MODE_MAINTENANCE; 
                    printf("[MODE] Entree en mode Maintenance\r\n");
                }
                Update_System_LED(current_mode);
                btn_red_timer_active = false;
                HAL_Delay(1000); 
            }
        } else {
            btn_red_timer_active = false;
        }

        // Bouton Vert - Accessible depuis le mode standard
        if (btn_green && current_mode == MODE_STANDARD) {
            if (!btn_green_timer_active) {
                btn_green_press_start = current_tick;
                btn_green_timer_active = true;
            } else if ((current_tick - btn_green_press_start) >= 5000) {
                current_mode = MODE_ECONOMIC; 
                Update_System_LED(current_mode);
                printf("[MODE] Entree en Mode Economique\r\n");
                btn_green_timer_active = false;
                HAL_Delay(1000); 
            }
        } else {
            btn_green_timer_active = false;
        }

        // --- TRAITEMENT SELON LE MODE ACTIF ---
        switch (current_mode) {
            
            case MODE_STANDARD:
            case MODE_ECONOMIC: {
                uint32_t active_interval = (current_mode == MODE_ECONOMIC) ? (log_interval_ms * 2) : log_interval_ms;

                if ((current_tick - last_log_tick) >= active_interval) {
                    last_log_tick = current_tick;

                    // Lecture Horloge RTC
                    RTC_DateTime dt;
                    GroveRTC_GetDateTime(&dt);

                    // Lecture Luminosité
                    uint16_t light = GroveLight_ReadRaw();

                    // Lecture BME280
                    BME280_Data env;
                    bool read_success = bme_ok && BME280_Read(&bme, &env);

                    // Lecture GPS Air530Z (Timeout de 3000 ms pour capturer la trame GGA)
                    GPS_Data gps_info;
                    bool gps_ok = GroveGPS_ReadGGA(&gps_info, 3000);

                    // Affichage des logs complets
                    printf("[%02u/%02u/%04u %02u:%02u:%02u] Mode:%d | Light:%u | ",
                           dt.day, dt.month, dt.year, dt.hours, dt.minutes, dt.seconds,
                           current_mode, light);

                    if (read_success) {
                        printf("Temp:%.2fC Pres:%.2fhPa Hum:%.2f%% | ",
                               env.temperature_c, env.pressure_hpa, env.humidity_percent);
                    } else {
                        printf("BME280:NA | ");
                    }

                    if (gps_ok) {
                        printf("Lat:%.4f Lon:%.4f (Sats:%u)\r\n", 
                               gps_info.latitude, gps_info.longitude, gps_info.satellites);
                    } else {
                        printf("GPS: En attente trame GGA...\r\n");
                    }
                }
                break;
            }

            case MODE_CONFIGURATION: {
                if ((current_tick - config_start_tick) > (30UL * 60UL * 1000UL)) {
                    current_mode = MODE_STANDARD;
                    Update_System_LED(current_mode);
                    printf("[MODE] Inactivite 30 min -> Bascule automatique en mode Standard\r\n");
                }
                HAL_Delay(100);
                break;
            }

            case MODE_MAINTENANCE: {
                RTC_DateTime dt;
                    GroveRTC_GetDateTime(&dt);
                BME280_Data env;
                if (bme_ok && BME280_Read(&bme, &env)) {
                    printf("[MAINTENANCE LIVE] %02u:%02u:%02u -> Temp: %.2fC | Hum: %.2f%%\r\n",
                           dt.hours, dt.minutes, dt.seconds, env.temperature_c, env.humidity_percent);
                }
                HAL_Delay(2000);
                break;
            }
        }

        HAL_Delay(50);
    }

    return 0;
}