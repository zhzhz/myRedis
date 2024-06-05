#include "redis.h"

void setCommand(redisClient *c) {
    setGenericCommand(c,c->argv[1],c->argv[2]);
}

void setGenericCommand(redisClient *c, robj *key, robj *val) {
    setKey(c->db,key,val);//set key=val in db
}

void getCommand(redisClient *c) {
    getGenericCommand(c);
}

int getGenericCommand(redisClient *c) {
    robj *o;

    // 尝试从数据库中取出键 c->argv[1] 对应的值对象
    // 如果键不存在时，向客户端发送回复信息，并返回 NULL
    if ((o = lookupKeyRead(c,c->argv[1])) == NULL)
    {
        printf("getCmd:no key\n");
        return REDIS_OK;
    }
        

    // 值对象存在，检查它的类型
    if (o->type != REDIS_STRING) {
        // 类型错误
        redisPanic("type error");
    } else {
        // 类型正确，向客户端返回对象的值
        printf("getCmd:value is %s\n", (char*)(o->ptr));
        return REDIS_OK;
    }
}



