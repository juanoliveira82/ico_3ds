/*
 * port/3ds/vm_3ds.h
 *
 * The figures of vm_reserve_3ds.c, for the status panel (status_3ds.c).
 */
#ifndef ICO_3DS_VM_H
#define ICO_3DS_VM_H

#include <3ds/types.h>

/* Bytes committed now, over every reservation (the arena, the fiber stacks). */
u32 ico_vm_3ds_committed_bytes(void);
/* The most that was ever committed at once. */
u32 ico_vm_3ds_committed_high_water(void);
/* Address space handed out so far (committed or not). */
u32 ico_vm_3ds_reserved_bytes(void);

#endif /* ICO_3DS_VM_H */
