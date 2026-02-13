#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

#include "es_node.h"
#include "es_status.h"
#include "es_bool.h"
#include "debug.h"

/*
 * Portable, event-driven receive loop.
 *
 * Replaces the previous timer-driven polling approach with a blocking poll(2)
 * loop that:
 *   - wakes up immediately when the socket becomes readable
 *   - drains the UDP receive queue (recvfrom in es_local_recv() is non-blocking)
 *   - performs keepalive pings on schedule without busy-waiting
 *
 * Notes:
 *   - es_local_recv() is expected to be non-blocking and return ES_ENODATA when
 *     no datagrams are available (EAGAIN/EWOULDBLOCK).
 *   - Keepalive interval is in seconds (per existing params).
 */

static uint64_t
now_ms(void)
{
    struct timespec ts;

#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
#endif

    /* Fallback: wall clock (still fine for basic timeouts) */
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

static int
clamp_poll_timeout_ms(int64_t v)
{
    if (v <= 0)
        return 0;
    if (v > 2147483647LL)
        return 2147483647;
    return (int)v;
}

static void
do_keepalive(es_node *node)
{
    dbg("[%s:%u] Connection needs keepalive - ping",
        node->params.remote_addr,
        (unsigned)node->params.remote_port);

    (void)es_remote_ping(node, ES_FALSE);
    (void)es_remote_ping(node, ES_TRUE);
}

es_status
es_local_start_recv(es_node *node)
{
    struct pollfd pfd;
    uint64_t next_keepalive_ms = 0;
    const uint32_t ka_interval_s = node->params.keepalive_interval;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = node->sk;
    pfd.events = POLLIN;

    if (ka_interval_s > 0)
        next_keepalive_ms = now_ms() + (uint64_t)ka_interval_s * 1000ull;

    for (;;)
    {
        uint64_t now = now_ms();
        int timeout_ms = -1;

        if (ka_interval_s > 0)
        {
            int64_t delta = (int64_t)(next_keepalive_ms - now);
            timeout_ms = clamp_poll_timeout_ms(delta);
        }

        /* Block until readable or next keepalive is due. */
        int pret;
        do
        {
            pret = poll(&pfd, 1, timeout_ms);
        } while (pret < 0 && errno == EINTR);

        if (pret < 0)
        {
            err("poll() failed: %s", strerror(errno));
            return ES_EFAIL;
        }

        now = now_ms();

        /* Keepalive timer. */
        if (ka_interval_s > 0 && now >= next_keepalive_ms)
        {
            do_keepalive(node);
            next_keepalive_ms = now + (uint64_t)ka_interval_s * 1000ull;
        }

        /* Socket readable: drain receive queue. */
        if (pret > 0 && (pfd.revents & (POLLIN | POLLERR | POLLHUP)))
        {
            for (;;)
            {
                es_status rc = es_local_recv(node);

                if (rc == ES_ENODATA)
                {
                    /* Drained. */
                    break;
                }

                if (es_status_is_conn_broken(rc))
                {
                    dbg("[%s:%u] Connection is broken - rebind",
                        node->params.remote_addr,
                        (unsigned)node->params.remote_port);

                    (void)es_twoway_bind(node);

                    /*
                     * After rebind, expected_tid and mapping state change;
                     * also reset keepalive schedule from "now" so we don't
                     * immediately ping again after transient errors.
                     */
                    if (ka_interval_s > 0)
                        next_keepalive_ms = now_ms() + (uint64_t)ka_interval_s * 1000ull;

                    break;
                }

                /*
                 * For other statuses: continue draining. es_local_recv() handles
                 * protocol-level errors (e.g., wrong TID) without requiring loop changes.
                 */
            }
        }

        /* Clear revents for next poll iteration. */
        pfd.revents = 0;
    }

    /* Unreachable */
    /* return ES_EOK; */
}