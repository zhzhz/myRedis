#include "redis.h"

void setKey(redisDb *db, robj *key, robj *val) {
    //添加或覆写数据库中的键值对
    if (lookupKeyWrite(db,key) == NULL) {
        dbAdd(db,key,val);
    } else {
        dbOverwrite(db,key,val);
    }
}

void dbAdd(redisDb *db, robj *key, robj *val) {
    printf("dbAdd()\n");

    // 复制键名
    sds copy = sdsdup(key->ptr);

    // 尝试添加键值对
    int retval = dictAdd(db->dict, copy, val);

    // 如果键已经存在，那么停止
    redisAssertWithInfo(NULL,key,retval == REDIS_OK);
}

void dbOverwrite(redisDb *db, robj *key, robj *val) {
    dictEntry *de = dictFind(db->dict,key->ptr);
    
    // 节点必须存在，否则中止
    redisAssertWithInfo(NULL,key,de != NULL);

    // 覆写旧值
    dictReplace(db->dict, key->ptr, val);
}

robj *lookupKeyWrite(redisDb *db, robj *key) {
    // 查找并返回 key 的值对象
    return lookupKey(db,key);
}

/*
 * 从数据库 db 中取出键 key 的值（对象）
 *
 * 如果 key 的值存在，那么返回该值；否则，返回 NULL 。
 */
robj *lookupKey(redisDb *db, robj *key) {

    // 查找键空间
    dictEntry *de = dictFind(db->dict,key->ptr);

    // 节点存在
    if (de) {
        // 取出值
        robj *val = dictGetVal(de);

        // 返回值
        return val;
    } else {

        // 节点不存在
        return NULL;
    }
}

/*
 * 将客户端的目标数据库切换为 id 所指定的数据库
 */
int selectDb(redisClient *c, int id) {

    // 确保 id 在正确范围内
    if (id < 0 || id >= server.dbnum)
        return REDIS_ERR;

    // 切换数据库（更新指针）
    c->db = &server.db[id];

    return REDIS_OK;
}

/*
 * 为执行读取操作而从数据库中查找返回 key 的值。
 *
 * 如果 key 存在，那么返回 key 的值对象。
 *
 * 如果 key 不存在，返回 NULL 。
 */
robj *lookupKeyRead(redisClient *c, robj *key) {

    // 查找
    robj *o = lookupKeyRead_(c->db, key);

    return o;
}

/*
 * 为执行读取操作而取出键 key 在数据库 db 中的值。
 *
 *
 * 找到时返回值对象，没找到返回 NULL 。
 */
robj *lookupKeyRead_(redisDb *db, robj *key) {
    robj *val;

    // 从数据库中取出键的值
    val = lookupKey(db,key);

    // 返回值
    return val;
}
