#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include "include/fcgi_stdio.h"
#include <gd.h>

extern "C"{
#include <libtaomee/conf_parser/config.h>
#include <fdfs_client.h>
#include <logger.h>
}

#include "proto.h"
#include "log.h"
#include "common.h"
#define MAX_URL_LEN 128
enum {
    DEL_HANDLE_OK = 0,
    DEL_HANDLE_INVALID_PARA,
    DEL_HANDLE_SOURCE_ERR,
    DEL_HANDLE_NAIL_ERR,
    DEL_HANDLE_SQUARE_ERR,
    DEL_HANDLE_UNALLOWED_METHOD,
    DEL_HANDLE_304_STATS,
    DEL_HANDLE_FDFS_ERR
};
const static char *descript[] = {
    "success",
    "invalid param",
    "source picture not found",
    "nail picture not found",
    "square picture not found",
    "invalid http method",
    "304",
    "fastdfs internal err"
};
int g_errorid;
char g_filename[MAX_URL_LEN];
char g_nail_filename[MAX_URL_LEN];
char g_square_filename[MAX_URL_LEN];
void print_result()
{
    printf("Content-type: text/plain\r\n\r\n"); 
    printf("\{\"result\":\"%s\",\r\n", descript[g_errorid]);
    printf("\"lloc\":\"%s\",\r\n", g_filename);
    printf("\"nail_lloc\":\"%s\",\r\n", g_nail_filename);
    printf("\"square_lloc\":\"%s\"}\r\n", g_square_filename);
}
int fdfs_init()
{
    /* fdfs log init*/
    log_init_fdfsx();
    g_log_context.log_level = LOG_ERR;

    if (fdfs_client_init(config_get_strval("fdfs_client")) != 0) {
        CGI_ERROR_LOG("fdfs_client_init client.conf");
        return FAIL;
    }

    return SUCC;
}

void fdfs_fini()
{
    tracker_close_all_connections();
    fdfs_client_destroy();
    log_destroy();
}

int fdfs_delete_file()
{
    int result;
    //Constructor source picture url & thumbsquare url
    const char *p = nullptr;
    char *prefix = config_get_strval("thumb_square_size");
    memset(g_filename, 0, sizeof(g_filename));
    memset(g_square_filename, 0, sizeof(g_square_filename));
    
    ConnectionInfo *p_tracker_server = tracker_get_connection();
    if (p_tracker_server == NULL) {
        g_errorid = DEL_HANDLE_FDFS_ERR;
        CGI_ERROR_LOG("tracker_get_connection fail");
        return FAIL;
    }
    
    result = storage_delete_file1(p_tracker_server, NULL, g_nail_filename);
    if (result != 0) {
        g_errorid = DEL_HANDLE_NAIL_ERR;
        CGI_ERROR_LOG("storage_delete_file1 nail file %s fail: errorno[%d] %s", \
                      g_nail_filename, result, STRERROR(result));
        return FAIL;
    }

    if (nullptr == (p = strrchr(g_nail_filename, '_'))) {
        CGI_ERROR_LOG("storage_delete_file failed, can not parse picture url <%s> split by _", g_nail_filename);
        //g_errorid = DEL_HANDLE_INVALID_PARA;
        return SUCC;
    }
    int len = p - g_nail_filename;
    memcpy(g_filename, g_nail_filename, len);
    memcpy(g_square_filename, g_nail_filename, len);
    if (nullptr == (p = strchr(p, '.'))) {
        g_errorid = DEL_HANDLE_INVALID_PARA;
        CGI_ERROR_LOG("storage_delete_file failed, can not parse picture url split by .");
        return FAIL;
    }
    memcpy(g_filename+len, p, strlen(p));

    result = storage_delete_file1(p_tracker_server, NULL, g_filename);
    if (result != 0) {
        CGI_ERROR_LOG("storage_delete_file1 delete source url%s fail: errorno[%d] %s", \
                      g_filename, result, STRERROR(result));
        g_errorid = DEL_HANDLE_SOURCE_ERR;
        return FAIL;
    }

    if (nullptr != prefix) {
        memcpy(g_square_filename+len, prefix, strlen(prefix));
        memcpy(g_square_filename+len+strlen(prefix), p, strlen(p));
        result = storage_delete_file1(p_tracker_server, NULL, g_square_filename);
        if (result != 0) {
            g_errorid = DEL_HANDLE_SQUARE_ERR;
            CGI_ERROR_LOG("storage_delete_file1 delete square url%s fail: errorno[%d] %s", \
                          g_square_filename, result, STRERROR(result));
            return FAIL;
        }

    }
#ifdef DEBUG
    CGI_DEBUG_LOG("[%d]Deleteing picture %s ...", __LINE__, g_nail_filename);
#endif
    return SUCC;
}

int main(void)
{
    /* load config file */
    if (FAIL == config_init("../etc/bench.conf")) {
        CGI_ERROR_LOG("read conf_file error");
        return FAIL;
    }
    
    /* my log init*/
    //cgi_log_init("../log/", (log_lvl_t)8, LOG_SIZE, 0, "");
    cgi_log_init(config_get_strval("log_path"), (log_lvl_t)config_get_intval("log_level", 8), LOG_SIZE, 0, "");
    
    if (FAIL == fdfs_init()) {
        CGI_ERROR_LOG("fdfs_init fail");
        return FAIL;
    }

#ifdef DEBUG
    int fcgi_request_cnt = 0;

    CGI_DEBUG_LOG("fastcgi begin");
#endif
    while (FCGI_Accept() >= 0) {	
#ifdef DEBUG
        fcgi_request_cnt++;
        CGI_DEBUG_LOG("----Request number %d running on host %s Process ID: %d", \
                      fcgi_request_cnt, getenv("SERVER_NAME"), getpid());
#endif
        g_errorid = DEL_HANDLE_OK;
        /* check request method */
        char *request_method = getenv("REQUEST_METHOD");
        if (NULL == request_method || (strncmp(request_method, "GET", 3) != 0)) {
            CGI_ERROR_LOG("Request method error.[%s]",request_method);
            g_errorid = DEL_HANDLE_UNALLOWED_METHOD;
            print_result();
            FCGI_Finish();
            continue;
        }

        /* check url */
        char *request_url = getenv("QUERY_STRING");
        if (NULL == request_url) {
            CGI_ERROR_LOG("request url NULL");
            g_errorid = DEL_HANDLE_INVALID_PARA;
            print_result();
            FCGI_Finish();
            continue;
        }

        /* return 304 not modified 
        */
        char *if_modified_since = getenv("HTTP_IF_MODIFIED_SINCE");
        if (NULL != if_modified_since) {
            CGI_DEBUG_LOG("return 304 status");
            g_errorid = DEL_HANDLE_304_STATS;
            print_result();
            FCGI_Finish();
            continue;
        }
        /* delete file from fdfs */
        memset(g_nail_filename, 0, sizeof(g_nail_filename));
        memcpy(g_nail_filename, request_url, strlen(request_url));
        if (FAIL == fdfs_delete_file()) {
            CGI_ERROR_LOG("fdfs_delete_file fail");
            print_result();
            FCGI_Finish();
            continue;
        }
        print_result();
        FCGI_Finish();
    }

    fdfs_fini();
    return SUCC;
}

