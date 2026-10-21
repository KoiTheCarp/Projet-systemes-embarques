#include "gestion_modes.h"
#include "grove_button.h"
#include "grove_rgb.h"
#include <stdio.h>

void initialiserModes(void) {
    GroveButton_Init();
    GroveRGB_Init();
}

EtatSysteme executerMachineAEtats(EtatSysteme etatActuel, bool *modeEcoActif) {
    EtatSysteme nouvelEtat = etatActuel;

    // Lecture des boutons poussoirs
    bool btn1 = GroveButton1_IsPressed();
    bool btn2 = GroveButton2_IsPressed();

    switch (etatActuel) {
        case ETAT_STANDARD:
            GroveRGB_SetColor(0, 255, 0); // Vert en mode standard
            *modeEcoActif = false;
            
            if (btn1) {
                nouvelEtat = ETAT_ECO;
            } else if (btn2) {
                nouvelEtat = ETAT_CONFIG;
            }
            break;

        case ETAT_ECO:
            GroveRGB_SetColor(0, 0, 255); // Bleu en mode éco
            *modeEcoActif = true;

            if (btn1) {
                nouvelEtat = ETAT_STANDARD;
            } else if (btn2) {
                nouvelEtat = ETAT_CONFIG;
            }
            break;

        case ETAT_CONFIG:
            GroveRGB_SetColor(255, 165, 0); // Orange en mode configuration
            *modeEcoActif = false;

            if (btn1 || btn2) {
                nouvelEtat = ETAT_STANDARD;
            }
            break;

        case ETAT_MAINTENANCE:
            GroveRGB_SetColor(255, 0, 0); // Rouge en mode maintenance
            *modeEcoActif = false;
            if (btn1) {
                nouvelEtat = ETAT_STANDARD;
            }
            break;

        default:
            nouvelEtat = ETAT_STANDARD;
            break;
    }

    return nouvelEtat;
}
