#include "redis.h"

/*
 * 创建一个新 robj 对象
 */
robj *createObject(int type, void *ptr) {
    robj *o = zmalloc(sizeof(*o));

    o->type = type;
    o->encoding = REDIS_ENCODING_RAW;
    o->ptr = ptr;
    o->refcount = 1;

    return o;
}

robj *createRawStringObject(char *ptr, size_t len) {
    return createObject(REDIS_STRING,sdsnewlen(ptr,len));
}

robj *createStringObject(char *ptr, size_t len) {   
    return createRawStringObject(ptr,len);
}

/*
 * 为对象的引用计数增一
 */
void incrRefCount(robj *o) {
    o->refcount++;
}

/*
 * 为对象的引用计数减一
 *
 * 当对象的引用计数降为 0 时，释放对象。
 */
void decrRefCount(robj *o) {
    // redisPanic("decrRefCount against refcount <= 0");
    if (o->refcount <= 0) redisPanic("decrRefCount against refcount <= 0");

    // 释放对象
    if (o->refcount == 1) {
        switch(o->type) {
        case REDIS_STRING: freeStringObject(o); break;
        //case REDIS_LIST: freeListObject(o); break;
        //case REDIS_SET: freeSetObject(o); break;
        //case REDIS_ZSET: freeZsetObject(o); break;
        //case REDIS_HASH: freeHashObject(o); break;
        default: redisPanic("Unknown object type"); break;
        }
        zfree(o);

    // 减少计数
    } else {
        o->refcount--;
    }
}

/*
 * 释放字符串对象
 */
void freeStringObject(robj *o) {
    if (o->encoding == REDIS_ENCODING_RAW) {
        sdsfree(o->ptr);
    }
}


