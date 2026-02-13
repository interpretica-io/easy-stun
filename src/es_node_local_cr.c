#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "es_node.h"
#include "es_status.h"
#include "stun.h"
#include "es_msg.h"
#include "es_bool.h"
#include "debug.h"
#include "helper.h"

es_status
es_local_conn_request(es_node *node, const char *buf, uint32_t buf_len)
{
    char full_cmd[1024];
    pid_t pid;

    UNUSED(buf);
    UNUSED(buf_len);

    /* FIXME: we need to parse connection request properly */

    sprintf(full_cmd, "%s cr %s %u", node->params.script,
        node->status.mapped_addr,
        (unsigned)node->status.mapped_port);

    /* Fire-and-forget: do not block the request processing path */
    pid = es_spawn_sh_noblock(full_cmd);
    if (pid < 0)
    {
        warn("Failed to spawn script '%s': %s", node->params.script, strerror(errno));
        return ES_ESCRIPTFAIL;
    }

    ring("Script '%s' spawned (pid %ld)", node->params.script, (long)pid);
    return ES_EOK;
}
