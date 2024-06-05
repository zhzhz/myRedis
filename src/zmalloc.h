#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef __ZMALLOC_H
#define __ZMALLOC_H

void *zmalloc(size_t size);
void *zcalloc(size_t size);

void zfree(void *ptr);

#endif /* __ZMALLOC_H */