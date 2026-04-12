#include "testkit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_SYSCALLS 1024

typedef struct {
    char name[64];
    double time;
} syscall_stat;

typedef struct {
    syscall_stat stats[MAX_SYSCALLS];
    int count;
    double total_time;
} syscall_stats;

int parse_strace_line(char *line, char *syscall_name, double *time);
void add_syscall(syscall_stats *stats, const char *name, double time);
void print_top_syscalls(syscall_stats *stats, int n);
char *resolve_command(const char *command);

static const char *REL_HELPER = "./tk_sperf_rel_helper.sh";

static void assert_double_close(double actual, double expected) {
    double diff = actual - expected;
    if (diff < 0) {
        diff = -diff;
    }
    tk_assert(diff < 1e-9, "Expected %.12f, got %.12f", expected, actual);
}

static char *capture_top_output(syscall_stats *stats, int n, size_t *size_out) {
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);
    tk_assert(mem != NULL, "open_memstream() should succeed");

    FILE *saved_stdout = stdout;
    stdout = mem;
    print_top_syscalls(stats, n);
    fflush(mem);
    stdout = saved_stdout;
    fclose(mem);

    *size_out = size;
    return buf;
}

static void create_relative_helper(void) {
    FILE *fp = fopen(REL_HELPER, "w");
    tk_assert(fp != NULL, "Should create %s", REL_HELPER);
    fputs("#!/bin/sh\nexit 0\n", fp);
    fclose(fp);
    tk_assert(chmod(REL_HELPER, 0700) == 0, "chmod(%s) should succeed", REL_HELPER);
}

static void cleanup_relative_helper(void) {
    remove(REL_HELPER);
}

UnitTest(parse_strace_line_basic) {
    char line[] =
        "openat(AT_FDCWD, \"/etc/ld.so.cache\", O_RDONLY|O_CLOEXEC) = 3 "
        "<0.000012>\n";
    char syscall_name[64];
    double time = 0;

    tk_assert(parse_strace_line(line, syscall_name, &time) == 1,
              "A normal strace line should parse successfully");
    tk_assert(strcmp(syscall_name, "openat") == 0,
              "Expected syscall name openat, got %s", syscall_name);
    assert_double_close(time, 0.000012);
}

UnitTest(parse_strace_line_uses_last_angle_brackets) {
    char line[] =
        "write(1, \"debug <ignore me>\", 17) = 17 <0.002500>\n";
    char syscall_name[64];
    double time = 0;

    tk_assert(parse_strace_line(line, syscall_name, &time) == 1,
              "Parser should use the trailing <time> section");
    tk_assert(strcmp(syscall_name, "write") == 0,
              "Expected syscall name write, got %s", syscall_name);
    assert_double_close(time, 0.002500);
}

UnitTest(parse_strace_line_rejects_invalid_input) {
    char line[] = "this is not a strace line\n";
    char syscall_name[64] = {0};
    double time = 0;

    tk_assert(parse_strace_line(line, syscall_name, &time) == 0,
              "Invalid text must not be treated as a syscall line");
}

UnitTest(add_syscall_accumulates_time_and_count) {
    syscall_stats stats = {0};

    add_syscall(&stats, "openat", 0.10);
    add_syscall(&stats, "read", 0.20);
    add_syscall(&stats, "openat", 0.05);

    tk_assert(stats.count == 2,
              "Expected 2 distinct syscalls, got %d", stats.count);
    assert_double_close(stats.total_time, 0.35);
    tk_assert(strcmp(stats.stats[0].name, "openat") == 0,
              "The first syscall should stay openat");
    assert_double_close(stats.stats[0].time, 0.15);
    tk_assert(strcmp(stats.stats[1].name, "read") == 0,
              "The second syscall should be read");
    assert_double_close(stats.stats[1].time, 0.20);
}

UnitTest(print_top_syscalls_sorts_formats_and_flushes_frame) {
    syscall_stats stats = {0};
    size_t size = 0;

    add_syscall(&stats, "read", 0.4);
    add_syscall(&stats, "openat", 0.6);

    char *buf = capture_top_output(&stats, 5, &size);

    const char *prefix = "openat (60%)\nread (40%)\n";
    size_t prefix_len = strlen(prefix);
    tk_assert(size == prefix_len + 80,
              "Expected text plus 80 NUL separators, got %zu bytes", size);
    tk_assert(memcmp(buf, prefix, prefix_len) == 0,
              "Unexpected profiler text output");
    for (size_t i = prefix_len; i < size; i++) {
        tk_assert(buf[i] == '\0',
                  "Separator region should be all NUL bytes");
    }

    free(buf);
}

UnitTest(print_top_syscalls_empty_stats_still_emits_frame) {
    syscall_stats stats = {0};
    size_t size = 0;

    char *buf = capture_top_output(&stats, 5, &size);

    tk_assert(size == 80,
              "Empty stats should still emit exactly 80 NUL separators, got %zu",
              size);
    for (size_t i = 0; i < size; i++) {
        tk_assert(buf[i] == '\0',
                  "Empty frame should contain only NUL separators");
    }

    free(buf);
}

UnitTest(resolve_command_accepts_absolute_path) {
    char *path = resolve_command("/bin/sh");
    tk_assert(path != NULL, "Absolute paths should be returned directly");
    tk_assert(strcmp(path, "/bin/sh") == 0,
              "Expected /bin/sh, got %s", path);
    free(path);
}

UnitTest(resolve_command_searches_path) {
    char *old_path = getenv("PATH") ? strdup(getenv("PATH")) : NULL;
    tk_assert(setenv("PATH", "/bin:/usr/bin", 1) == 0,
              "setenv(PATH) should succeed");

    char *path = resolve_command("sh");
    tk_assert(path != NULL, "Should find sh from PATH");
    tk_assert(access(path, X_OK) == 0,
              "Resolved path must be executable, got %s", path);
    tk_assert(strstr(path, "/sh") != NULL,
              "Resolved path should point to sh, got %s", path);

    free(path);
    if (old_path) {
        tk_assert(setenv("PATH", old_path, 1) == 0,
                  "Restoring PATH should succeed");
        free(old_path);
    } else {
        tk_assert(unsetenv("PATH") == 0,
                  "unsetenv(PATH) should succeed");
    }
}

UnitTest(resolve_command_accepts_relative_path_with_slash,
         .init = create_relative_helper,
         .fini = cleanup_relative_helper) {
    char *old_path = getenv("PATH") ? strdup(getenv("PATH")) : NULL;
    tk_assert(setenv("PATH", "/definitely/not/here", 1) == 0,
              "setenv(PATH) should succeed");

    char *path = resolve_command(REL_HELPER);
    tk_assert(path != NULL,
              "A command containing '/' should be treated as an explicit path");
    tk_assert(strcmp(path, REL_HELPER) == 0,
              "Expected relative path %s, got %s", REL_HELPER, path);

    free(path);
    if (old_path) {
        tk_assert(setenv("PATH", old_path, 1) == 0,
                  "Restoring PATH should succeed");
        free(old_path);
    } else {
        tk_assert(unsetenv("PATH") == 0,
                  "unsetenv(PATH) should succeed");
    }
}
