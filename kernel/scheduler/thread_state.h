#pragma once

#define THREAD_STATE_ENUM_READY      0
#define THREAD_STATE_ENUM_RUNNING    1
#define THREAD_STATE_ENUM_SUSPENDED  2
#define THREAD_STATE_ENUM_BLOCKED    3
#define THREAD_STATE_ENUM_TERMINATED 4

#ifndef __ASSEMBLER__
typedef enum thread_state_t {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_SUSPENDED,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED,
} thread_state_t;

_Static_assert(THREAD_STATE_ENUM_READY == THREAD_STATE_READY, "Thread state enum ready mismatch");
_Static_assert(THREAD_STATE_ENUM_RUNNING == THREAD_STATE_RUNNING, "Thread state enum running mismatch");
_Static_assert(THREAD_STATE_ENUM_SUSPENDED == THREAD_STATE_SUSPENDED, "Thread state enum suspended mismatch");
_Static_assert(THREAD_STATE_ENUM_BLOCKED == THREAD_STATE_BLOCKED, "Thread state enum blocked mismatch");
_Static_assert(THREAD_STATE_ENUM_TERMINATED == THREAD_STATE_TERMINATED, "Thread state enum terminated mismatch");
#endif
