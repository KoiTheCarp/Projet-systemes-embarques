#ifndef SD_DISKIO_H
#define SD_DISKIO_H

/*
 * Pilote bas niveau de la carte SD (SPI). Les fonctions disk_* utilisees
 * par FatFs sont dans sd_diskio.c ; ici seulement ce dont sd_logger.c a besoin.
 */

/* Oublie la carte : elle sera reinitialisee au prochain montage (carte retiree ou changee) */
void SdDisk_Forget(void);

#endif
