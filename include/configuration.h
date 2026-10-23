#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>
#include <stdint.h>

void initialiserConfiguration(void);
void gererModeConfiguration(void);
bool verifierTimeoutConfiguration(void);

// Lecture des paramètres enregistrés (valeurs chargées depuis la mémoire au démarrage)
uint32_t obtenirLogIntervalMs(void);
uint32_t obtenirFileMaxSize(void);
uint32_t obtenirTimeoutCapteurMs(void);

#endif
