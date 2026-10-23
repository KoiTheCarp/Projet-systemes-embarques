#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32l4xx_hal.h"
#include "cesi_types.h"
#include "setup.h"
#include "board_pins.h"
#include "configuration.h"

// Inclusions des pilotes matériels
#include "bme280.h"
#include "grove_button.h"
#include "grove_gps_air530z.h"
#include "grove_light.h"
#include "grove_rgb.h"
#include "grove_rtc_ds1307.h"
#include "sd_logger.h"

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

    // Délai de courtoisie pour laisser le moniteur série s'ouvrir
    HAL_Delay(2000);

    // Initialisation des modules Grove
    GroveLight_Init();
    GroveButton_Init();
    GroveRGB_Init();
    GroveRTC_Init();
    GroveGPS_Init(); // Initialisation du GPS sur l'UART4 (Port A0)

    // Initialisation de la carte SD
    if (!SDLogger_Init()) {
        printf("[SD] Erreur d'initialisation de la carte SD (Code : %d)\r\n", SDLogger_GetLastInitStatus());
    } else {
        printf("[SD] Carte SD initialisee avec succes.\r\n");
    }

    // 2. Initialisation du capteur BME280
    BME280_Handle bme;
    bool bme_ok = BME280_Init(&bme, &hi2c1);

    // Initialisation du module de configuration
    initialiserConfiguration();

    // 3. Choix du mode au démarrage (Bouton 2 pressé = Mode configuration)
    if (GroveButton2_IsPressed()) {
        current_mode = MODE_CONFIGURATION;
    } else {
        current_mode = MODE_STANDARD;
    }

    Update_System_LED(current_mode);
    printf("\r\n=== STATION METEO DEMARREE - Mode actuel : %d ===\r\n", current_mode);

    uint32_t last_log_tick = HAL_GetTick();

    while (1) {
        uint32_t current_tick = HAL_GetTick();
        bool btn_red = GroveButton2_IsPressed();
        bool btn_green = GroveButton1_IsPressed();

        // --- GESTION DES APPUIS LONGS (Boutons 5 secondes) ---
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
                // Intervalle réglé par la commande LOG_INTERVALL (doublé en mode éco)
                uint32_t log_interval_ms = obtenirLogIntervalMs();
                uint32_t active_interval = (current_mode == MODE_ECONOMIC) ? (log_interval_ms * 2) : log_interval_ms;

                if ((current_tick - last_log_tick) >= active_interval) {
                    last_log_tick = current_tick;

                    // 1. Lecture Horloge RTC
                    RTC_DateTime dt;
                    GroveRTC_GetDateTime(&dt);

                    // 2. Lecture Luminosité
                    uint16_t light = GroveLight_ReadRaw();

                    // 3. Lecture BME280
                    BME280_Data env;
                    bool read_success = bme_ok && BME280_Read(&bme, &env);

                    // 4. Lecture GPS Air530Z
                    GPS_Data gps_info;
                    bool gps_ok = GroveGPS_ReadGGA(&gps_info, 3000);

                    // --- FORMATAGE DES CHAÎNES DE DONNÉES ---
                    char bme_str[64];
                    char gps_str[64];
                    char log_line[256];
                    char filename[32];

                    snprintf(filename, sizeof(filename), "%02u%02u%02u_0.LOG", 
                             dt.year % 100, dt.month, dt.day);

                    if (read_success) {
                        snprintf(bme_str, sizeof(bme_str), "Temp:%.2fC Pres:%.2fhPa Hum:%.2f%%", 
                                 env.temperature_c, env.pressure_hpa, env.humidity_percent);
                    } else {
                        snprintf(bme_str, sizeof(bme_str), "BME280:NA");
                    }

                    if (gps_ok) {
                        snprintf(gps_str, sizeof(gps_str), "Lat:%.4f Lon:%.4f Sats:%u", 
                                 gps_info.latitude, gps_info.longitude, gps_info.satellites);
                    } else {
                        snprintf(gps_str, sizeof(gps_str), "GPS:NA");
                    }

                    snprintf(log_line, sizeof(log_line), "[%02u/%02u/%04u %02u:%02u:%02u] Mode:%d | Light:%u | %s | %s\r\n",
                             dt.day, dt.month, dt.year, dt.hours, dt.minutes, dt.seconds,
                             current_mode, light, bme_str, gps_str);

                    printf("%s", log_line);

                    if (SDLogger_WriteLine(filename, log_line)) {
                        printf("[SD] Ecriture reussie dans %s\r\n", filename);
                    } else {
                        printf("[SD] Erreur d'ecriture\r\n");
                    }
                }
                break;
            }

            case MODE_CONFIGURATION: {
                // Gestion de la configuration et des commandes série via configuration.c
                gererModeConfiguration();

                // Vérification du timeout de 30 minutes
                if (verifierTimeoutConfiguration()) {
                    current_mode = MODE_STANDARD;
                    Update_System_LED(current_mode);
                }
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