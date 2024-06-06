#ifndef ANET_H
#define ANET_H

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

int anetNonBlock(char *err, int fd);
int anetTcpAccept(char *err, int s, char *ip, size_t ip_len, int *port);

int anetTcpServer(char *err, int port, char *bindaddr, int backlog);

#endif