#include "redis.h"
#include "tmp.h"

/* Global vars */
struct redisServer server; /* server global state */

/* Db->dict, keys are sds strings, vals are Redis objects. */
dictType dbDictType = {
    dictSdsHash,                /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictSdsKeyCompare,          /* key compare */
    dictSdsDestructor,          /* key destructor */
    dictRedisObjectDestructor   /* val destructor */
};

void initServer() {
	int j;

	server.db = zmalloc(sizeof(redisDb)*server.dbnum);

	// 创建并初始化数据库结构
    for (j = 0; j < server.dbnum; j++) {
		server.db[j].id = j;
		server.db[j].dict = dictCreate(&dbDictType, NULL);
	}

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

void initServerConfig()
{
	server.dbnum = REDIS_DEFAULT_DBNUM;
	server.verbosity = REDIS_DEFAULT_VERBOSITY;

	server.port = REDIS_SERVERPORT;
    server.maxclients = REDIS_MAX_CLIENTS;
}


int main(void)
{
	initServerConfig();
	initServer();
    
    aeMain(server.el);
	return 0;
}
