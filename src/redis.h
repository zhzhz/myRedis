#ifndef __REDIS_H
#define __REDIS_H

#include <stddef.h>
#include "dict.h"    /* Hash tables */
#include "sds.h"     /* Dynamic safe strings */
#include "zmalloc.h"
#include "unistd.h"

#define redisPanic(_e) _redisPanic(#_e,__FILE__,__LINE__),_exit(1)

/* Log levels */
#define REDIS_DEBUG 0
#define REDIS_VERBOSE 1
#define REDIS_NOTICE 2
#define REDIS_WARNING 3
#define REDIS_LOG_RAW (1<<10) /* Modifier to log without timestamp */
#define REDIS_DEFAULT_VERBOSITY REDIS_NOTICE

#define REDIS_MAX_LOGMSG_LEN    1024 /* Default maximum length of syslog messages */

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

} robj;


typedef struct redisClient {
    // 当前正在使用的数据库
    redisDb *db;

    // 参数对象数组
    robj **argv;
} redisClient;

struct redisServer {
    int dbnum;

    // 数据库
    redisDb *db;

    // 日志可见性
    int verbosity;                  /* Loglevel in redis.conf */
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
void freeStringObject(robj *o);

#endif
