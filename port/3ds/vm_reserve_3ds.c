/*
 * port/3ds/vm_reserve_3ds.c
 *
 * The Nintendo 3DS implementation of port/platform/vm_reserve.h.
 *
 * The 3DS has no mmap, but an application can commit physical pages at any
 * free address of its heap area with svcControlMemory (MEMOP_ALLOC) and
 * give them back with MEMOP_FREE. heap_3ds.c leaves the top of the heap
 * area and part of the commit budget for this file, so a reservation is:
 *
 *   reserve   a range of virtual addresses in that area, nothing committed
 *             (or everything, when commit is 1)
 *   commit    svcControlMemory(MEMOP_ALLOC) over the range's pages that are
 *             not committed yet, then zero filled
 *   decommit  svcControlMemory(MEMOP_FREE) over the range's committed pages
 *
 * This keeps fiber.c's pool exactly as on the PC: 256 slots of address
 * space, of which only the live threads' stacks use memory.
 *
 * Fixed addresses are not supported (the 3DS build sets no pinned base),
 * so a reservation with `at` other than 0 fails, and the caller falls back
 * to "anywhere". Reservations of 1 MB or more start on a 1 MB boundary,
 * which the EE arena needs (arena.h ICO_ARENA_ALIGN).
 *
 * Everything here runs on the game's thread (the arena and the fiber pool
 * are created and changed by the scheduler only), so there is no lock.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../platform/vm_reserve.h"
#include "heap_3ds.h"
#include "vm_3ds.h"

#define PAGE_BYTES 4096u
#define BIG_ALIGN (1024u * 1024u)
#define MAX_RESERVATIONS 8

/* One reservation and the pages of it that are committed (one bit each). */
typedef struct Reservation {
    u32 base;
    u32 bytes;
    unsigned char *committed;
} Reservation;

static Reservation s_res[MAX_RESERVATIONS];
static u32 s_next;            /* the first free virtual address */
static u32 s_committed_bytes; /* over every reservation */
static u32 s_committed_high_water;

