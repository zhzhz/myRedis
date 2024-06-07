#include "redis.h"

void setCommand(redisClient *c) {
    setGenericCommand(c,c->argv[1],c->argv[2]);
}

void setGenericCommand(redisClient *c, robj *key, robj *val) {
    setKey(c->db,key,val);//set key=val in db

    // 设置成功，向客户端发送回复
    // 回复的内容由 ok_reply 决定
    
    addReply(c, shared.ok);
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
        redisPanic("getCmd:no key\n");
        return REDIS_OK;
    }
        

    // 值对象存在，检查它的类型
    if (o->type != REDIS_STRING) {
        // 类型错误
        redisPanic("get type error");
    } else {
        // 类型正确，向客户端返回对象的值
        addReplyBulk(c,o);

        return REDIS_OK;
    }
}

/* Add a Redis Object as a bulk reply 
 *
 * 返回一个 Redis 对象作为回复
 */
void addReplyBulk(redisClient *c, robj *obj) {
    addReplyBulkLen(c,obj);
    addReply(c,obj);
    addReply(c,shared.crlf);
}

/* Create the length prefix of a bulk reply, example: $2234 */
void addReplyBulkLen(redisClient *c, robj *obj) {
    size_t len;

    if (sdsEncodedObject(obj)) {
        len = sdslen(obj->ptr);
    } else {
        long n = (long)obj->ptr;

        /* Compute how many bytes will take this integer as a radix 10 string */
        len = 1;
        if (n < 0) {
            len++;
            n = -n;
        }
        while((n = n/10) != 0) {
            len++;
        }
    }

    if (len < REDIS_SHARED_BULKHDR_LEN)
        addReply(c,shared.bulkhdr[len]);
    else
        // addReplyLongLongWithPrefix(c,len,'$');
        redisPanic("addReplyBulkLen() error");
}



