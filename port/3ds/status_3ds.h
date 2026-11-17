/*
 * port/3ds/status_3ds.h
 *
 * The status panel on the top screen and the system's events (status_3ds.c).
 */
#ifndef ICO_3DS_STATUS_H
#define ICO_3DS_STATUS_H

#include <3ds/console.h>

/* Draws the panel on `top` once and hooks the per-vsync work. The log
   console is `bottom`; the panel selects it back after each redraw. */
void ico_3ds_status_init(PrintConsole *top, PrintConsole *bottom);

/* One line of text under the panel (the exit prompt, for one). */
void ico_3ds_status_message(const char *text);

#endif /* ICO_3DS_STATUS_H */
