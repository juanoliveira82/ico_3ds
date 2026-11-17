/*
 * port/3ds/main_3ds.c
 *
 * The program's entry point on the Nintendo 3DS.
 *
 * It sets up what the shared host code (port/platform/main_host.c) expects
 * from an operating system and then runs it, like the Android and iOS entry
 * points do:
 *
 *   1. the screens: a console on each (top: the status panel, bottom: the
 *      log), before anything prints;
 *   2. the data folder on the SD card (ico_3ds_config.h), which is the
 *      program's folder for host_config.c;
 *   3. the log file in it (log_3ds.c);
 *   4. the status panel and the system events (status_3ds.c);
 *   5. ico_host_main with --console, so the host keeps printing to the
 *      streams log_3ds.c set up instead of opening a log of its own.
 *
 * At exit (the game ended, a fatal error, START + SELECT) the last lines stay
 * on screen until START is pressed, so an error can be read.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ico_3ds_config.h"
#include "log_3ds.h"
#include "status_3ds.h"

/* port/platform/main_host.c (its main is ico_host_main on this build). */
int ico_host_main(int argc, char **argv);

static PrintConsole s_top;
static PrintConsole s_bottom;

/* Keeps the screens up until START is pressed (or the system closes the
   program), then shuts the services down. */
static void wait_and_shut_down(void)
{
    ico_3ds_log_close();
    ico_3ds_status_message("The run ended. Press START to close.");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            break;
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    gfxExit();
}

/* Creates the data folder if it is not there yet. */
static void make_data_dir(void)
{
    /* sdmc:/3ds always exists where homebrew runs; the folder under it may
       not on the first start */
    mkdir(ICO_3DS_DATA_DIR, 0777);
}

int main(int argc, char **argv)
{
    static char console_flag[] = "--console";
    char *host_argv[3];

    gfxInitDefault();
    consoleInit(GFX_TOP, &s_top);
    consoleInit(GFX_BOTTOM, &s_bottom); /* the log console is the selected one */
    atexit(wait_and_shut_down);

    /* the CPU at its best speed (New 3DS only; no effect on an Old 3DS) */
    osSetSpeedupEnable(true);

    make_data_dir();
    if (chdir(ICO_3DS_DATA_DIR) != 0) {
        fprintf(stderr, "3ds: cannot open the folder %s\n", ICO_3DS_DATA_DIR);
    }
    if (ico_3ds_log_open(ICO_3DS_DATA_DIR "/" ICO_3DS_LOG_NAME) != 0) {
        fprintf(stderr, "3ds: cannot write %s/%s; the log is on screen only\n", ICO_3DS_DATA_DIR,
                ICO_3DS_LOG_NAME);
    }
    fprintf(stderr, "3ds: ICO 3DS starting; data folder %s\n", ICO_3DS_DATA_DIR);

    ico_3ds_status_init(&s_top, &s_bottom);

    /* argv[0] from the Homebrew Launcher is the .3dsx's path; it may be
       missing when the program is started another way */
    host_argv[0] = argc > 0 && argv[0] != NULL ? argv[0] : (char *)"ico_3ds";
    host_argv[1] = console_flag;
    host_argv[2] = NULL;
    return ico_host_main(2, host_argv);
}