static u32 align_up(u32 value, u32 alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static int page_is_committed(const Reservation *r, u32 page)
{
    return (r->committed[page >> 3] >> (page & 7u)) & 1u;
}

static void page_mark(Reservation *r, u32 page, int committed)
{
    if (committed) {
        r->committed[page >> 3] |= (unsigned char)(1u << (page & 7u));
    } else {
        r->committed[page >> 3] &= (unsigned char)~(1u << (page & 7u));
    }
}

/* The reservation holding [addr, addr + bytes), or NULL. */
static Reservation *find_reservation(u32 addr, u32 bytes)
{
    int i;

    for (i = 0; i < MAX_RESERVATIONS; i++) {
        Reservation *r = &s_res[i];
        if (r->bytes != 0 && addr >= r->base && bytes <= r->bytes &&
            addr - r->base <= r->bytes - bytes) {
            return r;
        }
    }
    return NULL;
}

/* Commits the pages [first, first + count) of r that are not committed
   yet, in runs of consecutive pages (one system call a run). */
static int commit_pages(Reservation *r, u32 first, u32 count)
{
    u32 page = first;
    const u32 end = first + count;

    while (page < end) {
        u32 run_start;
        u32 run_bytes;
        u32 addr;
        u32 out;

        if (page_is_committed(r, page)) {
            page++;
            continue;
        }
        run_start = page;
        while (page < end && !page_is_committed(r, page)) {
            page++;
        }
        addr = r->base + run_start * PAGE_BYTES;
        run_bytes = (page - run_start) * PAGE_BYTES;
        if (R_FAILED(svcControlMemory(&out, addr, 0x0, run_bytes, MEMOP_ALLOC,
                                      MEMPERM_READ | MEMPERM_WRITE))) {
            return -1;
        }
        /* vm_reserve.h promises zero-filled pages */
        memset((void *)addr, 0, run_bytes);
        for (u32 p = run_start; p < page; p++) {
            page_mark(r, p, 1);
        }
        s_committed_bytes += run_bytes;
        if (s_committed_bytes > s_committed_high_water) {
            s_committed_high_water = s_committed_bytes;
        }
    }
    return 0;
}

/* Gives back the committed pages among [first, first + count) of r. */
static void decommit_pages(Reservation *r, u32 first, u32 count)
{
    u32 page = first;
    const u32 end = first + count;

    while (page < end) {
        u32 run_start;
        u32 out;

        if (!page_is_committed(r, page)) {
            page++;
            continue;
        }
        run_start = page;
        while (page < end && page_is_committed(r, page)) {
            page_mark(r, page, 0);
            page++;
        }
        svcControlMemory(&out, r->base + run_start * PAGE_BYTES, 0x0,
                         (page - run_start) * PAGE_BYTES, MEMOP_FREE, MEMPERM_DONTCARE);
        s_committed_bytes -= (page - run_start) * PAGE_BYTES;
    }
}

void *ico_vm_reserve(uintptr_t at, size_t bytes, int commit)
{
    const Ico3dsMemorySplit *split = ico_3ds_memory_split();
    Reservation *slot = NULL;
    u32 size;
    u32 base;
    int i;

    if (at != 0 || bytes == 0 || bytes > split->vm_end - split->vm_begin) {
        return NULL;
    }
    if (s_next == 0) {
        s_next = split->vm_begin;
    }
    for (i = 0; i < MAX_RESERVATIONS; i++) {
        if (s_res[i].bytes == 0) {
            slot = &s_res[i];
            break;
        }
    }
    if (slot == NULL) {
        fprintf(stderr, "vm_3ds: more than %d reservations\n", MAX_RESERVATIONS);
        return NULL;
    }
    size = align_up((u32)bytes, PAGE_BYTES);
    base = align_up(s_next, size >= BIG_ALIGN ? BIG_ALIGN : PAGE_BYTES);
    if (base < s_next || base > split->vm_end || size > split->vm_end - base) {
        fprintf(stderr, "vm_3ds: no address space for %lu bytes (%lu left)\n", (unsigned long)size,
                (unsigned long)(split->vm_end - s_next));
        return NULL;
    }
    slot->committed = calloc((size / PAGE_BYTES + 7u) / 8u, 1);
    if (slot->committed == NULL) {
        return NULL;
    }
    slot->base = base;
    slot->bytes = size;
    if (commit && commit_pages(slot, 0, size / PAGE_BYTES) != 0) {
        decommit_pages(slot, 0, size / PAGE_BYTES);
        free(slot->committed);
        memset(slot, 0, sizeof(*slot));
        return NULL;
    }
    s_next = base + size;
    return (void *)base;
}

void ico_vm_release(void *p, size_t bytes)
{
    Reservation *r;

    (void)bytes;
    if (p == NULL) {
        return;
    }
    r = find_reservation((u32)p, 1);
    if (r == NULL || r->base != (u32)p) {
        return;
    }
    decommit_pages(r, 0, r->bytes / PAGE_BYTES);
    /* the address space comes back only when it is the newest reservation;
       a release in the middle leaves a hole (fiber.c and arena.c release
       only on a failed start, so this does not add up) */
    if (r->base + r->bytes == s_next) {
        s_next = r->base;
    }
    free(r->committed);
    memset(r, 0, sizeof(*r));
}

int ico_vm_commit(void *p, size_t bytes)
{
    const u32 addr = (u32)p & ~(PAGE_BYTES - 1u);
    const u32 end = align_up((u32)p + (u32)bytes, PAGE_BYTES);
    Reservation *r = find_reservation(addr, end - addr);

    if (r == NULL) {
        return -1;
    }
    return commit_pages(r, (addr - r->base) / PAGE_BYTES, (end - addr) / PAGE_BYTES);
}

void ico_vm_decommit(void *p, size_t bytes)
{
    const u32 addr = (u32)p & ~(PAGE_BYTES - 1u);
    const u32 end = align_up((u32)p + (u32)bytes, PAGE_BYTES);
    Reservation *r = find_reservation(addr, end - addr);

    if (r != NULL) {
        decommit_pages(r, (addr - r->base) / PAGE_BYTES, (end - addr) / PAGE_BYTES);
    }
}

size_t ico_vm_page_size(void)
{
    return PAGE_BYTES;
}

u32 ico_vm_3ds_committed_bytes(void)
{
    return s_committed_bytes;
}

u32 ico_vm_3ds_committed_high_water(void)
{
    return s_committed_high_water;
}

u32 ico_vm_3ds_reserved_bytes(void)
{
    const Ico3dsMemorySplit *split = ico_3ds_memory_split();
    return s_next != 0 ? s_next - split->vm_begin : 0;
}
