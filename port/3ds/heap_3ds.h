/*
 * port/3ds/heap_3ds.h
 *
 * The process's memory split on the Nintendo 3DS.
 *
 * An application commits physical memory against one budget (its resource
 * limit, set by the system's memory mode: 64 MB for a normal Old 3DS
 * application). heap_3ds.c divides it in three before main runs:
 *
 *   newlib heap   malloc, fixed size (ICO_3DS_NEWLIB_HEAP_BYTES)
 *   linear heap   GPU and DSP buffers, fixed size (ICO_3DS_LINEAR_HEAP_BYTES)
 *   reservations  everything else: vm_reserve_3ds.c commits it page by page
 *                 for the EE arena and the fiber stacks
 *
 * The program's own code and static data are already committed when this
 * runs, so they are not part of the budget it divides.
 */
#ifndef ICO_3DS_HEAP_H
#define ICO_3DS_HEAP_H

#include <3ds/types.h>

typedef struct Ico3dsMemorySplit {
    u32 commit_budget_at_start; /* what was left to commit before the heaps */
    u32 newlib_heap_bytes;
    u32 linear_heap_bytes;
    u32 vm_commit_budget; /* what the reservations may commit */
    u32 vm_begin;         /* the reservations' virtual address range */
    u32 vm_end;
} Ico3dsMemorySplit;

/* The split made at start-up. */
const Ico3dsMemorySplit *ico_3ds_memory_split(void);

/* What the process may still commit right now. */
u32 ico_3ds_commit_left(void);

#endif /* ICO_3DS_HEAP_H */
