/*
 * port/3ds/log_3ds.h
 *
 * The log on the Nintendo 3DS: stdout and stderr to the bottom screen and to
 * a file on the SD card (log_3ds.c).
 */
#ifndef ICO_3DS_LOG_H
#define ICO_3DS_LOG_H

/* Opens the file at path and sends stdout and stderr to it and to the
   console. Returns 0, or -1 when the file cannot be written (the console
   still gets everything). */
int ico_3ds_log_open(const char *path);

/* Writes out what the buffers hold (the status panel does it every period,
   so a crash loses little). */
void ico_3ds_log_flush(void);

/* Flushes and closes the file (at exit). */
void ico_3ds_log_close(void);

#endif /* ICO_3DS_LOG_H */
