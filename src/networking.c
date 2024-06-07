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

    // 查询缓冲区
    c->querybuf = sdsempty();

    // 命令请求的类型
    c->reqtype = 0;

    // 命令参数数量
    c->argc = 0;

    // 命令参数
    c->argv = NULL;

    // 当前执行的命令
    c->cmd = NULL;

    // 查询缓冲区中未读入的命令内容数量
    c->multibulklen = 0;

    // 读入的参数的长度
    c->bulklen = -1;

    // 套接字
    c->fd = fd;

    // 回复缓冲区的偏移量
    c->bufpos = 0;

    // 已发送字节数
    c->sentlen = 0;

    return c;
}

/*
 * 读取客户端的查询缓冲区内容
 */
void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    redisClient *c = (redisClient*) privdata;
    int nread, readlen;
    size_t qblen;
    REDIS_NOTUSED(el);
    REDIS_NOTUSED(mask);

    // 设置服务器的当前客户端
    // server.current_client = c;
    
    // 读入长度（默认为 16 KB）
    readlen = REDIS_IOBUF_LEN;

    // 获取查询缓冲区当前内容的长度
    // 如果读取出现 short read ，那么可能会有内容滞留在读取缓冲区里面
    // 这些滞留内容也许不能完整构成一个符合协议的命令，
    qblen = sdslen(c->querybuf);
    
    // 为查询缓冲区分配空间
    c->querybuf = sdsMakeRoomFor(c->querybuf, readlen);
    // 读入内容到查询缓存
    nread = read(fd, c->querybuf+qblen, readlen);

    // 读入出错
    if (nread == -1) {
        if (errno == EAGAIN) {
            nread = 0;
        } else {
            redisLog(REDIS_VERBOSE, "Reading from client: %s",strerror(errno));
            freeClient(c);
            return;
        }
    // 遇到 EOF
    } else if (nread == 0) {
        redisLog(REDIS_VERBOSE, "Client closed connection");
        freeClient(c);
        return;
    }

    if (nread) {
        // 根据内容，更新查询缓冲区（SDS） free 和 len 属性
        // 并将 '\0' 正确地放到内容的最后
        sdsIncrLen(c->querybuf,nread);
    } else {
        // 在 nread == -1 且 errno == EAGAIN 时运行
        // server.current_client = NULL;
        return;
    }

    // 从查询缓存重读取内容，创建参数，并执行命令
    // 函数会执行到缓存中的所有内容都被处理完为止
    processInputBuffer(c);
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

/*
 * 清空所有命令参数
 */
static void freeClientArgv(redisClient *c) {
    int j;

    for (j = 0; j < c->argc; j++)
        decrRefCount(c->argv[j]);
    c->argc = 0;
    c->cmd = NULL;
}

/*
 * 释放客户端
 */
void freeClient(redisClient *c) {
    // redisPanic("freeClient todo");
    /* Free the query buffer */
    sdsfree(c->querybuf);
    c->querybuf = NULL;

    // 关闭套接字，并从事件处理器中删除该套接字的事件
    if (c->fd != -1) {
        aeDeleteFileEvent(server.el,c->fd,AE_READABLE);
        aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
        close(c->fd);
    }

    // 清空命令参数
    freeClientArgv(c);

    // 清除参数空间
    zfree(c->argv);

    // 释放客户端 redisClient 结构本身
    zfree(c);
}

int processInlineBuffer(redisClient *c) {
    redisPanic("processInlineBuffer todo");
}


/*
 * 将 c->querybuf 中的协议内容转换成 c->argv 中的参数对象
 * 
 * 比如 *3\r\n$3\r\nSET\r\n$3\r\nMSG\r\n$5\r\nHELLO\r\n
 * 将被转换为：
 * argv[0] = SET
 * argv[1] = MSG
 * argv[2] = HELLO
 */
