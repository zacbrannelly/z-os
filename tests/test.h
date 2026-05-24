#pragma once

#include <stdint.h>
#include <libz/syscall.h>

typedef struct test_case_t {
    const char *name;
    void (*test_case)(void);
} test_case_t;

#define TEST_CASE(ident) \
    static void ident(void); \
    static test_case_t __test_##ident \
    __attribute__((used, section(".tests"))) = { #ident, ident }; \
    static void ident(void)

extern const test_case_t __tests_start[];
extern const test_case_t __tests_end[];

static inline void run_tests(void) {
    for (const test_case_t *test_case = __tests_start; test_case < __tests_end; test_case++) {
        syscall_console_write("Running test: ");
        syscall_console_write(test_case->name);
        syscall_console_write("\r\n");
        test_case->test_case();
        syscall_console_write("Successfully ran test: ");
        syscall_console_write(test_case->name);
        syscall_console_write("\r\n");
    }
    syscall_console_write("All tests passed\r\n");
    syscall_shutdown();
}
