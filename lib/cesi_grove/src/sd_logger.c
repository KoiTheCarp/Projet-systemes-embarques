#include "sd_logger.h"
#include "sd_diskio.h"
#include "ff.h"
#include "grove_rtc_ds1307.h"
#include <stdio.h>
#include <string.h>

/*
 * FatFs gere le systeme de fichiers FAT32 : il sait ou ecrire chaque fichier.
 * sd_diskio.c lui fournit la lecture et l'ecriture des secteurs de 512 octets.
 */
static FATFS g_fs;
static bool g_mounted = false;
static FRESULT g_last_status = FR_NOT_READY;

static bool SDLogger_Mount(void)
{
    /* 1 = monter tout de suite : on sait des maintenant si la carte repond */
    g_last_status = f_mount(&g_fs, "", 1);
    g_mounted = (g_last_status == FR_OK);
    return g_mounted;
}

bool SDLogger_Init(void)
{
    return SDLogger_Mount();
}

int SDLogger_GetLastInitStatus(void)
{
    return (int)g_last_status;
}

bool SDLogger_WriteLine(const char *filename, const char *line)
{
    FIL file;
    FRESULT res;
    FRESULT close_res;
    UINT len;
    UINT written = 0;

    if ((filename == 0) || (line == 0))
    {
        return false;
    }

    /* Carte absente au demarrage, ou retiree puis remise : on reessaie de la monter */
    if (!g_mounted && !SDLogger_Mount())
    {
        printf("[SD] carte non montee (code FatFs %d)\r\n", (int)g_last_status);
        return false;
    }

    /* Ouvre le fichier en ajout a la fin. Il est cree s'il n'existe pas. */
    res = f_open(&file, filename, FA_OPEN_APPEND | FA_WRITE);
    if (res == FR_OK)
    {
        len = (UINT)strlen(line);
        res = f_write(&file, line, len, &written);
        if ((res == FR_OK) && (written != len))
        {
            res = FR_DENIED;    /* carte pleine */
        }

        /* f_close ecrit pour de vrai sur la carte : sans lui, la ligne reste en memoire */
        close_res = f_close(&file);
        if (res == FR_OK)
        {
            res = close_res;
        }
    }

    if (res != FR_OK)
    {
        printf("[SD] echec d'ecriture dans %s (code FatFs %d)\r\n", filename, (int)res);
        g_last_status = res;
        g_mounted = false;
        SdDisk_Forget();        /* la carte sera reinitialisee au prochain essai */
        return false;
    }

    return true;
}

/*
 * Date et heure des fichiers, demandees par FatFs a chaque ecriture.
 * Format FAT : annee depuis 1980, mois, jour, heures, minutes, secondes / 2.
 */
DWORD get_fattime(void)
{
    RTC_DateTime dt;

    if (!GroveRTC_GetDateTime(&dt) || (dt.year < 1980U) || (dt.month == 0U) || (dt.day == 0U))
    {
        /* RTC illisible ou pas reglee : 1er janvier 2026, 00:00 */
        dt.year = 2026U;
        dt.month = 1U;
        dt.day = 1U;
        dt.hours = 0U;
        dt.minutes = 0U;
        dt.seconds = 0U;
    }

    return ((DWORD)(dt.year - 1980U) << 25) | ((DWORD)dt.month << 21) | ((DWORD)dt.day << 16)
         | ((DWORD)dt.hours << 11) | ((DWORD)dt.minutes << 5) | ((DWORD)dt.seconds >> 1);
}
