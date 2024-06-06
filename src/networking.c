#include "redis.h"
#include "errno.h"

/*
 * 创建一个新客户端
 */
redisClient *createClient(int fd) {
    // 分配空间
    redisClient *c = zmalloc(sizeof(redisClient));

    if (fd != -1) {
        // 非阻塞
        anetNonBlock(NULL,fd);
        
        // 绑定读事件到事件 loop （开始接收命令请求）
        if (aeCreateFileEvent(server.el,fd,AE_READABLE,
            readQueryFromClient, c) == AE_ERR)
        {
            close(fd);
            zfree(c);
            return NULL;
        }
    }

    // 默认数据库
    selectDb(c,0);

    return c;
}

void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    printf("readQueryFromClient:\n");
}

/*
 * TCP 连接 accept 处理器
 */
#define MAX_ACCEPTS_PER_CALL 1000

static void acceptCommonHandler(int fd, int flags) {
    // 创建客户端
    redisClient *c;
    if ((c = createClient(fd)) == NULL) {
        redisLog(REDIS_WARNING,
            "Error registering fd event for the new client: %s (fd=%d)",
            strerror(errno),fd);
        close(fd); /* May be already closed, just ignore errors */
        return;
    }
}


/* 
 * 创建一个 TCP 连接处理器
 */
void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[REDIS_IP_STR_LEN];
    REDIS_NOTUSED(el);
    REDIS_NOTUSED(mask);
    REDIS_NOTUSED(privdata);

    while(max--) {
        // accept 客户端连接
        cfd = anetTcpAccept(server.neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                redisLog(REDIS_WARNING,
                    "Accepting client connection: %s", server.neterr);
            return;
        }
        redisLog(REDIS_NOTICE,"Accepted %s:%d", cip, cport);
        // 为客户端创建客户端状态（redisClient）
        acceptCommonHandler(cfd,0);
    }
}

//bind ipv4 0.0.0.0
int listenToPort(int port, int *fds, int *count) {
    fds[*count] = anetTcpServer(server.neterr,port,NULL,
                server.tcp_backlog);

    if (fds[*count] != ANET_ERR) {
        anetNonBlock(NULL,fds[*count]);
        (*count)++;

        return REDIS_OK;
    }

    if (fds[*count] == ANET_ERR) {
        redisLog(REDIS_WARNING,
            "Creating Server TCP listening socket %s:%d: %s",
            "*",port, server.neterr);
            
        return REDIS_ERR;
    }
}