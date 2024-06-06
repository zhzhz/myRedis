#include "redis.h"
#include "ae_epoll.c"

/*
 * 根据 mask 参数的值，监听 fd 文件的状态，
 * 当 fd 可用时，执行 proc 函数
 */
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData)
{
    if (fd >= eventLoop->setsize) return AE_ERR;

    // 取出文件事件结构
    aeFileEvent *fe = &eventLoop->events[fd];

    // 监听指定 fd 的指定事件
    if (aeApiAddEvent(eventLoop, fd, mask) == -1)
        return AE_ERR;

    // 设置文件事件类型，以及事件的处理器
    fe->mask |= mask;
    if (mask & AE_READABLE) fe->rfileProc = proc;
    if (mask & AE_WRITABLE) fe->wfileProc = proc;

    // 私有数据
    fe->clientData = clientData;

    return AE_OK;
}



/*
 * 初始化事件处理器状态
 */
aeEventLoop *aeCreateEventLoop(int setsize) {
    aeEventLoop *eventLoop;
    int i;

    // 创建事件状态结构
    if ((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;

    // 初始化文件事件结构和已就绪文件事件结构数组
    eventLoop->events = zmalloc(sizeof(aeFileEvent)*setsize);
    eventLoop->fired = zmalloc(sizeof(aeFiredEvent)*setsize);
    if (eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
    // 设置数组大小
    eventLoop->setsize = setsize;
    
    eventLoop->stop = 0;
    
    if (aeApiCreate(eventLoop) == -1) goto err;

    /* Events with mask == AE_NONE are not set. So let's initialize the
     * vector with it. */
    // 初始化监听事件
    for (i = 0; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;

    // 返回事件循环
    return eventLoop;

err:
    if (eventLoop) {
        zfree(eventLoop->events);
        zfree(eventLoop->fired);
        zfree(eventLoop);
    }
    return NULL;
}

int aeProcessEvents(aeEventLoop *eventLoop, int flags)
{
    int numevents;

    // 处理文件事件，阻塞时间由 tvp 决定
    numevents = aeApiPoll(eventLoop, NULL);

    for (int j = 0; j < numevents; j++) {
        // 从已就绪数组中获取事件
        aeFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];

        int mask = eventLoop->fired[j].mask;
        int fd = eventLoop->fired[j].fd;
        int rfired = 0;

        /* note the fe->mask & mask & ... code: maybe an already processed
            * event removed an element that fired and we still didn't
            * processed, so we check if the event is still valid. */
        // 读事件
        if (fe->mask & mask & AE_READABLE) {
            // rfired 确保读/写事件只能执行其中一个
            rfired = 1;
            fe->rfileProc(eventLoop,fd,fe->clientData,mask);
        }
        // 写事件
        if (fe->mask & mask & AE_WRITABLE) {
            if (!rfired || fe->wfileProc != fe->rfileProc)
                fe->wfileProc(eventLoop,fd,fe->clientData,mask);
        }
    }
}

/*
 * 事件处理器的主循环
 */
void aeMain(aeEventLoop *eventLoop) {

    eventLoop->stop = 0;

    while (!eventLoop->stop) {

        // 如果有需要在事件处理前执行的函数，那么运行它
        // if (eventLoop->beforesleep != NULL)
        //     eventLoop->beforesleep(eventLoop);

        // 开始处理事件
        aeProcessEvents(eventLoop, AE_ALL_EVENTS);
    }
}