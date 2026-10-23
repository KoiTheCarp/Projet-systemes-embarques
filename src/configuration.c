#include "configuration.h"
#include "stm32l4xx_hal.h"
#include "grove_rtc_ds1307.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define TIMEOUT_CONFIG_MS (30UL * 60UL * 1000UL) // 30 minutes

#define VERSION_PROGRAMME "v1.0"
#define NUMERO_LOT        "LOT-2026-001"

// Valeurs par défaut des paramètres
#define DEFAUT_LOG_INTERVALL 10    // minutes
#define DEFAUT_FILE_MAX_SIZE 4096  // octets
#define DEFAUT_TIMEOUT       30    // secondes

// Le STM32L476 n'a pas d'EEPROM : on l'émule avec la dernière page de la flash (2 ko).
// Le programme est loin de remplir 1 Mo, cette page reste libre.
#define ADRESSE_EEPROM (FLASH_BASE + FLASH_SIZE - FLASH_PAGE_SIZE)
#define MAGIC_EEPROM   0xC0F16001UL // permet de savoir si la page contient déjà des paramètres

typedef struct {
    uint32_t magic;
    uint32_t logIntervall; // minutes
    uint32_t fileMaxSize;  // octets
    uint32_t timeout;      // secondes
} ParametresConfig;

// La flash s'écrit par blocs de 8 octets : on range la structure dans un tableau de uint64_t
typedef union {
    ParametresConfig p;
    uint64_t mots[(sizeof(ParametresConfig) + 7) / 8];
} BlocEeprom;

static BlocEeprom params;

static uint32_t debutConfigTick = 0;
static bool configActive = false;

extern UART_HandleTypeDef huart2;

static const char *JOURS[7] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

static void chargerValeursParDefaut(void) {
    params.p.magic = MAGIC_EEPROM;
    params.p.logIntervall = DEFAUT_LOG_INTERVALL;
    params.p.fileMaxSize = DEFAUT_FILE_MAX_SIZE;
    params.p.timeout = DEFAUT_TIMEOUT;
}

// Efface la page puis y écrit les paramètres actuels
static bool sauvegarderParametres(void) {
    FLASH_EraseInitTypeDef effacement = {0};
    uint32_t erreurPage = 0;
    bool ok = true;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    effacement.TypeErase = FLASH_TYPEERASE_PAGES;
    effacement.Banks = FLASH_BANK_2;
    effacement.Page = (ADRESSE_EEPROM - FLASH_BASE - FLASH_BANK_SIZE) / FLASH_PAGE_SIZE;
    effacement.NbPages = 1;

    if (HAL_FLASHEx_Erase(&effacement, &erreurPage) != HAL_OK) {
        ok = false;
    }

    for (uint32_t i = 0; ok && i < sizeof(params.mots) / 8; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ADRESSE_EEPROM + i * 8, params.mots[i]) != HAL_OK) {
            ok = false;
        }
    }

    HAL_FLASH_Lock();

    if (!ok) {
        printf(">>> Erreur d'ecriture en memoire\r\n");
    }
    return ok;
}

void initialiserConfiguration(void) {
    configActive = false;

    // Lecture des paramètres enregistrés ; si la page est vide, on prend les valeurs par défaut
    memcpy(&params, (const void *)ADRESSE_EEPROM, sizeof(params));
    if (params.p.magic != MAGIC_EEPROM) {
        chargerValeursParDefaut();
        sauvegarderParametres();
    }
}

uint32_t obtenirLogIntervalMs(void) {
    return params.p.logIntervall * 60UL * 1000UL;
}

uint32_t obtenirFileMaxSize(void) {
    return params.p.fileMaxSize;
}

uint32_t obtenirTimeoutCapteurMs(void) {
    return params.p.timeout * 1000UL;
}

// CLOCK=HH:MM:SS
static void commandeClock(const char *valeur) {
    unsigned int h, m, s;
    RTC_DateTime dt;

    if (sscanf(valeur, "%u:%u:%u", &h, &m, &s) != 3 || h > 23 || m > 59 || s > 59) {
        printf(">>> Format attendu : CLOCK=HEURE{0-23}:MINUTE{0-59}:SECONDE{0-59}\r\n");
        return;
    }
    if (!GroveRTC_GetDateTime(&dt)) {
        printf(">>> Erreur : RTC injoignable\r\n");
        return;
    }
    dt.hours = h;
    dt.minutes = m;
    dt.seconds = s;
    if (GroveRTC_SetDateTime(&dt)) {
        printf(">>> Heure reglee : %02u:%02u:%02u\r\n", h, m, s);
    } else {
        printf(">>> Erreur : RTC injoignable\r\n");
    }
}