int processMultibulkBuffer(redisClient *c) {
    char *newline = NULL;
    int pos = 0, ok;
    long long ll;

    // 读入命令的参数个数
    // 比如 *3\r\n$3\r\nSET\r\n... 将令 c->multibulklen = 3
    if (c->multibulklen == 0) {
        /* The client should have been reset */
        redisAssertWithInfo(c,NULL,c->argc == 0);

        /* Multi bulk length cannot be read without a \r\n */
        // 检查缓冲区的内容第一个 "\r\n"
        newline = strchr(c->querybuf,'\r');
        if (newline == NULL) {
            return REDIS_ERR;
        }
        /* Buffer should also contain \n */
        if (newline-(c->querybuf) > ((signed)sdslen(c->querybuf)-2))
            return REDIS_ERR;

        /* We know for sure there is a whole line since newline != NULL,
         * so go ahead and find out the multi bulk length. */
        // 协议的第一个字符必须是 '*'
        redisAssertWithInfo(c,NULL,c->querybuf[0] == '*');
        // 将参数个数，也即是 * 之后， \r\n 之前的数字取出并保存到 ll 中
        // 比如对于 *3\r\n ，那么 ll 将等于 3
        ok = string2ll(c->querybuf+1,newline-(c->querybuf+1),&ll);
        
        // 参数的数量超出限制
        if (!ok || ll > 1024*1024) {
            redisPanic("Protocol error: invalid multibulk length");
            return REDIS_ERR;
        }

        // 参数数量之后的位置
        // 比如对于 *3\r\n$3\r\n$SET\r\n... 来说，
        // pos 指向 *3\r\n$3\r\n$SET\r\n...
        //                ^
        //                |
        //               pos
        pos = (newline-c->querybuf)+2;
        // 如果 ll <= 0 ，那么这个命令是一个空白命令
        // 那么将这段内容从查询缓冲区中删除，只保留未阅读的那部分内容
        // 为什么参数可以是空的呢？
        // processInputBuffer 中有注释到 "Multibulk processing could see a <= 0 length"
        // 但并没有详细说明原因
        if (ll <= 0) {
            // sdsrange(c->querybuf,pos,-1);
            redisPanic("Protocol error: ll <= 0");
            return REDIS_OK;
        }

        // 设置参数数量
        c->multibulklen = ll;

        /* Setup argv array on client structure */
        // 根据参数数量，为各个参数对象分配空间
        if (c->argv) zfree(c->argv);
        c->argv = zmalloc(sizeof(robj*)*c->multibulklen);
    }

    redisAssertWithInfo(c,NULL,c->multibulklen > 0);

    // 从 c->querybuf 中读入参数，并创建各个参数对象到 c->argv
    while(c->multibulklen) {

        /* Read bulk length if unknown */
        // 读入参数长度
        if (c->bulklen == -1) {

            // 确保 "\r\n" 存在
            newline = strchr(c->querybuf+pos,'\r');
            if (newline == NULL) {
                if (sdslen(c->querybuf) > REDIS_INLINE_MAX_SIZE) {
                    redisPanic("Protocol error: too big bulk count string");
                    return REDIS_ERR;
                }
                break;
            }
            /* Buffer should also contain \n */
            if (newline-(c->querybuf) > ((signed)sdslen(c->querybuf)-2))
                break;

            // 确保协议符合参数格式，检查其中的 $...
            // 比如 $3\r\nSET\r\n
            if (c->querybuf[pos] != '$') {
                redisPanic("Protocol error: expected '$'");
                return REDIS_ERR;
            }

            // 读取长度
            // 比如 $3\r\nSET\r\n 将会让 ll 的值设置 3
            ok = string2ll(c->querybuf+pos+1,newline-(c->querybuf+pos+1),&ll);
            if (!ok || ll < 0 || ll > 512*1024*1024) {
                redisPanic("Protocol error: invalid bulk length");
                return REDIS_ERR;
            }

            // 定位到参数的开头
            // 比如 
            // $3\r\nSET\r\n...
            //       ^
            //       |
            //      pos
            pos += newline-(c->querybuf+pos)+2;
            
            // 参数的长度
            c->bulklen = ll;
        }

        /* Read bulk argument */
        // 读入参数
        if (sdslen(c->querybuf)-pos < (unsigned)(c->bulklen+2)) {
            // 确保内容符合协议格式
            // 比如 $3\r\nSET\r\n 就检查 SET 之后的 \r\n
            /* Not enough data (+2 == trailing \r\n) */
            break;
        } else {
            // 为参数创建字符串对象  
            /* Optimization: if the buffer contains JUST our bulk element
             * instead of creating a new object by *copying* the sds we
             * just use the current sds string. */
            
            c->argv[c->argc++] =
                createStringObject(c->querybuf+pos,c->bulklen);
            pos += c->bulklen+2;
            

            // 清空参数长度
            c->bulklen = -1;

            // 减少还需读入的参数个数
            c->multibulklen--;
        }
    }

    /* Trim to pos */
    // 从 querybuf 中删除已被读取的内容
    if (pos) sdsrange(c->querybuf,pos,-1);//这个函数应该会修改c->querybuf的值

    /* We're done when c->multibulk == 0 */
    // 如果本条命令的所有参数都已读取完，那么返回
    if (c->multibulklen == 0) return REDIS_OK;

    /* Still not read to process the command */
    // 如果还有参数未读取完，那么就协议内容有错
    return REDIS_ERR;
}


