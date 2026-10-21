#ifndef GESTION_MODES_H
#define GESTION_MODES_H

#include <stdbool.h>
#include "cesi_types.h"

typedef enum {
    ETAT_STANDARD = 0,
    ETAT_ECO,
    ETAT_CONFIG,
    ETAT_MAINTENANCE
} EtatSysteme;

void initialiserModes(void);
EtatSysteme executerMachineAEtats(EtatSysteme etatActuel, bool *modeEcoActif);

#endif
