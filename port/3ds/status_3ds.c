/*
 * port/3ds/status_3ds.c
 *
 * The status panel on the top screen and the system's events
 * (status_3ds.h).
 *
 * A vsync hook (host_loop.h ico_host_on_vsync_register) runs once per
 * simulated vsync on the game's thread. Every vsync it lets the system
 * handle its events (aptMainLoop: the HOME button, the lid, the power
 * button) and reads the buttons; START + SELECT, or the system asking the
 * program to close, ends the run. Every ICO_3DS_STATUS_PERIOD_VSYNCS it
 * redraws the panel: how fast the simulation runs and where the memory is.
 * Those figures are what the headless milestone is for (docs/3DS.md).
 */
#include <3ds.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../platform/host_loop.h"
#include "../platform/trace_host.h"
#include "heap_3ds.h"
#include "ico_3ds_config.h"
#include "log_3ds.h"
#include "status_3ds.h"
#include "vm_3ds.h"

static PrintConsole *s_top;
static PrintConsole *s_bottom;
static u64 s_last_ms;             /* when the panel was last drawn */
static unsigned int s_last_vsync; /* the vsync count then */

/* Prints to the top screen only: straight to the console device, so the
   panel stays out of the log file (log_3ds.c). */
static void panel_line(int row, const char *fmt, ...)
{
    char text[128];
    char line[160];
    va_list ap;
    int n;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    /* move to the row, print, clear the rest of the line */
    n = snprintf(line, sizeof(line), "\x1b[%d;1H%s\x1b[K", row, text);
    if (n > 0) {
        (void)write(STDOUT_FILENO, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1);
    }
}

static unsigned int kib(u32 bytes)
{
    return (unsigned int)(bytes / 1024u);
}

static void draw_panel(void)
{
    const Ico3dsMemorySplit *split = ico_3ds_memory_split();
    const struct mallinfo heap = mallinfo();
    const unsigned int vsync = ico_host_vsync_count();
    const u64 now = osGetTime();
    const u64 elapsed = now - s_last_ms;
    const unsigned int rate =
        elapsed > 0 ? (unsigned int)((u64)(vsync - s_last_vsync) * 1000u / elapsed) : 0;

    s_last_ms = now;
    s_last_vsync = vsync;

    consoleSelect(s_top);
    panel_line(1, "ICO 3DS - headless milestone");
    panel_line(3, "vsync %u   main tick %u", vsync, ico_host_main_ticks());
    panel_line(4, "speed %u vsyncs/s (the game wants %d)", rate, ico_host_vsync_hz());
    panel_line(6, "Memory (KB)");
    panel_line(7, " budget at start  %6u", kib(split->commit_budget_at_start));
    panel_line(8, " left to commit   %6u", kib(ico_3ds_commit_left()));
    panel_line(9, " newlib heap      %6u used of %u", kib((u32)heap.uordblks),
               kib(split->newlib_heap_bytes));
    panel_line(10, " linear heap      %6u free of %u", kib(linearSpaceFree()),
               kib(split->linear_heap_bytes));
    panel_line(11, " arena + fibers   %6u now, %u peak", kib(ico_vm_3ds_committed_bytes()),
               kib(ico_vm_3ds_committed_high_water()));
    panel_line(12, " address space    %6u of %u", kib(ico_vm_3ds_reserved_bytes()),
               kib(split->vm_end - split->vm_begin));
    panel_line(14, "Log: %s/%s", ICO_3DS_DATA_DIR, ICO_3DS_LOG_NAME);
    panel_line(15, "START + SELECT: quit");
    consoleSelect(s_bottom);
}

/* The vsync hook. */
static void on_vsync(void *user)
{
    (void)user;
    if (!aptMainLoop()) {
        fprintf(stderr, "3ds: the system asked the program to close\n");
        exit(0);
    }
    hidScanInput();
    if ((hidKeysHeld() & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT)) {
        fprintf(stderr, "3ds: START + SELECT pressed, quitting\n");
        exit(0);
    }
    if (ico_host_vsync_count() % ICO_3DS_STATUS_PERIOD_VSYNCS == 0) {
        draw_panel();
        ico_3ds_log_flush();
    }
}

void ico_3ds_status_init(PrintConsole *top, PrintConsole *bottom)
{
    s_top = top;
    s_bottom = bottom;
    s_last_ms = osGetTime();
    if (ico_host_on_vsync_register(on_vsync, NULL) != 0) {
        fprintf(stderr, "3ds: the vsync hook table is full; no status panel\n");
    }
    draw_panel();
}

void ico_3ds_status_message(const char *text)
{
    consoleSelect(s_top);
    panel_line(17, "%s", text);
    consoleSelect(s_bottom);
}
