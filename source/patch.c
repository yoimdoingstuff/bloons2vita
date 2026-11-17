/*
 * patch.c
 *
 * Patching some of the .so internal functions or bridging them to native
 * for better compatibility.
 *
 * libbloons2.so's statically linked libgcc implements the __sync_* atomics
 * by calling Linux's kernel-provided user helpers at fixed addresses:
 *
 *   0xFFFF0FC0  __kuser_cmpxchg         (atomic compare-and-swap)
 *   0xFFFF0FA0  __kuser_memory_barrier
 *
 * Those don't exist on the Vita (calling them was the crash in the game's
 * static constructors). Like mc3-vita, we map a small executable block at
 * 0x9A000000, put branches to our own implementations at 0x9A000FC0 and
 * 0x9A000FA0, and rewrite the literal pool entries holding the Linux
 * addresses (47 of them, all inside libgcc's linux-atomic code) to point
 * there instead.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "patch.h"

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

// Same headers as so_util.c, which uses the same kubridge API: the kernel
// header psp2kern/kernel/sysmem/memtype.h clashes with kubridge.h's types.
#include <vitasdk.h>
#include <kubridge.h>
#include <so_util/so_util.h>

#include "utils/dialog.h"
#include "utils/logger.h"
#include "utils/utils.h"

// Not in the user-side SDK headers; same fallback as so_util.c
#ifndef SCE_KERNEL_MEMBLOCK_TYPE_USER_RX
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX (0x0C20D050)
#endif

extern so_module so_mod;

#define KUSER_BLOCK          0x9A000000
#define KUSER_MEMORY_BARRIER 0x9A000FA0
#define KUSER_CMPXCHG        0x9A000FC0

static void kuser_memory_barrier(void) {
    asm volatile("dmb" ::: "memory");
}

// Returns 0 if *ptr was oldval and has been set to newval, non-zero
// otherwise (callers in libgcc only look at the return value).
static int kuser_cmpxchg(int32_t oldval, int32_t newval, volatile int32_t *ptr) {
    return !atomic_compare_exchange_strong((volatile _Atomic int32_t *)ptr, &oldval, newval);
}

void kuser_helpers_install(void) {
    SceKernelAllocMemBlockKernelOpt opt;
    memset(&opt, 0, sizeof(opt));
    opt.size = sizeof(opt);
    opt.attr = 0x1;
    opt.field_C = (SceUInt32)KUSER_BLOCK;
    if (kuKernelAllocMemBlock("kuser_helpers", SCE_KERNEL_MEMBLOCK_TYPE_USER_RX, 0x1000, &opt) < 0)
        fatal_error("Error: could not allocate the atomics helper block.");

    hook_addr(KUSER_MEMORY_BARRIER, (uintptr_t)&kuser_memory_barrier);
    hook_addr(KUSER_CMPXCHG, (uintptr_t)&kuser_cmpxchg);
    // This block isn't part of the module, so so_flush_caches() won't cover it
    kuKernelFlushCaches((void *)KUSER_BLOCK, 0x1000);
    l_success("Kernel user helpers installed.");
}

// Analytics entry points of the game. They only forward events to Java
// (Flurry, Google Analytics), which is pointless offline, and Mrun's very
// first flurry_logEvent call crashed inside its JNI plumbing. Make them
// return immediately.
static const char *analytics_functions[] = {
    "flurry_startSession",
    "flurry_logEvent",
    "flurry_endTimedEvent",
    "sendGoogleEvent",
};

static void stub_analytics(void) {
    for (size_t i = 0; i < sizeof(analytics_functions) / sizeof(analytics_functions[0]); i++) {
        uintptr_t fn = so_symbol(&so_mod, analytics_functions[i]);
        if (!fn) {
            l_warn("patch: %s not found", analytics_functions[i]);
            continue;
        }
        hook_addr(fn, (uintptr_t)&ret0);
    }
    l_success("Analytics functions stubbed out.");
}

void so_patch(void) {
    stub_analytics();

    int patched = 0;
    for (size_t i = 0; i + 4 <= so_mod.text_size; i += 4) {
        uintptr_t addr = so_mod.text_base + i;
        uint32_t value = *(uint32_t *)addr;
        uint32_t repl;

        if (value == 0xFFFF0FC0)
            repl = KUSER_CMPXCHG;
        else if (value == 0xFFFF0FA0)
            repl = KUSER_MEMORY_BARRIER;
        else
            continue;

        kuKernelCpuUnrestrictedMemcpy((void *)addr, &repl, sizeof(repl));
        patched++;
    }

    l_success("Redirected %i kernel user helper reference(s).", patched);
}
