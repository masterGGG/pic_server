#ifndef TCPIP_H
#define TCPIP_H

#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define  SYS_ERR 1001
#define  NET_ERR 1003

class CTcp
{
public:
	//"10.1.1.24:21001"
	CTcp(const char *ipport, uint32_t send_recv_timeout = 1, uint32_t reconnect_interval = 10);
	~CTcp();

    void reconnect();
    int do_net_io(const char *sndbuf, const int sndlen, char **rcvbuf, int *rcvlen);
   
    const char *get_ip();
    uint16_t get_port();
    bool is_connect();

private:
    int m_fd;
    char m_ip[16];
    short m_port;
    uint32_t m_send_recv_timeout; //发送接收超时
    uint32_t m_reconnect_interval; //重连时间间隔
    uint32_t m_last_fail_connect_time; //最后一次连接失败的时间
};

#endif 
