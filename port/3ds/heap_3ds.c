/*
 * port/3ds/heap_3ds.c
 *
 * The process's memory split on the Nintendo 3DS (heap_3ds.h).
 *
 * libctru's start-up code calls __system_allocateHeaps() before main to
 * commit the newlib heap and the linear heap. Its default gives every byte
 * the process may still commit to those two heaps. This port needs a third
 * share: the reservations of vm_reserve_3ds.c (the EE arena and the fiber
 * stacks), which commit their pages on demand. So this file replaces the
 * weak default with one that commits fixed-size heaps
 * (ico_3ds_config.h) and leaves the rest of the commit budget uncommitted.
 */
#include <3ds.h>
#include <string.h>

#include "heap_3ds.h"
#include "ico_3ds_config.h"

/* newlib's heap bounds (libctru's sbrk reads them). */
extern char *fake_heap_start;
extern char *fake_heap_end;

/* libctru's own records of the two heaps (other libctru code reads them). */
extern u32 __ctru_heap;
extern u32 __ctru_heap_size;
extern u32 __ctru_linear_heap;
extern u32 __ctru_linear_heap_size;

static Ico3dsMemorySplit s_split;

/* What the process may still commit, from its resource limit. */
static u32 commit_budget_left(void)
{
    Handle reslimit = 0;
    ResourceLimitType type = RESLIMIT_COMMIT;
    s64 limit = 0;
    s64 used = 0;

    if (R_FAILED(svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE))) {
        svcBreak(USERBREAK_PANIC);
    }
    svcGetResourceLimitLimitValues(&limit, reslimit, &type, 1);
    svcGetResourceLimitCurrentValues(&used, reslimit, &type, 1);
    svcCloseHandle(reslimit);
    return (u32)(limit - used) & ~0xFFFu;
}

/* Replaces libctru's weak default (libctru/source/system/allocateHeaps.c).
   Runs before main, before newlib works: no printing here. */
void __system_allocateHeaps(void)
{
    const u32 left = commit_budget_left();
    u32 heap = ICO_3DS_NEWLIB_HEAP_BYTES;
    u32 linear = ICO_3DS_LINEAR_HEAP_BYTES;

    /* Too little memory for the configured heaps: keep the proportions so
       start-up still reaches main, where the status panel reports it. */
    if (heap + linear > left) {
        linear = (left / 4u) & ~0xFFFu;
        heap = left - linear;
    }

    __ctru_heap_size = heap;
    __ctru_linear_heap_size = linear;
    if (R_FAILED(svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, heap, MEMOP_ALLOC,
                                  MEMPERM_READ | MEMPERM_WRITE))) {
        svcBreak(USERBREAK_PANIC);
    }
    if (R_FAILED(svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, linear, MEMOP_ALLOC_LINEAR,
                                  MEMPERM_READ | MEMPERM_WRITE))) {
        svcBreak(USERBREAK_PANIC);
    }

    /* The mappable allocator, as libctru's default sets it up. */
    mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

    fake_heap_start = (char *)__ctru_heap;
    fake_heap_end = fake_heap_start + heap;

    /* The reservations' address range: from the end of the newlib heap,
       rounded up to 1 MB, to the end of the heap area. */
    s_split.commit_budget_at_start = left;
    s_split.newlib_heap_bytes = heap;
    s_split.linear_heap_bytes = linear;
    s_split.vm_commit_budget = left - heap - linear;
    s_split.vm_begin = (__ctru_heap + heap + 0xFFFFFu) & ~0xFFFFFu;
    s_split.vm_end = OS_HEAP_AREA_END;
}

const Ico3dsMemorySplit *ico_3ds_memory_split(void)
{
    return &s_split;
}

u32 ico_3ds_commit_left(void)
{
    return commit_budget_left();
}
