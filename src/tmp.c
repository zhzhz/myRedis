#include "tmp.h"


void addCmdSetParamTmp(redisClient *c, char *key, char *value)
{
    int multibulklen = 3;
    c->argv = zmalloc(sizeof(robj*)*multibulklen);

    
    c->argv[1] = createStringObject(key, strlen(key));

    
    c->argv[2] = createStringObject(value, strlen(value));
}


void addCmdGetParamTmp(redisClient *c, char *key)
{
    int multibulklen = 2;
    c->argv = zmalloc(sizeof(robj*)*multibulklen);

    c->argv[1] = createStringObject(key, strlen(key));
}