// DATE=MM,JJ,AAAA
static void commandeDate(const char *valeur) {
    unsigned int mois, jour, annee;
    RTC_DateTime dt;

    if (sscanf(valeur, "%u,%u,%u", &mois, &jour, &annee) != 3
        || mois < 1 || mois > 12 || jour < 1 || jour > 31 || annee < 2000 || annee > 2099) {
        printf(">>> Format attendu : DATE=MOIS{1-12},JOUR{1-31},ANNEE{2000-2099}\r\n");
        return;
    }
    if (!GroveRTC_GetDateTime(&dt)) {
        printf(">>> Erreur : RTC injoignable\r\n");
        return;
    }
    dt.month = mois;
    dt.day = jour;
    dt.year = annee;
    if (GroveRTC_SetDateTime(&dt)) {
        printf(">>> Date reglee : %02u/%02u/%04u\r\n", jour, mois, annee);
    } else {
        printf(">>> Erreur : RTC injoignable\r\n");
    }
}

// DAY=MON..SUN (le DS1307 code le jour de 1 à 7)
static void commandeDay(const char *valeur) {
    RTC_DateTime dt;

    for (uint8_t i = 0; i < 7; i++) {
        if (strcmp(valeur, JOURS[i]) == 0) {
            if (!GroveRTC_GetDateTime(&dt)) {
                printf(">>> Erreur : RTC injoignable\r\n");
                return;
            }
            dt.dayOfWeek = i + 1;
            if (GroveRTC_SetDateTime(&dt)) {
                printf(">>> Jour regle : %s\r\n", JOURS[i]);
            } else {
                printf(">>> Erreur : RTC injoignable\r\n");
            }
            return;
        }
    }
    printf(">>> Format attendu : DAY={MON,TUE,WED,THU,FRI,SAT,SUN}\r\n");
}

// Pour LOG_INTERVALL, FILE_MAX_SIZE et TIMEOUT : lit un entier > 0, le range et sauvegarde
static void commandeParametre(const char *nom, const char *valeur, uint32_t *parametre, const char *unite) {
    unsigned long nombre;
    char reste;

    if (sscanf(valeur, "%lu%c", &nombre, &reste) != 1 || nombre == 0) {
        printf(">>> Valeur invalide pour %s (entier > 0 attendu)\r\n", nom);
        return;
    }
    *parametre = nombre;
    if (sauvegarderParametres()) {
        printf(">>> %s = %lu %s (enregistre)\r\n", nom, nombre, unite);
    }
}

static void executerCommande(char *commande) {
    // Commandes acceptées en minuscules comme en majuscules
    for (char *c = commande; *c; c++) {
        *c = (char)toupper((unsigned char)*c);
    }

    // Sépare "NOM=VALEUR" en deux chaînes
    char *valeur = strchr(commande, '=');
    if (valeur != NULL) {
        *valeur = '\0';
        valeur++;
    }

    if (strcmp(commande, "VERSION") == 0) {
        printf(">>> VERSION : Station Meteo STM32 L4 - %s - lot %s\r\n", VERSION_PROGRAMME, NUMERO_LOT);
    } else if (strcmp(commande, "RESET") == 0) {
        chargerValeursParDefaut();
        if (sauvegarderParametres()) {
            printf(">>> Parametres remis par defaut : LOG_INTERVALL=%d, FILE_MAX_SIZE=%d, TIMEOUT=%d\r\n",
                   DEFAUT_LOG_INTERVALL, DEFAUT_FILE_MAX_SIZE, DEFAUT_TIMEOUT);
        }
    } else if (valeur == NULL) {
        printf(">>> Commande inconnue : %s\r\n", commande);
    } else if (strcmp(commande, "LOG_INTERVALL") == 0) {
        commandeParametre("LOG_INTERVALL", valeur, &params.p.logIntervall, "min");
    } else if (strcmp(commande, "FILE_MAX_SIZE") == 0) {
        commandeParametre("FILE_MAX_SIZE", valeur, &params.p.fileMaxSize, "octets");
    } else if (strcmp(commande, "TIMEOUT") == 0) {
        commandeParametre("TIMEOUT", valeur, &params.p.timeout, "s");
    } else if (strcmp(commande, "CLOCK") == 0) {
        commandeClock(valeur);
    } else if (strcmp(commande, "DATE") == 0) {
        commandeDate(valeur);
    } else if (strcmp(commande, "DAY") == 0) {
        commandeDay(valeur);
    } else {
        printf(">>> Commande inconnue : %s\r\n", commande);
    }
}

void gererModeConfiguration(void) {
    if (!configActive) {
        configActive = true;
        debutConfigTick = HAL_GetTick();
        printf("[CONFIG] Entree dans le mode configuration (timeout 30 min).\r\n");
        printf("[CONFIG] Commandes : LOG_INTERVALL=, FILE_MAX_SIZE=, TIMEOUT=, RESET, VERSION, CLOCK=, DATE=, DAY=\r\n");
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

                executerCommande(rx_buffer);

                // Une commande remet à zéro le délai d'inactivité de 30 minutes
                debutConfigTick = HAL_GetTick();
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
