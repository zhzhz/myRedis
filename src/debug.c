#include "redis.h"
#include "stdarg.h"
#include "syslog.h"

void _redisAssertWithInfo(redisClient *c, robj *o, char *estr, char *file, int line) {
    _redisAssert(estr,file,line);
}

void _redisAssert(char *estr, char *file, int line) {
    redisLog(REDIS_WARNING,"=== ASSERTION FAILED ===");
    redisLog(REDIS_WARNING,"==> %s:%d '%s' is not true",file,line,estr);

    *((char*)-1) = 'x';
}

void _redisPanic(char *msg, char *file, int line) {
    redisLog(REDIS_WARNING,"------------------------------------------------");
    redisLog(REDIS_WARNING,"Guru Meditation: %s #%s:%d",msg,file,line);
    *((char*)-1) = 'x';
}

void redisLog(int level, const char *fmt, ...) {
    va_list ap;
    char msg[REDIS_MAX_LOGMSG_LEN];

    if ((level&0xff) < server.verbosity) return;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    redisLogRaw(level,msg);
}

void redisLogRaw(int level, const char *msg) {
    const char *syslogLevelMap[] = { "LOG_DEBUG", "LOG_INFO", "LOG_NOTICE", "LOG_WARNING" };

    level &= 0xff; /* clear flags */
    if (level < server.verbosity) return;


    printf("%s:",syslogLevelMap[level]);
    printf("%s\n",msg);
}