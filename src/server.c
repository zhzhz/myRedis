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
}

void initServerConfig()
{
	server.dbnum = REDIS_DEFAULT_DBNUM;
	server.verbosity = REDIS_DEFAULT_VERBOSITY;
}


int main(void)
{
	initServerConfig();
	initServer();

	redisClient *c = createClient(-1);

	//get x
	addCmdGetParamTmp(c, "x");
	getCommand(c);

	//set x foo
	addCmdSetParamTmp(c, "x", "foo");
	setCommand(c);

	//get x
	addCmdGetParamTmp(c, "x");
	getCommand(c);

	//set x bar
	addCmdSetParamTmp(c, "x", "bar");
	setCommand(c);

	//get x
	addCmdGetParamTmp(c, "x");
	getCommand(c);

	return 0;
}
