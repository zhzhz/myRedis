#ifndef __REDIS_H
#define __REDIS_H

#include <stddef.h>
#include "dict.h"    /* Hash tables */
#include "sds.h"     /* Dynamic safe strings */
#include "zmalloc.h"
#include "unistd.h"
#include "ae.h"
#include "anet.h"
#include <netinet/in.h>
#include "util.h"


#define sdsEncodedObject(objptr) (objptr->encoding == REDIS_ENCODING_RAW || objptr->encoding == REDIS_ENCODING_EMBSTR)

#define REDIS_INLINE_MAX_SIZE   (1024*64) /* Max size of inline reads */
#define REDIS_MBULK_BIG_ARG     (1024*32)

/* Anti-warning macro... */
#define REDIS_NOTUSED(V) ((void) V)

#define REDIS_SERVERPORT        6379    /* TCP port */


#define redisPanic(_e) _redisPanic(#_e,__FILE__,__LINE__),_exit(1)

#define REDIS_SHARED_BULKHDR_LEN 32

/* Log levels */
#define REDIS_DEBUG 0
#define REDIS_VERBOSE 1
#define REDIS_NOTICE 2
#define REDIS_WARNING 3
#define REDIS_LOG_RAW (1<<10) /* Modifier to log without timestamp */
#define REDIS_DEFAULT_VERBOSITY REDIS_NOTICE

#define REDIS_MAX_LOGMSG_LEN    1024 /* Default maximum length of syslog messages */

#define redisAssert(_e) ((_e)?(void)0 : (_redisAssert(#_e,__FILE__,__LINE__),_exit(1)))
#define redisAssertWithInfo(_c,_o,_e) ((_e)?(void)0 : (_redisAssertWithInfo(_c,_o,#_e,__FILE__,__LINE__),_exit(1)))

/* Error codes */
#define REDIS_OK                0
#define REDIS_ERR               -1

#define REDIS_DEFAULT_DBNUM     16

/* Object types */
// 对象类型
#define REDIS_STRING 0
#define REDIS_LIST 1
#define REDIS_SET 2
#define REDIS_ZSET 3
#define REDIS_HASH 4

// 对象编码
#define REDIS_ENCODING_RAW 0     /* Raw representation */
#define REDIS_ENCODING_INT 1     /* Encoded as integer */
#define REDIS_ENCODING_HT 2      /* Encoded as hash table */
#define REDIS_ENCODING_ZIPMAP 3  /* Encoded as zipmap */
#define REDIS_ENCODING_LINKEDLIST 4 /* Encoded as regular linked list */
#define REDIS_ENCODING_ZIPLIST 5 /* Encoded as ziplist */
#define REDIS_ENCODING_INTSET 6  /* Encoded as intset */
#define REDIS_ENCODING_SKIPLIST 7  /* Encoded as skiplist */
#define REDIS_ENCODING_EMBSTR 8  /* Embedded sds string encoding */


#define REDIS_IP_STR_LEN INET6_ADDRSTRLEN

#define REDIS_BINDADDR_MAX 16

#define REDIS_MIN_RESERVED_FDS 32
#define REDIS_EVENTLOOP_FDSET_INCR (REDIS_MIN_RESERVED_FDS+96)

#define REDIS_MAX_CLIENTS 10000

#define REDIS_IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */

/* Client request types */
#define REDIS_REQ_INLINE 1
#define REDIS_REQ_MULTIBULK 2


/* Command call flags, see call() function */
#define REDIS_CALL_NONE 0
#define REDIS_CALL_SLOWLOG 1
#define REDIS_CALL_STATS 2
#define REDIS_CALL_PROPAGATE 4
#define REDIS_CALL_FULL (REDIS_CALL_SLOWLOG | REDIS_CALL_STATS | REDIS_CALL_PROPAGATE)

#define REDIS_REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */

typedef struct redisDb {
    // 数据库键空间，保存着数据库中的所有键值对
    dict *dict;                 /* The keyspace for this DB */

    // 数据库号码
    int id;                     /* Database ID */
} redisDb;

/*
 * Redis 对象
 */
typedef struct redisObject {

    // 类型
    unsigned type:4;

    // 编码
    unsigned encoding:4;

    // 指向实际值的指针
    void *ptr;

    // 引用计数
    int refcount;

} robj;


