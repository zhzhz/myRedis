#ifndef __TMP_H
#define __TMP_H

#include "redis.h"

void addCmdSetParamTmp(redisClient *c, char *key, char *value);
void addCmdGetParamTmp(redisClient *c, char *key);


#endif