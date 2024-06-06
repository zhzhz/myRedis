#include "redis.h"
#include <sys/epoll.h>
#include <sys/time.h>

/*
 * 事件状态
 */
typedef struct aeApiState {

    // epoll_event 实例描述符
    int epfd;

    // 事件槽
    struct epoll_event *events;

} aeApiState;

/*
 * 关联给定事件到 fd
 */
static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee;

    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. 
     *
     * 如果 fd 没有关联任何事件，那么这是一个 ADD 操作。
     *
     * 如果已经关联了某个/某些事件，那么这是一个 MOD 操作。
     */
    int op = eventLoop->events[fd].mask == AE_NONE ?
            EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    // 注册事件到 epoll
    ee.events = 0;
    mask |= eventLoop->events[fd].mask; /* Merge old events */
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;

    if (epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1;

    return 0;
}

/*
 * 创建一个新的 epoll 实例，并将它赋值给 eventLoop
 */
static int aeApiCreate(aeEventLoop *eventLoop) {

    aeApiState *state = zmalloc(sizeof(aeApiState));

    if (!state) return -1;

    // 初始化事件槽空间
    state->events = zmalloc(sizeof(struct epoll_event)*eventLoop->setsize);
    if (!state->events) {
        zfree(state);
        return -1;
    }

    // 创建 epoll 实例
    state->epfd = epoll_create(1024); /* 1024 is just a hint for the kernel */
    if (state->epfd == -1) {
        zfree(state->events);
        zfree(state);
        return -1;
    }

    // 赋值给 eventLoop
    eventLoop->apidata = state;
    return 0;
}

/*
 * 获取可执行事件
 */
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    // 等待时间
    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);

    // 有至少一个事件就绪？
    if (retval > 0) {
        int j;

        // 为已就绪事件设置相应的模式
        // 并加入到 eventLoop 的 fired 数组中
        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j;

            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE;

            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    
    // 返回已就绪事件个数
    return numevents;
}