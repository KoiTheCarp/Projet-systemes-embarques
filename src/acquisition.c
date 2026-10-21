#include "acquisition.h"
#include "bme280.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include <stdbool.h>

// Déclaration externe du bus I2C défini dans le projet
extern I2C_HandleTypeDef hi2c1;

// Instance globale du capteur BME280
static BME280_Handle bme_handle;
static bool bme_initialized = false;

/**
 * @brief Initialise l'ensemble des capteurs globaux de la station météo.
 */
void initialiserCapteursGlobaux(void) {
    // Initialisation du capteur BME280 via le pilote I2C validé
    bme_initialized = BME280_Init(&bme_handle, &hi2c1);

    // Initialisation des autres modules Grove
    GroveLight_Init();
    GroveRTC_Init();
}

/**
 * @brief Récupère les mesures de l'ensemble des capteurs sous forme de structure MesureMeteo.
 */
MesureMeteo acquerirDonneesMeteo(bool modeEco) {
    MesureMeteo mesure = {0};

    // 1. Lecture de l'horloge RTC
    GroveRTC_GetDateTime(&mesure.dt);

    // 2. Lecture du capteur de luminosité
    mesure.light = GroveLight_ReadRaw();

    // 3. Lecture du capteur environnemental BME280
    if (bme_initialized) {
        BME280_Read(&bme_handle, &mesure.env);
    }

    return mesure;
}