#include "es_node.h"
#include "es_params.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "stun.h"
#include "debug.h"

#define SOCKET_RCVBUF_SIZE (2 * 1024 * 1024)  /* 2MB receive buffer */

static int
optimize_socket(int sockfd)
{
    int rcvbuf_size = SOCKET_RCVBUF_SIZE;
    int flags;
    int enable = 1;

    /* Increase receive buffer size for burst traffic */
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size)) < 0)
    {
        warn("Failed to set SO_RCVBUF: %s", strerror(errno));
    }

    /* Enable address reuse */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        warn("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

#ifdef SO_REUSEPORT
    /* Enable port reuse for better performance */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0)
    {
        warn("Failed to set SO_REUSEPORT: %s", strerror(errno));
    }
#endif

    /* Set socket to non-blocking mode */
    flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1)
    {
        err("Failed to get socket flags: %s", strerror(errno));
        return -1;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        err("Failed to set non-blocking: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, const char *argv[])
{
    es_status rc;
    uint16_t local_port;
    es_node *node = calloc(1, sizeof(es_node));
    es_params *params = calloc(1, sizeof(es_params));

    rc = es_params_read_from_cmdline(params, argc, argv);
    if (rc != ES_EOK)
    {
        crit("Invalid command line arguments");
        return -1;
    }

    rc = es_params_read_config(params);
    if (rc != ES_EOK)
    {
        crit("Invalid config parameters");
        return -1;
    }

    if (params->fork)
    {
        pid_t pid = fork();

        if (pid < 0)
            exit(EXIT_FAILURE);
        if (pid > 0)
            exit(EXIT_SUCCESS);
        if (setsid() < 0)
            exit(EXIT_FAILURE);

        chdir("/");
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }

    es_init(node);
    es_init_params(node, params);


    while (true)
    {
        rc = es_twoway_bind(node);
        if (rc != ES_EOK)
        {
            err("Failed to bind");
            goto restart;
        }

        /* Optimize socket for high-performance packet reception */
        if (optimize_socket(node->sk) < 0)
        {
            warn("Socket optimization failed, continuing anyway");
        }

        rc = es_local_start_recv(node);
        if (rc != ES_EOK)
        {
            err("Failed to start receiving");
            goto restart;
        }

        while (ES_TRUE)
        {
            pause();
        }

restart:
        if (params->restart_interval == 0)
        {
            crit("exiting due to connection error");
            return -1;
        }

        dbg("Restarting in %d seconds", params->restart_interval);
        sleep(params->restart_interval);
    }
    es_fini(node);

    return 0;
}
