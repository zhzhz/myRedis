#ifndef __AE_H__
#define __AE_H__


/*
 * 事件执行状态
 */
// 成功
#define AE_OK 0
// 出错
#define AE_ERR -1

/*
 * 文件事件状态
 */
// 未设置
#define AE_NONE 0
// 可读
#define AE_READABLE 1
// 可写
#define AE_WRITABLE 2


/*
 * 时间处理器的执行 flags
 */
// 文件事件
#define AE_FILE_EVENTS 1
// 时间事件
#define AE_TIME_EVENTS 2
// 所有事件
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
// 不阻塞，也不进行等待
#define AE_DONT_WAIT 4


/*
 * 事件处理器状态
 */
struct aeEventLoop;

/* Types and data structures 
 *
 * 事件接口
 */
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);


/* File event structure
 *
 * 文件事件结构
 */
typedef struct aeFileEvent {

    // 监听事件类型掩码，
    // 值可以是 AE_READABLE 或 AE_WRITABLE ，
    // 或者 AE_READABLE | AE_WRITABLE
    int mask; /* one of AE_(READABLE|WRITABLE) */

    // 读事件处理器
    aeFileProc *rfileProc;

    // 写事件处理器
    aeFileProc *wfileProc;

    // 多路复用库的私有数据
    void *clientData;

} aeFileEvent;

/* A fired event
 *
 * 已就绪事件
 */
typedef struct aeFiredEvent {

    // 已就绪文件描述符
    int fd;

    // 事件类型掩码，
    // 值可以是 AE_READABLE 或 AE_WRITABLE
    // 或者是两者的或
    int mask;

} aeFiredEvent;



/* State of an event based program 
 *
 * 事件处理器的状态
 */
typedef struct aeEventLoop {

    // 已注册的文件事件
    aeFileEvent *events; /* Registered events */

    // 已就绪的文件事件
    aeFiredEvent *fired; /* Fired events */


    // 事件处理器的开关
    int stop;

    // 多路复用库的私有数据
    void *apidata; /* This is used for polling API specific data */

    // 在处理事件前要执行的函数
    // aeBeforeSleepProc *beforesleep;

    // 目前已追踪的最大描述符
    int setsize; /* max number of file descriptors tracked */

} aeEventLoop;

int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData);

aeEventLoop *aeCreateEventLoop(int setsize);

void aeMain(aeEventLoop *eventLoop);

void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);

#endif