#include <stddef.h>
#include <stdarg.h>

#ifndef __SDS_H
#define __SDS_H

/*
 * 最大预分配长度
 */
#define SDS_MAX_PREALLOC (1024*1024)

/*
 * 类型别名，用于指向 sdshdr 的 buf 属性
 */
typedef char *sds;

/*
 * 保存字符串对象的结构
 */
struct sdshdr {
    
    // buf 中已占用空间的长度
    int len;

    // buf 中剩余可用空间的长度
    int free;

    // 数据空间
    char buf[];
};

static inline size_t sdslen(const sds s) {
    struct sdshdr *sh = (void*)(s-(sizeof(struct sdshdr)));
    return sh->len;
}

/*
 * 返回 sds 可用空间的长度
 *
 * T = O(1)
 */
static inline size_t sdsavail(const sds s) {
    struct sdshdr *sh = (void*)(s-(sizeof(struct sdshdr)));
    return sh->free;
}

sds sdsnewlen(const void *init, size_t initlen);

sds sdsdup(const sds s);

void sdsfree(sds s);

sds sdsempty(void);

void sdsIncrLen(sds s, int incr);

sds sdsMakeRoomFor(sds s, size_t addlen);

sds sdsnew(const char *init);

sds sdscatprintf(sds s, const char *fmt, ...);
sds sdscatvprintf(sds s, const char *fmt, va_list ap);
sds sdscat(sds s, const char *t);

sds sdscatlen(sds s, const void *t, size_t len);

void sdsrange(sds s, int start, int end);
#endif