// 处理客户端输入的命令内容
void processInputBuffer(redisClient *c) {

    /* Keep processing while there is something in the input buffer */
    // 尽可能地处理查询缓冲区中的内容
    // 如果读取出现 short read ，那么可能会有内容滞留在读取缓冲区里面
    // 这些滞留内容也许不能完整构成一个符合协议的命令，
    // 需要等待下次读事件的就绪
    while(sdslen(c->querybuf)) {
        /* Determine request type when unknown. */
        // 判断请求的类型
        // 两种类型的区别可以在 Redis 的通讯协议上查到：
        // http://redis.readthedocs.org/en/latest/topic/protocol.html
        // 简单来说，多条查询是一般客户端发送来的，
        // 而内联查询则是 TELNET 发送来的
        if (!c->reqtype) {
            if (c->querybuf[0] == '*') {
                // 多条查询
                c->reqtype = REDIS_REQ_MULTIBULK;
            } else {
                // 内联查询
                c->reqtype = REDIS_REQ_INLINE;
            }
        }

        // 将缓冲区中的内容转换成命令，以及命令参数
        if (c->reqtype == REDIS_REQ_INLINE) {
            if (processInlineBuffer(c) != REDIS_OK) break;
        } else if (c->reqtype == REDIS_REQ_MULTIBULK) {
            if (processMultibulkBuffer(c) != REDIS_OK) break;
        } else {
            redisPanic("Unknown request type");
        }

        /* Multibulk processing could see a <= 0 length. */
        if (c->argc == 0) {
            resetClient(c);
        } else {
            /* Only reset the client when the command was executed. */
            // 执行命令，并重置客户端
            if (processCommand(c) == REDIS_OK)
                resetClient(c);
        }
    }
}




/* resetClient prepare the client to process the next command */
// 在客户端执行完命令之后执行：重置客户端以准备执行下个命令
void resetClient(redisClient *c) {
    freeClientArgv(c);
    c->reqtype = 0;
    c->multibulklen = 0;
    c->bulklen = -1;
}

/*
 * 负责传送命令回复的写处理器
 */
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    redisClient *c = privdata;
    int nwritten = 0, objlen;
    size_t objmem;
    robj *o;
    REDIS_NOTUSED(el);
    REDIS_NOTUSED(mask);

    // 一直循环，直到回复缓冲区为空
    // 或者指定条件满足为止
    while(c->bufpos > 0) {
        // c->bufpos > 0

        // 写入内容到套接字
        // c->sentlen 是用来处理 short write 的
        // 当出现 short write ，导致写入未能一次完成时，
        // c->buf+c->sentlen 就会偏移到正确（未写入）内容的位置上。
        nwritten = write(fd,c->buf+c->sentlen,c->bufpos-c->sentlen);
        // 出错则跳出
        if (nwritten <= 0) break;
        // 成功写入则更新写入计数器变量
        c->sentlen += nwritten;

        /* If the buffer was sent, set bufpos to zero to continue with
            * the remainder of the reply. */
        // 如果缓冲区中的内容已经全部写入完毕
        // 那么清空客户端的两个计数器变量
        if (c->sentlen == c->bufpos) {
            c->bufpos = 0;
            c->sentlen = 0;
        }
    }

    // 写入出错检查
    if (nwritten == -1) {
        if (errno == EAGAIN) {
            nwritten = 0;
        } else {
            redisLog(REDIS_VERBOSE,
                "Error writing to client: %s", strerror(errno));
            freeClient(c);
            return;
        }
    }

    
    if (c->bufpos == 0) {
        c->sentlen = 0;

        // 删除 write handler
        aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
    }
}


int prepareClientToWrite(redisClient *c) {
    // 一般情况，为客户端套接字安装写处理器到事件循环
    if (c->bufpos == 0 && aeCreateFileEvent(server.el, c->fd, AE_WRITABLE,
        sendReplyToClient, c) == AE_ERR) 
        return REDIS_ERR;

    return REDIS_OK;
}

/*
 * 尝试将回复添加到 c->buf 中
 */
int _addReplyToBuffer(redisClient *c, char *s, size_t len) {
    size_t available = sizeof(c->buf)-c->bufpos;

    /* Check that the buffer has enough space available for this string. */
    // 空间必须满足
    if (len > available) return REDIS_ERR;

    // 复制内容到 c->buf 里面
    memcpy(c->buf+c->bufpos,s,len);
    c->bufpos+=len;

    return REDIS_OK;
}


void addReply(redisClient *c, robj *obj) {
    // 为客户端安装写处理器到事件循环
    if (prepareClientToWrite(c) != REDIS_OK) return;

    if (sdsEncodedObject(obj)) {
        // 首先尝试复制内容到 c->buf 中，这样可以避免内存分配
        if (_addReplyToBuffer(c,obj->ptr,sdslen(obj->ptr)) != REDIS_OK)
            // 如果 c->buf 中的空间不够，就复制到 c->reply 链表中
            // 可能会引起内存分配
            // _addReplyObjectToList(c,obj);
            redisPanic("addReply() : replay too large");
    } else {
         redisPanic("Wrong obj->encoding in addReply()");
    }
}