typedef struct redisClient {
    // 当前正在使用的数据库
    redisDb *db;

    // 参数数量
    int argc;

    // 参数对象数组
    robj **argv;

    // 查询缓冲区
    sds querybuf;

    // 请求的类型：内联命令还是多条命令
    int reqtype;

    // 剩余未读取的命令内容数量
    int multibulklen;       /* number of multi bulk arguments left to read */

    // 命令内容的长度
    long bulklen;           /* length of bulk argument in multi bulk request */

    // 记录被客户端执行的命令
    struct redisCommand *cmd;

    // 套接字描述符
    int fd;

     /* Response buffer */
    // 回复偏移量
    int bufpos;

    // 回复缓冲区
    char buf[REDIS_REPLY_CHUNK_BYTES];

    // 已发送字节，处理 short write 用
    int sentlen;            /* Amount of bytes already sent in the current
                               buffer or object being sent. */
} redisClient;

typedef void redisCommandProc(redisClient *c);

/*
 * Redis 命令
 */
struct redisCommand {

    // 命令名字
    char *name;

    // 实现函数
    redisCommandProc *proc;

    // 参数个数
    int arity;
};

struct redisServer {
    int dbnum;

    // 数据库
    redisDb *db;

    // 日志可见性
    int verbosity;                  /* Loglevel in redis.conf */

    // 事件状态
    aeEventLoop *el;

    // 网络错误
    char neterr[ANET_ERR_LEN];   /* Error buffer for anet.c */

    // TCP 监听端口
    int port;                   /* TCP listening port */

    int tcp_backlog;            /* TCP listen() backlog */

    // 描述符
    int ipfd[REDIS_BINDADDR_MAX]; /* TCP socket file descriptors */
    // 描述符数量
    int ipfd_count;             /* Used slots in ipfd[] */

    /* Limits */
    int maxclients;

    // 命令表（受到 rename 配置选项的作用）
    dict *commands;             /* Command table */
};

// 通过复用来减少内存碎片，以及减少操作耗时的共享对象
struct sharedObjectsStruct {
    robj *crlf, *ok, *err, 
    *bulkhdr[REDIS_SHARED_BULKHDR_LEN];  /* "$<value>\r\n" */;
};

void setGenericCommand(redisClient *c, robj *key, robj *val);
int getGenericCommand(redisClient *c);

robj *lookupKeyWrite(redisDb *db, robj *key);
void dbAdd(redisDb *db, robj *key, robj *val);
void dbOverwrite(redisDb *db, robj *key, robj *val);
robj *lookupKey(redisDb *db, robj *key);



/* db.c -- Keyspace access API */
void setKey(redisDb *db, robj *key, robj *val);

/* Commands prototypes */
void setCommand(redisClient *c);
void getCommand(redisClient *c);

/* networking.c -- Networking and Client related operations */
redisClient *createClient(int fd);

int selectDb(redisClient *c, int id);

unsigned int dictSdsHash(const void *key);

int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2);

robj *createStringObject(char *ptr, size_t len);
robj *createRawStringObject(char *ptr, size_t len);

/*-----------------------------------------------------------------------------
 * Extern declarations
 *----------------------------------------------------------------------------*/

extern struct redisServer server;
extern struct sharedObjectsStruct shared;


/* Debugging stuff */
void _redisAssertWithInfo(redisClient *c, robj *o, char *estr, char *file, int line);
void redisLog(int level, const char *fmt, ...);
void _redisAssert(char *estr, char *file, int line);
void redisLogRaw(int level, const char *msg);
void _redisPanic(char *msg, char *file, int line);

robj *lookupKeyRead(redisClient *c, robj *key);
robj *lookupKeyRead_(redisDb *db, robj *key);
robj *lookupKey(redisDb *db, robj *key);

void dictSdsDestructor(void *privdata, void *val);

void decrRefCount(robj *o);
void incrRefCount(robj *o);
void freeStringObject(robj *o);

void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask);

void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask);

int listenToPort(int port, int *fds, int *count);

aeEventLoop *aeCreateEventLoop(int setsize);
int aeProcessEvents(aeEventLoop *eventLoop, int flags);

void freeClient(redisClient *c);

void processInputBuffer(redisClient *c);

int processCommand(redisClient *c);

void resetClient(redisClient *c);

int processInlineBuffer(redisClient *c);

int processMultibulkBuffer(redisClient *c);

void addReply(redisClient *c, robj *obj);

int prepareClientToWrite(redisClient *c);

robj *createObject(int type, void *ptr);

void addReplyBulk(redisClient *c, robj *obj);
void addReplyBulkLen(redisClient *c, robj *obj);
#endif
