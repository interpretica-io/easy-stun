#pragma once

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Execute a shell command asynchronously:
 *  - does NOT block the caller
 *  - returns immediately with child's PID (>0) on success
 *  - returns -1 on failure and sets errno
 *
 * If `dbg`/`warn` are available (debug.h), the implementation logs failures.
 *
 * The child:
 *  - detaches from parent's process group
 *  - execs `/bin/sh -lc <cmd>`
 *
 * Reaping:
 *  - child is waited for using WNOHANG once immediately
 *  - on many systems the child will be reaped when it exits if SIGCHLD is
 *    configured appropriately by the embedding program; otherwise it may become
 *    a zombie until reaped elsewhere.
 *
 * If you want strict zombie-free behavior without an event loop, install a
 * SIGCHLD handler that loops waitpid(-1, ..., WNOHANG) in your program init.
 */
static inline pid_t
es_spawn_sh_noblock(const char *cmd)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        /* fork failed */
#ifdef warn
        warn("fork() failed: %s", strerror(errno));
#endif
        return (pid_t)-1;
    }

    if (pid == 0)
    {
        /* child */
        (void)setsid();
        execl("/bin/sh", "sh", "-lc", cmd, (char *)NULL);

        /* exec failed */
#ifdef warn
        warn("exec(/bin/sh -lc ...) failed: %s", strerror(errno));
#endif
        _exit(127);
    }

    (void)waitpid(pid, NULL, WNOHANG);
    return pid;
}

#define EXIT_ON_ERROR(__msg, __rc) do { \
    if ((__rc) != ES_EOK) { \
        err("%s", __msg); \
        rc = (__rc); \
        goto err; \
    } \
} while (0)

#define UNUSED(__x) do { (void)(__x); } while (0)

#define PAD(__val, __pad) ({ \
    int __v = (__pad) - ((__val) % (__pad)); \
    if (__v == (__pad)) \
        __v = 0; \
    __v; \
})

#define ES_BREAKABLE_START() do {
#define ES_BREAKABLE_END() } while (0)
