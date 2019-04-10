#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

extern "C"{
#include "log.h"
}

#include "common.h"

#include "tcpip.h"

CTcp::CTcp(const char *ipport, uint32_t send_recv_timeout, uint32_t reconnect_interval)
{
    m_fd = -1;
    m_send_recv_timeout = send_recv_timeout;
    m_reconnect_interval = reconnect_interval;
    m_last_fail_connect_time = 0;

    const char *p = ipport; 
    while (*p != '\0' && *p != ':')
        ++p;
 
    if (*p == '\0') {
        strcpy(m_ip, "");
        m_port = 0;
        return;
    }

    uint32_t len = p - ipport;
    if (len > sizeof(m_ip) - 1) {
        strcpy(m_ip, "");
        m_port = 0;
        return;
    }  

    strncpy(m_ip, ipport, len);
    m_ip[len] = '\0';
    m_port = atol(p + 1);
    reconnect();
}

CTcp::~CTcp()
{
    if (m_fd != -1)
  		close(m_fd);
}

void CTcp::reconnect()
{
    uint32_t now = time(NULL);
    if (m_fd != -1) {
        close(m_fd);
        m_fd = -1;
    }

    if (now - m_last_fail_connect_time < m_reconnect_interval) {
        //小于失败时间 间隔，直接返回
        return;
    }

    struct sockaddr_in peer;
    bzero(&peer, sizeof (peer));
    peer.sin_family  = AF_INET;
    peer.sin_port    = htons(m_port);
    if (inet_pton(AF_INET, m_ip, &peer.sin_addr) <= 0)
        return;

    m_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == connect(m_fd, (const struct sockaddr *)&peer, sizeof(peer))) {
        CGI_DEBUG_LOG("err: connect %s:%u errno:%s", m_ip, m_port, strerror(errno));    
        m_last_fail_connect_time = time(NULL);
        close(m_fd);
        m_fd = -1;
		return;
    }

    //设置接收发送超时
	struct timeval tv = { m_send_recv_timeout, 0 };
	if (setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) != 0
		|| setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv)) != 0) {
		close(m_fd);
        m_fd = -1;
		return;
	}

    return;
}


int CTcp::do_net_io(const char *sndbuf, const int sndlen, char **rcvbuf, int *rcvlen)
{
    if (m_fd == -1) {
        reconnect();
        if (m_fd == -1)
            return FAIL;
    }

	int cur_len;
 	//log send data 
	for (int send_bytes = 0; send_bytes < sndlen; send_bytes += cur_len) {
		cur_len = send(m_fd, sndbuf + send_bytes, sndlen - send_bytes, 0);
		if (cur_len == 0) {
            close(m_fd);
            m_fd = -1;
            return FAIL;
		} else if (cur_len == -1) {
			if (errno == EINTR)
                cur_len = 0;
			else {
                close(m_fd);
                m_fd = -1;
                return FAIL;
            }
		}
	}

    int recv_bytes;
	recv_bytes = recv(m_fd, (void *)rcvlen, 4, 0);
	if (recv_bytes != 4) {
        close(m_fd);
        m_fd = -1;
        return FAIL;
    }

	if (*rcvlen > (1 << 22)) {
        close(m_fd);
        m_fd = -1;
        return FAIL;
    }
  	
	if (!(*rcvbuf = (char *) malloc(*rcvlen))) {
        close(m_fd);
        m_fd = -1;
        return FAIL;
	}

    *((int*)(*rcvbuf)) = *rcvlen;
	
    for (recv_bytes = 4; recv_bytes < *rcvlen; recv_bytes += cur_len) {
        cur_len = recv(m_fd, *rcvbuf  + recv_bytes, *rcvlen - recv_bytes, 0);
        if (cur_len == 0) {
            free(*rcvbuf);
            *rcvbuf = NULL;
            close(m_fd);
            m_fd = -1;
            return FAIL;
        } else if (cur_len == -1) {
            if (errno == EINTR) {
                cur_len = 0;
            } else {
                free(*rcvbuf);
                *rcvbuf = NULL;
                close(m_fd);
                m_fd = -1;
                return FAIL;
            }
        }
    }

    return SUCC;
}

const char *CTcp::get_ip()
{
    return m_ip;
}

uint16_t CTcp::get_port()
{
    return m_port;
}

bool CTcp::is_connect()
{
    return m_fd != -1 ? true : false;
}



