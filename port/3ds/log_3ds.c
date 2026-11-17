/*
 * port/3ds/log_3ds.c
 *
 * The log on the Nintendo 3DS (log_3ds.h): everything printed to stdout and
 * stderr goes both to the bottom screen's console and to a file on the SD
 * card, so a run can be read back on a PC.
 *
 * libctru's console is the device behind file descriptor 1 (STDOUT_FILENO).
 * The log replaces the stdout and stderr streams with one stream of its
 * own (newlib's funopen) whose writes go to that descriptor and to the
 * file. The status panel (status_3ds.c) writes to the descriptor directly,
 * so its lines are not in the file.
 *
 * Only the main thread's streams are replaced (newlib keeps the standard
 * streams per thread); a thread started later prints to the console only.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log_3ds.h"

static FILE *s_file; /* the SD card file, fully buffered */
static FILE *s_tee;  /* the stream stdout and stderr now are */

/* funopen's write callback: the console first, then the file. */
static int tee_write(void *cookie, const char *buf, int n)
{
    (void)cookie;
    if (n <= 0) {
        return 0;
    }
    (void)write(STDOUT_FILENO, buf, (size_t)n);
    if (s_file != NULL) {
        fwrite(buf, 1, (size_t)n, s_file);
    }
    return n;
}

int ico_3ds_log_open(const char *path)
{
    s_file = fopen(path, "w");
    if (s_file != NULL) {
        /* a large buffer: the SD card is slow with small writes */
        setvbuf(s_file, NULL, _IOFBF, 16 * 1024);
    }
    s_tee = funopen(NULL, NULL, tee_write, NULL, NULL);
    if (s_tee == NULL) {
        return -1;
    }
    /* line buffered: a line reaches the screen as soon as it is complete */
    setvbuf(s_tee, NULL, _IOLBF, 512);
    stdout = s_tee;
    stderr = s_tee;
    return s_file != NULL ? 0 : -1;
}

void ico_3ds_log_flush(void)
{
    if (s_tee != NULL) {
        fflush(s_tee);
    }
    if (s_file != NULL) {
        fflush(s_file);
    }
}

void ico_3ds_log_close(void)
{
    ico_3ds_log_flush();
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
}
