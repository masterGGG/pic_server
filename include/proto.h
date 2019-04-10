#ifndef PROTO_H
#define PROTO_H

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>

struct protocol_t {
	uint32_t	len;
	uint16_t	cmd;
	uint32_t	id;
	uint32_t	seq;
	uint32_t	ret;
	uint8_t		body[];
}__attribute__((packed));

#define PROTO_MAX_LEN   3*1024*1024
#define PROTO_H_SIZE    sizeof(protocol_t)

#define UNPKG_UINT(pkg, val, idx) \
    do { \
        switch (sizeof(val)) { \
        case 1: (val) = *(uint8_t*)((pkg) + (idx)); (idx) += 1; break; \
        case 2: (val) = ntohs(*(uint16_t*)((pkg) + (idx))); (idx) += 2; break; \
        case 4: (val) = ntohl(*(uint32_t*)((pkg) + (idx))); (idx) += 4; break; \
        } \
    } while (0)

#define UNPKG_UINT8(b, v, j) \
    do { \
        (v) = *(uint8_t*)((b)+(j)); (j) += 1; \
    } while (0)

#define UNPKG_UINT16(b, v, j) \
    do { \
        (v) = ntohs(*(uint16_t*)((b)+(j))); (j) += 2; \
    } while (0)

#define UNPKG_UINT32(b, v, j) \
    do { \
        (v) = ntohl(*(uint32_t*)((b)+(j))); (j) += 4; \
    } while (0)

#define UNPKG_UINT64(b, v, j) \
    do { \
        (v) = bswap_64(*(uint64_t*)((b)+(j))); (j) += 8; \
    } while (0)

#define UNPKG_STR(b, v, j, l) \
    do { \
        memcpy((v), (b)+(j), (l)); (j) += (l); \
    } while (0)

#define UNPKG_H_UINT32(b, v, j) \
    do { \
        (v) = *(uint32_t*)((b)+(j)); (j) += 4; \
    } while (0)

#define UNPKG_H_UINT16(b, v, j) \
    do { \
        (v) = *(uint16_t*)((b)+(j)); (j) += 2; \
    } while (0)

#define UNPKG_H_UINT8(buf, val, idx) UNPKG_UINT8((buf), (val), (idx))

#define PKG_UINT(pkg, val, idx) \
    do { \
        switch ( sizeof(val) ) { \
        case 1: *(uint8_t*)((pkg) + (idx)) = (val); (idx) += 1; break; \
        case 2: *(uint16_t*)((pkg) + (idx)) = htons(val); (idx) += 2; break; \
        case 4: *(uint32_t*)((pkg) + (idx)) = htonl(val); (idx) += 4; break; \
        } \
    } while (0)

#define PKG_UINT8(b, v, j) \
    do { \
        *(uint8_t*)((b)+(j)) = (v); (j) += 1; \
    } while (0)

#define PKG_UINT16(b, v, j) \
    do { \
        *(uint16_t*)((b)+(j)) = htons(v); (j) += 2; \
    } while (0)

#define PKG_UINT32(b, v, j) \
    do { \
        *(uint32_t*)((b)+(j)) = htonl(v); (j) += 4; \
    } while (0)

#define PKG_UINT64(b, v, j) \
    do { \
        *(uint64_t*)((b)+(j)) = bswap_64((v)); (j) += 8; \
    } while (0)

#define PKG_H_UINT8(b, v, j) \
    do { \
        *(uint8_t*)((b)+(j)) = (v); (j) += 1; \
    } while (0)
		
#define PKG_H_UINT16(b, v, j) \
    do { \
        *(uint16_t*)((b)+(j)) = (v); (j) += 2; \
    } while (0)

#define PKG_H_UINT32(b, v, j) \
    do { \
        *(uint32_t*)((b)+(j)) = (v); (j) += 4; \
    } while (0)

#define PKG_H_UINT64(b, v, j) \
    do { \
        *(uint64_t*)((b)+(j)) = (v); (j) += 8; \
    } while (0)

#define PKG_STR(b, v, j, l) \
    do { \
        memcpy((b)+(j), (v), (l)); (j) += (l); \
    } while (0)

static inline void
init_proto_head(void* buf, uint32_t id, int cmd, int len)
{
	protocol_t* p = (protocol_t*)(buf);
	p->len = len;
	p->cmd = cmd;
	p->id  = id;
	p->ret = 0;
}

#endif
