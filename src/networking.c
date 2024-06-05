#include "redis.h"

/*
 * 创建一个新客户端
 */
redisClient *createClient(int fd) {
    // 分配空间
    redisClient *c = zmalloc(sizeof(redisClient));

    // 默认数据库
    selectDb(c,0);

    return c;
}