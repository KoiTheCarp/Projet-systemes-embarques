#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdint.h>
#include <stdbool.h>
#include "bme280.h"
#include "grove_rtc_ds1307.h"

// Définition propre de la structure globale des mesures
typedef struct {
    RTC_DateTime dt;
    uint16_t light;
    BME280_Data env;
} MesureMeteo;

void initialiserCapteursGlobaux(void);
MesureMeteo acquerirDonneesMeteo(bool modeEco);

#endif