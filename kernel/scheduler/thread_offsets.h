#pragma once

#define THREAD_STATE_OFF    24
#define THREAD_CTX_SP_OFF   40
#define THREAD_CTX_REGS_OFF 48
#define THREAD_CTX_SPSR_OFF 304
#define THREAD_CTX_ELR_OFF  312

#ifndef __ASSEMBLER__
#include <stddef.h>
#include "thread.h"

_Static_assert(offsetof(thread_t, state) == THREAD_STATE_OFF, "Thread state offset mismatch");
_Static_assert(offsetof(thread_t, ctx.sp) == THREAD_CTX_SP_OFF, "Thread context sp offset mismatch");
_Static_assert(offsetof(thread_t, ctx.registers[0]) == THREAD_CTX_REGS_OFF, "Thread context registers offset mismatch");
_Static_assert(offsetof(thread_t, ctx.spsr) == THREAD_CTX_SPSR_OFF, "Thread context spsr offset mismatch");
_Static_assert(offsetof(thread_t, ctx.elr) == THREAD_CTX_ELR_OFF, "Thread context elr offset mismatch");
#endif
