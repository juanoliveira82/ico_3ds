/*
 * port/3ds/ico_3ds_config.h
 *
 * Compile-time settings of the Nintendo 3DS build (ICO_3DS=1), shared by the
 * 3DS platform files and the few port files that have a 3DS branch.
 *
 * The values that the shared port code reads as macros (the fiber stack
 * sizes, the pinned bases) are set on the command line by cmake/Ico3ds.cmake,
 * because those headers are included before this one; this header holds the
 * values only the 3DS files use.
 */
#ifndef ICO_3DS_CONFIG_H
#define ICO_3DS_CONFIG_H

/* The folder on the SD card that holds everything: the disc image the
   player copies there, the game data the first start extracts from it, the
   settings, the saves and the logs (host_config.c treats it as the
   program's folder, in portable mode). */
#define ICO_3DS_DATA_DIR "sdmc:/3ds/ico"

/* The log file inside ICO_3DS_DATA_DIR (log_3ds.c). Everything the game and
   the port print goes there and to the bottom screen. */
#define ICO_3DS_LOG_NAME "ico-3ds.log"

/* The linear heap: physically contiguous memory the GPU and the DSP read.
   The headless milestone draws nothing, so it only needs room for the
   console and the system's own buffers; the renderer will raise it. */
#define ICO_3DS_LINEAR_HEAP_BYTES (4u * 1024u * 1024u)

/* The newlib heap (malloc): the port's host-side buffers (the disc image
   reader, the archive index, audio and video decoders). The game's own
   heap lives in the EE arena, which is not part of this. Whatever the
   process may still commit after this heap and the linear heap is left to
   the reservations of vm_reserve_3ds.c (the arena and the fiber stacks). */
#define ICO_3DS_NEWLIB_HEAP_BYTES (12u * 1024u * 1024u)

/* How often the status panel on the top screen is redrawn, in vsyncs. */
#define ICO_3DS_STATUS_PERIOD_VSYNCS 30

#endif /* ICO_3DS_CONFIG_H */
