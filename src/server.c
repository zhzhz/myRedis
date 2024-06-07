#include "redis.h"
#include "tmp.h"

/* Global vars */
struct redisServer server; /* server global state */

struct sharedObjectsStruct shared;

/* Db->dict, keys are sds strings, vals are Redis objects. */
dictType dbDictType = {
    dictSdsHash,                /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictSdsKeyCompare,          /* key compare */
    dictSdsDestructor,          /* key destructor */
    dictRedisObjectDestructor   /* val destructor */
};

/* Command table. sds string -> command struct pointer. */
dictType commandTableDictType = {
    dictSdsCaseHash,           /* hash function */
    NULL,                      /* key dup */
    NULL,                      /* val dup */
    dictSdsKeyCaseCompare,     /* key compare */
    dictSdsDestructor,         /* key destructor */
    NULL                       /* val destructor */
};

/* =========================== Server initialization ======================== */
void createSharedObjects(void) {
    // 常用回复
    shared.crlf = createObject(REDIS_STRING,sdsnew("\r\n"));
    shared.ok = createObject(REDIS_STRING,sdsnew("+OK\r\n"));
    shared.err = createObject(REDIS_STRING,sdsnew("-ERR\r\n"));
    

    // 常用长度 bulk 或者 multi bulk 回复
    for (int j = 0; j < REDIS_SHARED_BULKHDR_LEN; j++) {
        shared.bulkhdr[j] = createObject(REDIS_STRING,
            sdscatprintf(sdsempty(),"$%d\r\n",j));
    }
}


void initServer() {
	int j;

	server.db = zmalloc(sizeof(redisDb)*server.dbnum);

	// 创建并初始化数据库结构
    for (j = 0; j < server.dbnum; j++) {
		server.db[j].id = j;
		server.db[j].dict = dictCreate(&dbDictType, NULL);
	}

    // 创建共享对象
    createSharedObjects();

    server.el = aeCreateEventLoop(server.maxclients+REDIS_EVENTLOOP_FDSET_INCR);

	// 打开 TCP 监听端口，用于等待客户端的命令请求
    if (server.port != 0 &&
        listenToPort(server.port,server.ipfd,&server.ipfd_count) == REDIS_ERR)
        exit(1);


	// 为 TCP 连接关联连接应答（accept）处理器
    // 用于接受并应答客户端的 connect() 调用
    for (j = 0; j < server.ipfd_count; j++) {
        if (aeCreateFileEvent(server.el, server.ipfd[j], AE_READABLE,
            acceptTcpHandler,NULL) == AE_ERR)
            {
                redisPanic(
                    "Unrecoverable error creating server.ipfd file event.");
            }
    }
}

struct redisCommand redisCommandTable[] = {
    {"get",getCommand,2},
    {"set",setCommand,-3},
};

/* Populates the Redis Command Table starting from the hard coded list
 * we have on top of redis.c file. 
 *
 * 根据 redis.c 文件顶部的命令列表，创建命令表
 */
void populateCommandTable(void) {
    int j;

    // 命令的数量
    int numcommands = sizeof(redisCommandTable)/sizeof(struct redisCommand);

    for (j = 0; j < numcommands; j++) {
        
        // 指定命令
        struct redisCommand *c = redisCommandTable+j;

        int retval1;

        // 将命令关联到命令表
        retval1 = dictAdd(server.commands, sdsnew(c->name), c);

        redisAssert(retval1 == DICT_OK);
    }
}

void initServerConfig()
{
	server.dbnum = REDIS_DEFAULT_DBNUM;
	server.verbosity = REDIS_DEFAULT_VERBOSITY;

	server.port = REDIS_SERVERPORT;
    server.maxclients = REDIS_MAX_CLIENTS;

    // 初始化命令表
    // 在这里初始化是因为接下来读取 .conf 文件时可能会用到这些命令
    server.commands = dictCreate(&commandTableDictType,NULL);
    populateCommandTable();
}

/*
 * 获取包含给定键的节点的值
 *
 * 如果节点不为空，返回节点的值
 * 否则返回 NULL
 *
 * T = O(1)
 */
void *dictFetchValue(dict *d, const void *key) {
    dictEntry *he;

    // T = O(1)
    he = dictFind(d,key);

    return he ? dictGetVal(he) : NULL;
}

/*
 * 根据给定命令名字（SDS），查找命令
 */
struct redisCommand *lookupCommand(sds name) {
    return dictFetchValue(server.commands, name);
}

void call(redisClient *c, int flags) {
    REDIS_NOTUSED(flags);

    // 执行实现函数
    c->cmd->proc(c);
}

int processCommand(redisClient *c) {
    // 查找命令，并进行命令合法性检查，以及命令参数个数检查
    c->cmd = lookupCommand(c->argv[0]->ptr);

    if (!c->cmd) {
        // 没找到指定的命令
        redisLog(REDIS_WARNING,"unknown command %s", (char*)c->argv[0]->ptr);
        
        return REDIS_OK;
    } else if ((c->cmd->arity > 0 && c->cmd->arity != c->argc) ||
               (c->argc < -c->cmd->arity)) {
        // 参数个数错误
        redisLog(REDIS_WARNING,"wrong number of arguments for '%s' command", c->cmd->name);

        return REDIS_OK;
    }

    // 执行命令
    call(c,REDIS_CALL_FULL);

    return REDIS_OK;
}


int main(void)
{
	initServerConfig();
	initServer();
    
    aeMain(server.el);
	return 0;
}
