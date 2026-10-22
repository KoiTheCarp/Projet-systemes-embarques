#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdbool.h>

/*
 * Ecriture de lignes de texte dans des fichiers de la carte SD, en FAT32 (FatFs).
 * Les fichiers se lisent ensuite normalement sur un PC.
 * Le pilote SPI de la carte est dans sd_diskio.c.
 */

/* Monte la carte (FAT32). Renvoie true si la carte est prete. */
bool SDLogger_Init(void);

/* Dernier code FatFs : 0 = OK, 3 = carte absente ou muette, 13 = carte pas en FAT32 */
int SDLogger_GetLastInitStatus(void);

/* Ajoute la ligne a la fin du fichier (cree le fichier s'il n'existe pas) */
bool SDLogger_WriteLine(const char *filename, const char *line);

#endif
